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

typedef enum{
	PRESENCE_NONE,
	PRESENCE_PRESENT,
	PRESENCE_ABSENT
}Presence;

class User{	
	
public:
	User(string name_, string UID_){
		name = name_;
		UID = UID_;
		presence = PRESENCE_ABSENT;
	}

public:
	void arrived(){
		unix_arrived = std::time(0);
		presence = PRESENCE_PRESENT;
	}
	void left(){
		unix_left = std::time(0);
		presence = PRESENCE_ABSENT;
	}

	std::time_t getArrivalTime(){
		return unix_arrived;
	}

	std::time_t time_spend(){
		if( presence == PRESENCE_PRESENT ) return 0;
		return unix_left - unix_arrived;
	}

	string getName(){
		return name;
	}

	Presence getPresence(){
		return presence;
	}

	private:
		string name;
		string UID;
		std::time_t unix_arrived;
		std::time_t unix_left;
		Presence presence; 		
};


string log_filename = "/home/pi/Zeiterfassung/log.txt";

// Zuordnung zwischen UID und Benutzername
map<string, User*> users_map;



void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	cout << "Verbindung zum Broker hergestellt." << endl;
}


void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	bool match = 0;
	cout << "Topic: " << message->topic << endl;

	mosquitto_topic_matches_sub("/RFID/UID", message->topic, &match);
	if (match) {
		cout << (char*) message->payload << endl;

		if ( users_map.find( (char*) message->payload ) == users_map.end() ){
			User* new_user = new User("Unknown", (char*) message->payload);
			cout << "Neuen unbekannten Benutzer " << "Unknown" << " , " << (char*) message->payload << endl;
			users_map.insert(pair<string, User*>((char*) message->payload, new_user));	
		}
		
		User* user = users_map[(char*) message->payload];	
		if( user->getPresence() == PRESENCE_ABSENT ){
			// Benutzer meldet sich an
			user->arrived();
			cout << "Benutzer " << user->getName() << " hat sich angemeldet." << endl;
			
			char presence = PRESENCE_PRESENT;
			mosquitto_publish(mosq, 0, "/RFID/User_Presence", 1,&presence, 2, false);
			
		}	else {
			user->left();
			cout << "Benutzer " << user->getName() << " hat sich nach " << user->time_spend()  << " Sekunden abgemeldet." << endl;
			
			char presence = PRESENCE_ABSENT;
			mosquitto_publish(mosq, 0, "/RFID/User_Presence", 1 ,&presence, 2, false);
			
			std::fstream fs;
			fs.open (log_filename, std::fstream::out | std::fstream::app);
			if (fs.is_open()){
				fs << (char*) message->payload << "," << "\t" 
					 << user->getName()          << "," << "\t" 
					 << user->getArrivalTime()   << "," << "\t" 
					 << user->time_spend()
					 << endl;
			fs.close();
			}		
		}
	}
}

int main(){
	cout << "Starte Arbeitszeiterfassung" << endl;
	
	struct mosquitto *mosq;
	int rc = 0;	
	
	Config cfg;
	
	try{
		cfg.readFile( "/home/pi/Zeiterfassung/UID.config" );
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
    
		cout << "Folgende Benutzer sind bekannt:" << endl;
    for(int i = 0; i < users_count; ++i)
	{
		const Setting &user = users[i];
		string UID;	
		string user_name;	
		if(!user.lookupValue("UID", UID) ) continue;
		if(!user.lookupValue("name", user_name)) continue;
		User* new_user = new User(user_name, UID);
		cout << "   " << user_name << " , " << UID << endl;
		users_map.insert(pair<string, User*>(UID, new_user));		
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
