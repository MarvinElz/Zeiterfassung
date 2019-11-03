#! /bin/bash
echo "Installation der Zeiterfassung"
sudo apt-get install mosquitto
sudo apt-get install mosquitto-dev
sudo apt-get install mosquitto-clients
sudo apt-get install libmosquitto-dev
sudo apt-get install libconfig++-dev
make Arbeitszeiterfassung
echo "Einstellunge können in UID.config gemacht werden"