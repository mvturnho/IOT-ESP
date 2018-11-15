/*
 * target.h
 *
 *  Created on: 23 feb. 2017
 *      Author: michi_000
 */

#ifndef TARGET_H_
#define TARGET_H_

#define DEBUG

//Harware layout
#define LED_PIN 	D0
#define SETUP_PIN	D1
#define I2C_SDA_PIN	D2
#define SW_PIN 		D3
#define I2C_SCL_PIN	D4
//#define IOEXT_INT	D5
#define MOTION_PIN1 	D5
#define MOTION_PIN2 	D6     // what digital pin we're connected to
#define MOTION_PIN3		D7

//EEPROM data locations
#define SSID_EPOS	0		//32
#define PWD_EPOS	32		//32
#define OTAS_EPOS	64		//32
#define OTAP_EPOS	96		//8
#define OTAU_EPOS	104		//32
#define MQTS_EPOS	136		//32
#define MQTP_EPOS	168		//8
#define MQTD_EPOS	176		//16
#define MQTL_EPOS	192		//16
#define MQTC_EPOS	208		//32
#define MQTU_EPOS	240		//16
#define MQTW_EPOS	256		//16
#define NUMSTRIP	264		//8
#define NUMOUTP		272		//8

#define EEPROM_MAX  300



#endif /* TARGET_H_ */
