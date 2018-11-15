/*
 * Setup.cpp
 *
 *  Created on: 7 apr. 2017
 *      Author: michi_000
 */

#include "Setup.h"
#include <EEPROM.h>
#include <ESP8266WiFi.h>

Setup::Setup() {
	// TODO Auto-generated constructor stub

}

void Setup::initSetup() {
	EEPROM.begin(512);
	readSettingsFromEeprom();
}

void Setup::readSettingsFromEeprom(void) {
	ssid = readStringEeprom(ssid, SSID_EPOS, 32);
	Serial.println("SSID: " + ssid);

	passwd = readStringEeprom(passwd, PWD_EPOS, 32);
	Serial.println("PASS: " + passwd);

	otahost = readStringEeprom(otahost, OTAS_EPOS, 32);
	otaport = readStringEeprom(otaport, OTAP_EPOS, 8);
	otaurl = readStringEeprom(otaurl, OTAU_EPOS, 32);
	Serial.println("OTA SERVER: " + otahost + ":" + otaport + otaurl);

	mqtthost = readStringEeprom(mqtthost, MQTS_EPOS, 32);
	mqttport = readStringEeprom(mqttport, MQTP_EPOS, 8);
	mqttdevice = readStringEeprom(mqttdevice, MQTD_EPOS, 16);
	mqttlocation = readStringEeprom(mqttlocation, MQTL_EPOS, 16);
	mqttclientid = readStringEeprom(WiFi.macAddress(), MQTC_EPOS, 32);
	mqttuser = readStringEeprom(mqttuser, MQTU_EPOS, 16);
	mqttpassword = readStringEeprom(mqttpassword, MQTW_EPOS, 16);
	Serial.println("MQTT SERVER: " + mqtthost + ":" + mqttport + " device: " + mqttdevice + "/" + mqttlocation);
	Serial.println("MQTT Cliet ID: " + mqttclientid);

	String snumleds = String(numleds);
	snumleds = readStringEeprom(snumleds, NUMSTRIP, 8);
	numleds = snumleds.toInt();

	String snumout = String(numoutputs);
	snumout = readStringEeprom(snumout, NUMOUTP, 8);
	numoutputs = snumout.toInt();

}

String Setup::readStringEeprom(String string, int start, int length) {
	String def = string;
	string = "";
	int end = start + length;
	for (int i = start; i < end; ++i) {
		char c = char(EEPROM.read(i));
		if (c == 0)
			break;
		string += c;
	}
	Serial.println("read: " + string + " default: " + def + " from " + start + " length: " + string.length());
	if (string.length() < 1)
		return def;
	return string;
}

int Setup::readIntEeprom(int start) {
	byte high = EEPROM.read(start); //read the first half
	byte low = EEPROM.read(start + 1); //read the second half
	int xVal = (high << 8) + low; //reconstruct the integer
	return xVal;
}

int Setup::saveStringEeprom(String string, int start) {
	int i = 0;
	Serial.println("save " + string + " from " + start);
	if (string.length() > 0) {
		for (i = 0; i < string.length(); ++i) {
			EEPROM.write(i + start, string[i]);
			Serial.print(string[i]);
		}
		Serial.println();
	}
	return i + start;
}

int Setup::saveIntEeprom(int value, int start) {
	int i = 0;
//	Serial.println("save " + string + " from " + start);
	EEPROM.write(start, highByte(value)); //write the first half
	EEPROM.write(start + 1, lowByte(value)); //write the second half
	return start + 2;
}

void Setup::commit(void) {
	EEPROM.commit();
}

String Setup::getHTML() {
	String content = "";
	content += "</td><td><label>password: </label></td><td><input type='text' name='pass' value='" + passwd + "' length=64></td></tr>";
	content += "<tr><td><label>OTA host: </label></td><td><input name='otaserver' value='" + otahost
			+ "' length=32></td><td><label>OTA port: </label></td><td><input name='otaport' value='" + otaport + "' length=8></td></tr>";
	content += "<tr><td><label>OTA url: </label></td><td><input name='otaurl' value='" + otaurl + "' length=32></td></tr>";
	content += "<tr><td><label>MQTT: </label></td><td><input name='mqttserver' value='" + mqtthost
			+ "' length=32></td><td><label>port: </label></td><td><input name='mqttport' value='" + mqttport + "' length=8></td></tr>";
	content += "<tr><td><label>MQTT dev: </label></td><td><input name='mqttdevice' value='" + mqttdevice
			+ "' length=16></td><td><label>MQTT location: </label></td><td><input name='mqttlocation' value='" + mqttlocation
			+ "' length=16></td></tr>";
	content += "<tr><td><label>MQTT clientid: </label></td><td><input name='mqttclid' value='" + mqttclientid + "' length=32></td></tr>";
	content += "<tr><td><label>MQTT user: </label></td><td><input name='mqttuser' value='" + mqttuser
			+ "' length=16></td><td><label>MQTT password: </label></td><td><input name='mqttpwd' value='" + mqttpassword + "' length=16></td></tr>";
	content += "<tr><td><label>#Led Strips: </label></td><td><input name='numleds' value='" + String(numleds) + "' length=8></td></tr>";
	content += "<tr><td><label>#PCF8575: </label></td><td><input name='numoutputs' value='" + String(numoutputs) + "' length=8></td></tr>";
	return content;
}
