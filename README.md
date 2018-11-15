# IOT-ESP
IOT home automation with esp8266

The device can be configured by pressing the button on pin D1 when booting up the device.
It is then configured as an access point with SSID SETUP_ESP and password setupesp.
When connected you can direct your browser to 192.168.4.1.

[!(docs/images/iot-dev.PNG)]

## subscribes to
- led
- rgb
- hsl
- out

`device/location/rgb/n`      
`device/location/hsl/n` 

n is 0 to configured ledstrips (max 5)
or n is * to address all leddstrips

## publishes
- json (sensordata)
- switch/n

`device/location/switch/n`

n is switch number 0 to max 4
D3, D5, D6 , D7 are the pins for switches or motion detectors.

### payload for json sensordata
```json
{
    "location":"testdev",
    "data":{
        "temp":23.91,
        "humidity":48.86133,
        "pressure":1.025782,
        "lux":106
    }
}
```
