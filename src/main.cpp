#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266httpUpdate.h>

#include <MQTTClient.h>
#include <ArduinoJson.h>

//#include <BME280I2C.h>
#include <Wire.h>
#include "BH1750.h"
#include "PCF8575.h"
#include "MCP23017.h"

#include "target.h"
#include "I2Cexp.h"
#include "PWMContr.h"
#include "Setup.h"
#include "PCF8574.h"

#define DEBUG_ESP_HTTP_UPDATE 1

#define SEALEVELPRESSURE_HPA (1013.25)

#define PWMCHAN 16
#define MAXPWM 4095
#define MAXRELAY 16
#define MAXSW 5

String version = "TRAP17_SENSOR";

const char* publisher_json_id = "json";

const char* publisher_switch_id = "switch";
//const char* publisher_motion_id = "motion";

const char* subscriber_led_id = "led";
const char* subscriber_rgb_id = "rgb";
const char* subscriber_hsl_id = "hsl";
const char* subscriber_out_id = "out";
const char* subscriber_setupmode_id = "setup";

struct swdata {
	bool publish = false;
	uint8 state = 0;
};

bool metric = true;

volatile swdata sw[MAXSW];

static uint16_t ioextender;

int lux, oldlux = 0;

WiFiClient net;
MQTTClient client;

//I2C Sensors
I2Cexp i2cexp;
PCF8575 *device;
PCF8574 *exp74;  // add leds to lines      (used as output)
MCP23017 *mcp1;
PWMContr pwmcontr;
Setup iotsetup;

//Config variables
boolean ap_mode = false;
const char* apssid = "SETUP_ESP";
const char* passphrase = "setupesp";
ESP8266WebServer server(80);
String content;
String aplist;
int statusCode;

unsigned long lastMillis = 0;
unsigned long luxLastMillis = 0;

int blinkstate = LOW;

void INT_ReleaseSw0(void);
void INT_ReleaseSw1(void);
void INT_ReleaseSw2(void);
void INT_ReleaseSw3(void);
void INT_ReleaseSw4(void);

void sendData();
void connect(); // <- predefine connect() for setup()
void setupAP(void);
void messageReceived(String &topic, String &payload);

void setup() {
	Serial.begin(115200);

	Serial.println("Booting " + version);
	EEPROM.begin(512);
	delay(10);
	WiFi.mode(WIFI_STA);

	pinMode(LED_PIN, OUTPUT);

	pinMode(SW_PIN, INPUT_PULLUP);
	pinMode(SETUP_PIN, INPUT_PULLUP);
	pinMode(MOTION_PIN1, INPUT_PULLUP);
	pinMode(MOTION_PIN2, INPUT_PULLUP);
	pinMode(MOTION_PIN3, INPUT_PULLUP);

	attachInterrupt(SW_PIN, INT_ReleaseSw0, CHANGE);
	attachInterrupt(SETUP_PIN, INT_ReleaseSw1, CHANGE);
	attachInterrupt(MOTION_PIN1, INT_ReleaseSw2, CHANGE);
	attachInterrupt(MOTION_PIN2, INT_ReleaseSw3, CHANGE);
	attachInterrupt(MOTION_PIN3, INT_ReleaseSw4, CHANGE);
	iotsetup.initSetup();

	if (digitalRead(SETUP_PIN) == LOW) {
		setupAP();
		ap_mode = true;
	} else {
		i2cexp.initbus(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
		i2cexp.scanbus();

		if (i2cexp.hasBME280 == true) {
			Serial.println("CHECK hasBME280");
		}
		
		if (i2cexp.hasBH1750 == true) {
			Serial.println("CHECK hasBH1750");
		}

		if (i2cexp.haspcf8575 == true) {
			Serial.println("CHECK haspcf8575");
			device = new PCF8575(PCF8575_ADDR);
			device->digitalWrite(0x0000);
		}

		if (i2cexp.haspcf8574 == true) {
			Serial.println("CHECK haspcf8574");
			exp74 = new PCF8574(PCF8574_ADDR);
			exp74->digitalWrite(0x00);
		}

		if (i2cexp.hasPCA9685 == true) {
			Serial.println("CHECK hasPCA9685");
			pwmcontr.initPWM(iotsetup.getNumleds());
		}

		if (i2cexp.hasMCP23017 == true) {
			Serial.println("Check hasMCP23017");
			mcp1 = new MCP23017();
			mcp1->begin(MCP23017_ADDR);
		}

		if (iotsetup.getSsid().length() > 1) {
			WiFi.begin(iotsetup.getSsid().c_str(), iotsetup.getPasswd().c_str());
		}

		client.begin(iotsetup.getMqtthost().c_str(), iotsetup.getMqttport().toInt(), net);
		client.onMessage(messageReceived);
		connect();

		t_httpUpdate_return ret = ESPhttpUpdate.update(iotsetup.getOtahost(), iotsetup.getOtaport().toInt(), iotsetup.getOtaurl(), version);
		switch (ret) {
		case HTTP_UPDATE_FAILED:
			Serial.println("[update] Update failed.");
			break;
		case HTTP_UPDATE_NO_UPDATES:
			Serial.println("[update] Update no Update.");
			break;
		case HTTP_UPDATE_OK:
			Serial.println("[update] Update ok."); // may not called we reboot the ESP
			break;
		}
	}
}

//-------------------------------------------
// Interrupt handler
//-------------------------------------------
void INT_ReleaseSw0(void) {
	sw[0].state = !digitalRead(SW_PIN);
	sw[0].publish = true;
}
void INT_ReleaseSw1(void) {
	sw[1].state = !digitalRead(SETUP_PIN);
	sw[1].publish = true;
}
void INT_ReleaseSw2(void) {
	sw[2].state = !digitalRead(MOTION_PIN1);
	sw[2].publish = true;
}
void INT_ReleaseSw3(void) {
	sw[3].state = !digitalRead(MOTION_PIN2);
	sw[3].publish = true;
}
void INT_ReleaseSw4(void) {
	sw[4].state = !digitalRead(MOTION_PIN3);
	sw[4].publish = true;
}

//String getMacString(void) {
//	uint8_t MAC_array[6];
//	String macaddress = "";
//	WiFi.macAddress(MAC_array);
//
//	for (int i = 0; i < sizeof(MAC_array); ++i) {
//		macaddress.concat(String(MAC_array[i], HEX));
//	}
//
//	return macaddress;
//}

void connect() {
	Serial.println();
	Serial.println("Mac: " + WiFi.macAddress());

	digitalWrite(LED_PIN, LOW);
	Serial.print("checking wifi...");
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(500);
	}
	Serial.println();
	Serial.print("connecting...");

	while (!client.connect(iotsetup.getMqttclientid().c_str(), iotsetup.getMqttuser().c_str(), iotsetup.getMqttpassword().c_str())) {
		Serial.print(".");
		delay(1000);
	}
	Serial.println();
	Serial.println("connected!");
	digitalWrite(LED_PIN, HIGH);
	//subscribe to setup
	String dev_topic = iotsetup.getMqttdevice() + "/" + iotsetup.getMqttlocation();
	client.subscribe(dev_topic + "/" + subscriber_setupmode_id);
	//subscribe to output
	Serial.println("Subscribe to output");
	if (i2cexp.hasIOexpander()) {
		for (int i = 0; i < iotsetup.getNumoutputs(); i++) {
			String topic = dev_topic + "/" + subscriber_out_id + "/" + i;
			Serial.println("Subscribe: " + topic);
			client.subscribe(topic);
		}
		client.subscribe(dev_topic + "/" + subscriber_out_id + "/*");
	}
	//subscribe to rgb
	Serial.println("Subscribe to RGB");
	if (i2cexp.hasPWM()) {
		for (int i = 0; i < (iotsetup.getNumleds()); i++) {
			String topic = dev_topic + "/" + subscriber_rgb_id + "/" + i;
			Serial.println("Subscribe: " + topic);
			client.subscribe(topic);
		}
		client.subscribe(dev_topic + "/" + subscriber_rgb_id + "/*");
	}
}

void pubswitch(int pin, const char* topic) {
	Serial.print("SW press ");

	if (sw[pin].state == 0) {
		client.publish(iotsetup.getMqttdevice() + "/" + iotsetup.getMqttlocation() + "/" + topic + "/" + pin, "off");
		Serial.println(topic + String(pin) + "= off");
	} else {
		client.publish(iotsetup.getMqttdevice() + "/" + iotsetup.getMqttlocation() + "/" + topic + "/" + pin, "on");
		Serial.println(topic + String(pin) + "= on");
	}
}

void loop() {
	if (ap_mode) {
		server.handleClient();
		if (millis() - lastMillis > 500) {
			lastMillis = millis();
			if (blinkstate == LOW)
				blinkstate = HIGH;
			else
				blinkstate = LOW;

			digitalWrite(LED_PIN, blinkstate);
		}
	} else {
		if (!client.connected()) {
			connect();
		}

		client.loop();

		if (i2cexp.hasPCA9685 == true) {
			pwmcontr.animate();
		}
		//delay(10); // <- fixes some issues with WiFi stability

//		if (client.connected()) { //only send when connected to MQTT server
//		if (dopubsw > -1) {
//			pubswitch(dopubsw, publisher_switch_id);
//			dopubsw = -1;
//		}

		for (int i = 0; i < MAXSW; i++) {
			if (sw[i].publish) {
				pubswitch(i, publisher_switch_id);
				sw[i].publish = false;
			}
		}
		
		if (millis() - lastMillis > 10000) {
			if(i2cexp.getMetrics()) {
				lastMillis = millis();
				sendData();
			}
		}
		//i2cexp.getMetrics();

//		}

	}

}

void sendData() {
	float temp = i2cexp.metrics.temp;
	float hum = i2cexp.metrics.hum;
	float pres = i2cexp.metrics.pres;
	int luxvalue = i2cexp.metrics.luxvalue;

	char t[20];
	dtostrf(temp, 0, 1, t);
	char h[20];
	dtostrf(hum, 0, 1, h);
	char p[20];
	dtostrf(pres, 0, 1, p);

	StaticJsonBuffer<512> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["location"] = iotsetup.getMqttlocation();
	JsonObject& data = root.createNestedObject("data");
	data["temp"] = temp;
	data["humidity"] = hum;
	data["pressure"] = pres;
	data["lux"] = luxvalue;

	char buffer[200];
	int size = root.printTo(buffer, sizeof(buffer));

	client.publish(String(iotsetup.getMqttdevice() + "/" + iotsetup.getMqttlocation() + "/" + publisher_json_id).c_str(), buffer, size);

	root.prettyPrintTo(Serial);
	Serial.print("\n");
}

void messageReceived(String &topic, String &payload) {
	Serial.print("\nincoming: ");
	Serial.print(topic);
	Serial.print(" - ");
	Serial.print(payload);
	Serial.println();

	if (topic.startsWith(iotsetup.getMqttdevice() + "/" + iotsetup.getMqttlocation() + "/" + subscriber_rgb_id)) {
		int ind = (iotsetup.getMqttdevice() + "/" + iotsetup.getMqttlocation() + "/" + subscriber_rgb_id).length();
		String indstr = topic.substring(ind + 1, ind + 2);
		int pwmindex = indstr.toInt();
		//uint8_t pwmnum = pwmindex * 3;
		Serial.println("PWM rgb-index:" + String(pwmindex));

		if (payload.startsWith("pwm")) {
			Serial.println(" PWM ");
			pwmcontr.pwmLedStrip(indstr, payload);
			//check for hsl values
		} else if (payload.startsWith("rgb")) {
			Serial.println(" RGB ");
			pwmcontr.rgbLedStrip(indstr, payload);
			//check for hsl values
		} else if (payload.startsWith("hsl")) {
			Serial.println("PWM hsl-index:" + String(pwmindex));
			pwmcontr.hslLedStrip(indstr, payload);
		} else if (payload.startsWith("ani")) {
			Serial.println(" ANIMATION ");
			pwmcontr.setAnimate(indstr, payload);
		} else if (payload.startsWith("pulse")) {
			Serial.println(" PULSE ");
			pwmcontr.setPulse(indstr, payload);
		} else if (payload.startsWith("fad")) {
			Serial.println(" FADE ");
			pwmcontr.setFade(indstr, payload);
		} else if ((payload.startsWith("on")) || (payload.startsWith("off"))) {
			Serial.println(" SWITCH ");
			pwmcontr.switchLedStrip(indstr, payload);
		}
	} else if (topic.startsWith(iotsetup.getMqttdevice() + "/" + iotsetup.getMqttlocation() + "/" + subscriber_out_id)) {
		Serial.println("SWITCH - " + topic);
		int index = (iotsetup.getMqttdevice() + "/" + iotsetup.getMqttlocation() + "/" + subscriber_out_id).length();
		String snum = topic.substring(index + 1);
		uint8_t status = 0;
		if (snum.equals("*")) {
			if (payload.equals("off")) {
				//Serial.println(" off " + num);
				ioextender = 0;
			} else {
				//Serial.println(" on " + num);
				ioextender = 0b1111111111111111;
			}
		} else {
			int num = snum.toInt();

			//Serial.println("index = " + snum);
			if (payload.equals("off")) {
				//Serial.println(" off " + num);
				ioextender &= ~(1 << num);
			} else {
				//Serial.println(" on " + num);
				ioextender |= 1 << num;
			}
		}
		Serial.println("SEND ioexpander out");
		// attempt to write 16-bit word
		if (i2cexp.haspcf8575 == true) {
			Serial.println("haspcf8575");
			status = device->digitalWrite(ioextender);
		} else if (i2cexp.haspcf8574 == true) {
			Serial.println("haspcf8574");
			status = exp74->digitalWrite(ioextender);
		}
		if (i2cexp.hasIOexpander()) {
			if (status != TWI_SUCCESS) {
				Serial.print("write error ");
				Serial.println(status, DEC);
			}

		}
	} else if (topic.equals(iotsetup.getMqttdevice() + "/" + iotsetup.getMqttlocation() + "/" + subscriber_setupmode_id)) {
		setupAP();
		ap_mode = true;
	} else
		Serial.println("Unknown topic!");
}

void createWebServer(int webtype) {
	if (webtype == 1) {
		server.on("/", []() {
			IPAddress ip = WiFi.softAPIP();
			String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
			content = "<!DOCTYPE HTML>\r\n<html><h1>Configuration for ESP8266</h1><br/><hr/><p> ";
			content += "<table>";
			content += "<tr><td>version:</td><td>"+version+"</td></tr>";
			content += "<tr><td>Mac:</td><td>"+WiFi.macAddress()+"</td></tr>";
			content += "<tr><td>IP:</td><td>"+ipStr+"</td></tr>";
			content += "</table>";
			content += "<p>";
			content += "</p><form method='get' action='setting'><table>";
			content += "<tr><td><label>SSID: </label></td>";
			content += "<td><input type='text' name='ssid' length=32 list='accesspoints'/><datalist id='accesspoints'>";
			content += aplist;
			content += "</datalist>";
			content += iotsetup.getHTML();
//					content += "</td><td><label>password: </label></td><td><input type='text' name='pass' value='"+passwd+"' length=64></td></tr>";
//					content += "<tr><td><label>OTA host: </label></td><td><input name='otaserver' value='"+otahost+"' length=32></td><td><label>OTA port: </label></td><td><input name='otaport' value='"+otaport+"' length=8></td></tr>";
//					content += "<tr><td><label>OTA url: </label></td><td><input name='otaurl' value='"+otaurl+"' length=32></td></tr>";
//					content += "<tr><td><label>MQTT: </label></td><td><input name='mqttserver' value='"+mqtthost+"' length=32></td><td><label>port: </label></td><td><input name='mqttport' value='"+mqttport+"' length=8></td></tr>";
//					content += "<tr><td><label>MQTT dev: </label></td><td><input name='mqttdevice' value='"+mqttdevice+"' length=16></td><td><label>MQTT location: </label></td><td><input name='mqttlocation' value='"+mqttlocation+"' length=16></td></tr>";
//					content += "<tr><td><label>MQTT clientid: </label></td><td><input name='mqttclid' value='"+mqttclientid+"' length=32></td></tr>";
//					content += "<tr><td><label>MQTT user: </label></td><td><input name='mqttuser' value='"+mqttuser+"' length=16></td><td><label>MQTT password: </label></td><td><input name='mqttpwd' value='"+mqttpassword+"' length=16></td></tr>";
//					content += "<tr><td><label>#Led Strips: </label></td><td><input name='numleds' value='"+String(numleds)+"' length=8></td></tr>";
//					content += "<tr><td><label>#PCF8575: </label></td><td><input name='numoutputs' value='"+String(numoutputs)+"' length=8></td></tr>";
				content += "</table><BR/><input type='submit' value='Save'></form>";
				content += "</html>";
				server.send(200, "text/html", content);
			});
		server.on("/setting", []() {
			String qsid = server.arg("ssid");
			String qpass = server.arg("pass");
			String qotas = server.arg("otaserver");
			String qotap = server.arg("otaport");
			String qotau= server.arg("otaurl");
			String qmqtts = server.arg("mqttserver");
			String qmqttp = server.arg("mqttport");
			String qmqttd = server.arg("mqttdevice");
			String qmqttl = server.arg("mqttlocation");
			String qmqttc = server.arg("mqttclid");
			String qmqttu = server.arg("mqttuser");
			String qmqttw = server.arg("mqttpwd");
			String qnl = server.arg("numleds");
			String qnout = server.arg("numoutputs");

			if (qsid.length() > 0 && qpass.length() > 0) {
				Serial.println("clearing eeprom");
				for (int i = 0; i < EEPROM_MAX; ++i) {
					EEPROM.write(i, 0);
				}

				iotsetup.saveStringEeprom(qsid,SSID_EPOS);
				iotsetup.saveStringEeprom(qpass,PWD_EPOS);
				iotsetup.saveStringEeprom(qotas,OTAS_EPOS);
				iotsetup.saveStringEeprom(qotap,OTAP_EPOS);
				iotsetup.saveStringEeprom(qotau,OTAU_EPOS);
				iotsetup.saveStringEeprom(qmqtts,MQTS_EPOS);
				iotsetup.saveStringEeprom(qmqttp,MQTP_EPOS);
				iotsetup.saveStringEeprom(qmqttd,MQTD_EPOS);
				iotsetup.saveStringEeprom(qmqttl,MQTL_EPOS);
				iotsetup.saveStringEeprom(qmqttc,MQTC_EPOS);
				iotsetup.saveStringEeprom(qmqttu,MQTU_EPOS);
				iotsetup.saveStringEeprom(qmqttw,MQTW_EPOS);
				iotsetup.saveStringEeprom(qnl,NUMSTRIP);
				iotsetup.saveStringEeprom(qnout,NUMOUTP);

				EEPROM.commit();
				delay(500);
				iotsetup.readSettingsFromEeprom();
				content = "<!DOCTYPE HTML>\r\n<html>";
				content += "</p><form method='get' action='reboot'>";
				content += "Settings saved succesfully!<P>";
				content += "<input type='submit' value='Reboot'></form></html>";
				statusCode = 200;
			} else {
				content = "{\"Error\":\"404 Select your SSID\"}";
				statusCode = 404;
				Serial.println("Sending 404");
			}
			server.send(statusCode, "text/html", content);
		});
		server.on("/cleareeprom", []() {
			content = "<!DOCTYPE HTML>\r\n<html>";
			content += "<p>Clearing the EEPROM</p></html>";
			server.send(200, "text/html", content);
			Serial.println("clearing eeprom");
			for (int i = 0; i < EEPROM_MAX; ++i) {
				EEPROM.write(i, 0);
			}
			EEPROM.commit();
		});
		server.on("/reboot", []() {
			content = "<!DOCTYPE HTML>\r\n<html>";
			content += "<p>Rebooting ....</p></html>";
			server.send(200, "text/html", content);

			ESP.restart();
		});
	} else if (webtype == 0) {
		server.on("/", []() {
			IPAddress ip = WiFi.localIP();
			String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
			server.send(200, "application/json", "{\"IP\":\"" + ipStr + "\"}");
		});
		server.on("/cleareeprom", []() {
			content = "<!DOCTYPE HTML>\r\n<html>";
			content += "<p>Clearing the EEPROM</p></html>";
			server.send(200, "text/html", content);
			Serial.println("clearing eeprom");
			for (int i = 0; i < EEPROM_MAX; ++i) {
				EEPROM.write(i, 0);
			}
			EEPROM.commit();
		});
	}
}

void launchWeb(int webtype) {
	Serial.println("");
	Serial.println("WiFi connected");
	Serial.print("Local IP: ");
	Serial.println(WiFi.localIP());
	Serial.print("SoftAP IP: ");
	Serial.println(WiFi.softAPIP());
	createWebServer(webtype);
// Start the server
	server.begin();
	Serial.println("Server started");
}

void setupAP(void) {
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);
	ESP8266WiFiScanClass wifi;
	int n = wifi.scanNetworks();
	Serial.println("scan done");
	if (n == 0)
		Serial.println("no networks found");
	else {
		Serial.print(n);
		Serial.println(" networks found");
//		for (int i = 0; i < n; ++i) {
//			// Print SSID and RSSI for each network found
//			Serial.print(i + 1);
//			Serial.print(": ");
//			Serial.print(wifi.SSID(i));
//			Serial.print(" (");
//			Serial.print(wifi.RSSI(i));
//			Serial.print(")");
//			Serial.println(
//					(WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
//			delay(10);
//		}
	}
	Serial.println("");
	aplist = "";
	for (int i = 0; i < n; ++i) {
		// Print SSID and RSSI for each network found
		aplist += "<option value='";
		aplist += wifi.SSID(i);
		aplist += "'>";
		aplist += wifi.SSID(i);
		aplist += "(";
		aplist += wifi.RSSI(i);
		aplist += ")";
		aplist += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
		aplist += "</option>";
	}
	delay(100);
//Serial.println(st);
	WiFi.softAP(apssid, passphrase, 0);
	Serial.println("SoftAp initialized.");
	launchWeb(1);
	Serial.println("WebInterface started.");
}

