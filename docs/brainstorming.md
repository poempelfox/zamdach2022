
# Brainstorming / datenmuellhalde

## Possible/selected sensors

- Sunlight
  + MAX44009 Ambient Light sensor (up to 188000 lux)
  + TSL2591 (up to 88000 lux) (in contrast to most others this seems to actually be available)
  + There seem to be few to no digital UV-sensors available in 2022. VEML6075 and VEML6070 have both been discontinued and there seems to be no successor. Si1145 is available, but it doesn't contain an actual UV sensing element, instead it makes guesstimates from visible+IR light. LTR390 might be our only option.
  + we'll now use TSL2591 for light and LTR390 for UV sensing.
  + This will need to be directly exposed to the sunlight. We'll need to put it under some sort of protective covering, figure out how much light/UV that absorbs, and then correct the measured values accordingly.
- Pressure
  + LPS25HB
  + This does not need direct exposure to light or air, and can be mounted in a relatively protected position, e.g. together with the ESP32-POE-ISO.
- Temperature
  + SHT31 / SHT35 / SHT40 / SHT41 / SHT45. the better sensors are probably a waste due to the mounting location - the temperatures on the roof in the middle of the city will always be way off, so the high accuracy is wasted.
  + we'll now use a SHT40 or SHT41.
- Rain
  + RG15 optical sensor
  + will need to be mounted on a metal arm on its own, needs direct sky access
  + there is also a rain gauge on the Sparkfun weather meter kit - but that will probably not last long. We could still connect it up.
- Wind speed/direction
  + Sparkfun Weather meter kit https://learn.sparkfun.com/tutorials/weather-meter-hookup-guide/all
  + mounting TBD - this comes with its own metal pole and wind speed/direction sensors on top, and an arm attached to the side for its rain gauge.
- Particulate matter
  + SPS30 - seems to be better for PM2.5 (less susceptible to high humidity), but only measures PM1.0 and PM2.5, the PM10.0 readings it gives are just estimates and absolutely useless. A bit more expensive. Needs 5V input voltage (but communication can be either 5 or 3.3V).
  + SDS011 - this has been used for years. Pretty good and affordable, but it has its weaknesses - like massively overestimating PM-values when humidity is high. Measures PM2.5 and PM10.0. Needs 5V input voltage (but serial communication is still at 3.3V).
  + in the end, both sensors will be off by quite a bit, but in different ways
  + interesting evaluation in https://amt.copernicus.org/articles/13/2413/2020/
  + there seems to be a relatively new SEN50 that might be a very good alternative, but it also only actually measures PM2.5, and calculates PM10.0 "based on distribution profile of all measured particles", whatever that means.
  + we'll now use a SEN50.
- Radiation
  + this will be a seperate sensor-project. The risk of the high voltage stuff required for the Geiger-Mueller-Tube damaging the rest, e.g. if stuff gets wet, is just too high.

## Mainboard

We'll use an Olimex ESP32-POE-ISO.
Fox has a revision B board lying around.

This allows us to either:
- Power and network through POE
  + a problem with this is that the POE chip does have a rather high power dissipation, and that might both mess with other measurements and cause overheating problems. We'll have to evaluate if this is really workable.
- Power through solar and network via WiFi
  + Feed Power into the ESP via its USB connector
  + use "big" solar panel with "big" AGM battery, and a readymade solar battery controller that has USB output
  + TIL: there are nowadays such batterys with builtin Bluetooth m(  (the bluetooth is in the BATTERY BLOCK, not in the charge controller!)
  + It would also be possible to connect a LiPo battery directly to the ESP32 board, it has a simple charger builtin, but I don't think that is a good idea. Because considering the harsh conditions this will have to endure, a less volatile battery setup is probably a good idea.

## GPIO / pin usage

This is the planned GPIO / pin usage:

| Pin  | Use |
| ---  | --- |
| GPI34 == RTC_GPIO4 | Aenometer (wind sensor) |
| GPIO13 | I2C-bus A SDA |
| GPIO16 | I2C-bus A SCL |
| GPIO4 == UART1_TX | RG15 rain sensor |
| GPI36 == UART1_RX | RG15 rain sensor |
| ? | I2C-bus B SDA |
| ? | I2C-bus B SCL |

We have not decided how to distribute all the I2C devices onto the two I2C buses yet.
(The reason that there are exactly two I2C buses is that this is what the ESP32 can do in hardware.)

