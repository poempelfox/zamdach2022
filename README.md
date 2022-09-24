
# ZAM-Dach 2022

A bunch of environment sensors to be mounted on the roof of the ZAM (https://zam.haus) in Erlangen. This is a WIP, it is NOT finished.

More info might be in the 'docs' directory.

## Things measured

* particulate matter (via SEN50)
* pressure (via LPS25HB)
* temperature (via SHT41) - note that a roof in the centre of the city is certainly not the perfect place to measure temperature
* humidity (via SHT41)
* ambient light (via LTR390)
* UV-Index (via LTR390)
* wind speed
* wind direction

## TODO before version "1.0"

This is not finished yet, and before firmware-release 1.0, the following should be implemented:

* Automatically turn on the heater on the SHT41 after extended periods of extremely high humidity
* send data not only to wetter.poempelfox.de but also to madavi and sensor.community
* not only measure average wind speed over 1 minute but also peak wind speed.

