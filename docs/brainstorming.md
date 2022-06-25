
= Brainstorming / datenmuellhalde

== Possible/selected sensors

- Sunlight
  + MAX44009 Ambient Light sensor (up to 188000 lux)
  + TSL2591 (up to 88000 lux) (in contrast to most others this seems to actually be available)
- Pressure
  + LPS25HB
- Temperature
  + SHT31 / SHT35. SHT35 is probably a waste due to the mounting location - the temperatures on the roof in the middle of the city will always be way off, so the high accuracy is wasted.
- Rain
  + RG15 optical sensor
  + there is also a rain gauge on the Sparkfun weather meter kit - but that will probably not last long. We could still connect it up.
- Wind speed/direction
  + Sparkfun Weather meter kit https://learn.sparkfun.com/tutorials/weather-meter-hookup-guide/all
- Particulate matter
  + SPS30
  + SDS011
- Radiation
  + no idea yet. And more of a joke than a real sensor.

== Mainboard

Olimex ESP32-POE-ISO. Fox has a revision B board lying around.

This allows us to either:
- Power and network through POE
- Power through solar and network via WiFi
  + Feed Power into the ESP via its USB connector
  + use "big" solar panel with "big" AGM battery, and a readymade solar battery controller that has USB output
  + TIL: there are nowadays such batterys with builtin Bluetooth m(  (the bluetooth is in the BATTERY BLOCK, not in the charge controller!)
  + It would also be possible to connect a LiPo battery directly to the ESP32 board, it has a simple charger builtin, but I don't think that is a good idea. Because considering the harsh conditions this will have to endure, a less volatile battery setup is probably a good idea.

