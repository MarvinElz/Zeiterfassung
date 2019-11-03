#include <iostream>
#include <fstream>

#include <string>
#include <cstring>

#include <unistd.h>

#include <libconfig.h++>

#include <iterator> 
#include <map> 

#include <ctime>

#include <mosquitto.h>

#define mqtt_host "localhost"
#define mqtt_port 1883

using namespace std;
using namespace libconfig;

string log_filename = "log.txt";

map<string, string> users_map;

void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	cout << "Verbindung zum Broker hergestellt." << endl;
}


void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	bool match = 0;
	cout << "Topic: " << message->topic << endl;
	
	//printf("got message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);

	mosquitto_topic_matches_sub("/RFID/UID", message->topic, &match);
	if (match) {
		cout << (char*) message->payload << endl;
		string user_name = users_map[(char*) message->payload];		
		
		std::fstream fs;
		fs.open (log_filename, std::fstream::out | std::fstream::app);
		if (fs.is_open()){
			fs << (char*) message->payload << "," << "\t" << user_name << "," << "\t" << std::time(0) << endl;
			fs.close();
		}
	}

}

int main(){
	cout << "Starte Arbeitszeiterfassung" << endl;
	
	struct mosquitto *mosq;
	int rc = 0;	
	
	Config cfg;
	
	try{
		cfg.readFile( "UID.config" );
	}
	catch( const FileIOException &fioex ){	
		cout << "Fehler beim Lesen des Config-Files \"UID.config\"" << endl;
		return -1;
	}
	catch( const ParseException &pex ){
		cout << "Fehler beim Lesen des Config-Files \"UID.config\"" << endl;
		return -1;
	}

	const Setting& root = cfg.getRoot();
	
	const Setting &users = root["users"];
    const int users_count = users.getLength();  
    
    for(int i = 0; i < users_count; ++i)
	{
		const Setting &user = users[i];
		string UID;	
		string user_name;	
		if(!user.lookupValue("UID", UID) ) continue;
		if(!user.lookupValue("name", user_name)) continue;
		users_map.insert(pair<string, string>(UID, user_name));
	}  
	
	mosquitto_lib_init();
	
	mosq = mosquitto_new("FRID_UID_Listener", true, 0);
	
	if(mosq){
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

	    rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);

		mosquitto_subscribe(mosq, NULL, "/RFID/UID", 0);

		while(1){
			rc = mosquitto_loop(mosq, -1, 1);
			if( rc ){
				printf("connection error!\n");
				sleep(10);
				mosquitto_reconnect(mosq);
			}
		}
		mosquitto_destroy(mosq);
	}

	mosquitto_lib_cleanup();
}
