/*
 * Setup.h
 *
 *  Created on: 7 apr. 2017
 *      Author: michi_000
 */

#ifndef SETUP_H_
#define SETUP_H_

#include <Arduino.h>

#include "target.h"

class Setup {
public:
	Setup();
	void initSetup();
	void readSettingsFromEeprom(void);
	String readStringEeprom(String string, int start, int length);
	int readIntEeprom(int start);
	int saveStringEeprom(String string, int start);
	int saveIntEeprom(int value, int start);
	void commit(void);

	String getHTML();

	const String& getMqttclientid() const {
		return mqttclientid;
	}

	const String& getMqttdevice() const {
		return mqttdevice;
	}

	const String& getMqtthost() const {
		return mqtthost;
	}

	const String& getMqttlocation() const {
		return mqttlocation;
	}

	const String& getMqttpassword() const {
		return mqttpassword;
	}

	const String& getMqttport() const {
		return mqttport;
	}

	const String& getMqttuser() const {
		return mqttuser;
	}

	int getNumleds() const {
		return numleds;
	}

	int getNumoutputs() const {
		return numoutputs;
	}

	void setNumoutputs(int numoutputs = 0) {
		this->numoutputs = numoutputs;
	}

	const String& getOtahost() const {
		return otahost;
	}

	const String& getOtaport() const {
		return otaport;
	}

	const String& getOtaurl() const {
		return otaurl;
	}

	const String& getPasswd() const {
		return passwd;
	}

	const String& getSsid() const {
		return ssid;
	}

private:
	String ssid = "IOT";
	String passwd = "nopass";
	String otahost = "192.168.3.100";
	String otaport = "80";
	String otaurl = "/esp/update/ota.php";
	String mqtthost = "192.168.3.100";
	String mqttport = "1883";
	String mqttlocation = "unknown";
	String mqttdevice = "default";
	String mqttclientid = "";
	String mqttuser = "";
	String mqttpassword = "";

	int numleds = 5;
	int numoutputs = 0;
};

#endif /* SETUP_H_ */
