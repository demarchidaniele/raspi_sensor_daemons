# Raspi Sensor Daemons
A set of daemons for reading sensors on a Raspberry PI.

Tested on RaspberryPi 3

Actually sensors supported are:

AS3935 - Franklin Lightning

BMP180 - Barometric Pressure

TX23   - Lacrosse Wind speed and direction

DHT22  - Temperature and Humidity

Each executalbe can run as a foreground process or a daemon.

If lauched as a foreground process, the software pool the sensor for a while and print the values on exit.

If launched as a daemon, the same executalbe must be started twice: one for running the dameon and a second time for asking data to the background process.

-------------------------------
Compilation

do a "make" in the main folder

-------------------------------
Usage

... to be written

... but for now run "./readXXXX --help" to get some basic instructions.
