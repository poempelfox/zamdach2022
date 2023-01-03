
# ZAM-Dach 2022

A bunch of environment sensors mounted on the roof of the ZAM (https://zam.haus) in Erlangen.

The first version/iteration of this was mounted on the roof of the ZAM on January 1st 2023.

The measured data can currently be found here: https://wetter.poempelfox.de/static/zamdach.html

More info (decision making from the development phase) might be in `docs/brainstorming.md`.

## Things measured

* particulate matter (via SEN50)
* pressure (via LPS25HB)
* temperature (via SHT41) - note that a roof in the centre of the city is certainly not the perfect place to measure temperature
* humidity (via SHT41)
* ambient light (via LTR390)
* UV-Index (via LTR390)
* wind speed
* wind direction
* rain (via optical RG15 rain sensor)

## Assembly

FIXME: There really should be some pictures here.

The principal mount for ZAM-Dach2022 is an "U" aluminium tube that is screwed to the mount for the (preexisting) solar panels on the roof.
It sticks out a bit further than the solar panels.

Most sensors were mounted in two "┌┐" and one "└┐" made from 90° drainage pipe bends.
This is the same mounting-technique used by the luftdaten.info project to have cheap but relatively reliable weather protection.
The drainage pipe thingies are then held by pipe clamps screwed onto the side of the "U" aliminium tube.

The drainage-pipe-housing where one side faces up is the one housing the ambient light / UV-Index-sensor.
The side facing up has a petri-dish made from borosilicate on it, glued on and sealed with some special (extra-large temperature range) glue we found laying around in the ZAM.
The sensor is sitting directly beneath it, kept in place flatly by some wooden/HDF mount that is tucked inside the drain pipes so it cannot move.

One of the other two housings houses the ESP32-POE-ISO and the pressure sensor.
This one is not held by pipe clamps because we did not have enough large-enough pipe clamps but cable length constraints, so instead it is attached in the middle with zip-ties.

The final housing is home to the SHT41 temperature/humidity-sensor, and the SEN50 particulate matter sensor.
Again, a piece of HDF inside the drainage pipe bends keeps things neatly in place, which is especially critival for the rather large SEN50.

The RG15 rain sensor is screwed to the tip of the "U" aluminium tube, with a small bended metal sheet that corrects its angle.

The wind speed and direction sensors came with their own mount, and that is screwed to the "U" aluminium tube with yet another mini-pipe-clamp.

## Firmware

### Features

All measurements are done once per minute.

The firmware has a tiny webinterface, where you can see the current values in a
browser, or you can download a JSON with the values for use in scripting. Said
webinterface can also be used to trigger a firmware update, provided you know
the correct password for that (set in `secrets.h` at compile time).

The firmware will also send all values to wetter.poempelfox.de for storage.

### TODOs

Unfortunately, the following features were not implemented before the sensor
was mounted on the roof, but they still should be added - and we can do updates
through the webinterface of the sensor, though it is a bit risky...

* Automatically turn on the heater on the SHT41 after extended periods of extremely high humidity
* send data not only to wetter.poempelfox.de but also to madavi and sensor.community
* not only measure average wind speed over 1 minute but also peak wind speed.
