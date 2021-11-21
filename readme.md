Using esp32 and various type of temp/humidity sensor to send data to an mqtt server.

Includes code for 4 different types of sensor.

- DS18
- AHT10 & AHT20
- BME/BMP 280

Choose which sensor you are looking to use at the end of the config file.

Not tested but this should allow usage of multiple types of sensor, but not
multiple of the same sensor.



Various libs are hidden away deeper in the code so ill include them here. Lib
Version numbers are included if I can but those are only the last known working
version so try later versions and update reference if need be.

#### BME/BMP 280
[Adafruit_BMP280_Library](https://github.com/adafruit/Adafruit_BMP280_Library)
// V 2.4.2

[BME280I2C](https://github.com/finitespace/BME280)
#### AHT10 & AHT20
[Adafruit_AHTX0](https://github.com/adafruit/Adafruit_AHTX0) // V 2.0.1
#### DS18B20
[OneWire](https://www.pjrc.com/teensy/td_libs_OneWire.html)

[DallasTemperature/Arduino-Temperature-Control-Library](https://github.com/milesburton/Arduino-Temperature-Control-Library)
