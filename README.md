# IOT-ESP
IOT home automation with esp8266

connects to mqtt broker

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

n is switch number 0 to max 5

### payload
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