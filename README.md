# Raspi Sensor Daemons
A set of daemons for reading sensors on a Raspberry PI.

Tested on RaspberryPi 3

Actually sensors supported are:

AS3935         - Franklin Lightning

BMP180         - Barometric Pressure

TX23           - Lacrosse Wind speed and direction

DHT11 / DHT22  - Temperature and Humidity

Each executable can run as a foreground process or a daemon.

If lauched as a foreground process, the software query the sensor for a while and print the values on exit.

If launched as a daemon, the same executable must be started twice: one for running the dameon and a second time for query data to the background process.

-------------------------------

Compilation

do a "make" in the main folder to build:

readDHT

readBMP180

readTX23

readAS3935

do a "make install" to install everything on /usr/local/bin

-------------------------------

readDHT manpage


NAME

       readDHT - read values from DHT11 or DHT22 Temperature and Humidity sensor


SYNOPSIS

       readDHT - options


DESCRIPTION

       Read values from DHT sensor connected to Raspberry PI GPIO, and print information on screen in variuos formats. Also can start a background dameon for continuos reading. Daemon instance can be queried from the same executable with the -q parameter.
       Parameter -gpon is useful to recover a bad reading or a critical stop of the sensor. Due to the low consumption, gpon pin can be used to power the sensor.

       Mandatory arguments to long options are mandatory for short options too.

	-v, --verbose
		Give detailed messages

	-t,--type
		Specify DTH type [11 or 22] (default 22)

	-gpio, --gpio
		Select GPIO port (default 22)

	-gpon, --gpon
		Select GPIO port to Power ON of the sensor (default none)

	-r [0..10], --retry[0..10]
		Retry counter

	-d, --daemonize
		Start daemon with update cycle data refresh

	-uc [5..60], --update-cycle [10..60]
		Set update cycle (default 3)

	-s [1..255], --samples [1..64]
		Set average samples num(default 1)

	-q, --query-daemon
		Query the daemon

	-psa, --print-sampling
		Print sampling of input pin

	-pt, --print-temperature-only
		Print temperature only

	-ph, --print-humidity-only
		Print humudity only

	-pj, --print-json
		JSON output

	-?, --help
		Show usage information

AUTHOR

       Written by Daniele De Marchi.


REPORTING BUGS

       Use github <https://github.com/demarchidaniele/raspi_sensor_daemons/issues>


COPYRIGHT

       Copyright    ©    2016    Daniele De Marchi     License   GPLv3+:   GNU   GPL   version   3   or   later
       <http://gnu.org/licenses/gpl.html>.
       This is free software: you are free to change and redistribute it.  There is NO WARRANTY, to the extent permitted by law.


SEE ALSO

       Full documentation at: <https://github.com/demarchidaniele/raspi_sensor_daemons>

-------------------------------

readAS3935 manpage


NAME

       readAS3935 - read values from AS3935 


SYNOPSIS

       readAS3935 - options


DESCRIPTION

       Read values from AS3935 sensor connected to Raspberry PI GPIO, and print information on screen in variuos formats. Also can start a background dameon for continuos reading. Daemon instance can be queried from the same executable with the -q parameter.
       AS3935 must be connected using SPI interface and SPI_CE0 as enable.
       During startup readAS3935 perform the proper tuning of antenna and intial noise floor level.
       During execution readAS3935 perform a continuos adjust of noise floow up and down of the interrupt level.

       Mandatory arguments to long options are mandatory for short options too.

	-v, --verbose
		Give detailed messages

	-w, --wait
		Wait n second for events if not running as a daemon

	-d, --daemon
		Start daemon core

	-dk, --daemonkill
		Kill dameon core

	-q, --query-daemon
		Query the daemon

	-qcmd, --query-command
		Query the command to the daemon

	-pj, --print-json
		Print JSON output

	-?, --help
		Show usage information

AUTHOR

       Written by Daniele De Marchi.


REPORTING BUGS

       Use github <https://github.com/demarchidaniele/raspi_sensor_daemons/issues>


COPYRIGHT

       Copyright    ©    2016    Daniele De Marchi     License   GPLv3+:   GNU   GPL   version   3   or   later
       <http://gnu.org/licenses/gpl.html>.
       This is free software: you are free to change and redistribute it.  There is NO WARRANTY, to the extent permitted by law.


SEE ALSO

       Full documentation at: <https://github.com/demarchidaniele/raspi_sensor_daemons>

-------------------------------

readBMP180 manpage

NAME

       readBMP180 - read values from BMP180 


SYNOPSIS

       readBMP180 - options


DESCRIPTION

       Read values from BMP180 sensor connected to Raspberry PI GPIO, and print information on screen in variuos formats. Also can start a background dameon for continuos reading. Daemon instance can be queried from the same executable with the -q parameter.
       BMP180 must be connected using I2C interface.

       Mandatory arguments to long options are mandatory for short options too.

	-v, --verbose
		Give detailed messages

	-r [0..10], --retry[0..10]
		Retry counter

	-d, --daemonize
		Start daemon with update cycle data refresh

	-uc [10..60], --update-cycle [10..60] 
		Set update cycle (default 10)

	-s [1..255], --samples [1..64]
		Set average samples num(default 1)

	-q, --query-daemon
		Query the daemon

	-pj, --print-json
		JSON output

	-?, --help
		Show usage information


AUTHOR

       Written by Daniele De Marchi.


REPORTING BUGS

       Use github <https://github.com/demarchidaniele/raspi_sensor_daemons/issues>


COPYRIGHT

       Copyright    ©    2016    Daniele De Marchi     License   GPLv3+:   GNU   GPL   version   3   or   later
       <http://gnu.org/licenses/gpl.html>.
       This is free software: you are free to change and redistribute it.  There is NO WARRANTY, to the extent permitted by law.


SEE ALSO

       Full documentation at: <https://github.com/demarchidaniele/raspi_sensor_daemons>

-------------------------------

readTX23 manpage

NAME

       readTX23 - read values from LaCrosse TX23 Wind Sensor 


SYNOPSIS

       readTX23 - options


DESCRIPTION

       Read values from LaCrosse TX23 sensor connected to Raspberry PI GPIO, and print information on screen in variuos formats. Also can start a background dameon for continuos reading. Daemon instance can be queried from the same executable with the -q parameter.
       LaCrosse TX23 seems to suffer of bitrate instability that could cause reading errors.
       This instability is avoided by the auto bitrate adapative algorithm.

       Mandatory arguments to long options are mandatory for short options too.

	-v, --verbose
		Give detailed messages

	-gpio, --gpio
		Select GPIO port (default 17)

	-r [0..10], --retry[0..10]
		Retry counter

	-d, --daemonize
		Start daemon with update cycle data refresh

	-uc [5..60], --update-cycle [5..60]
		Set update cycle (default 3)

	-s [1..64], --samples [1..64]
		Set samples (default 5)

	-d, --query-daemon
		Query the daemon

	-psa, --print-sampling
		Print sampling of input pin

	-psmax, --print-speedmax-only
		Print wind speed max only

	-psmin, --print-speedmin-only
		Print wind speed min only

	-psave,	--print-speedave-only
		Print wind speed average only

	-pd, --print-direction-only
		Print wind direction only

	-pj, --print-json
		JSON output

	-ms, --speed_ms
		Print speed in m/s

	-kmh, --speed_kmh
		Print speed in Km/h

	-deg, --degrees
		Print direction in degrees from north

	-dego [0..360],	--degrees-offset [0..360]
		Correct Offset degrees from north

	-?, --help
		Show usage information


AUTHOR

       Written by Daniele De Marchi.


REPORTING BUGS

       Use github <https://github.com/demarchidaniele/raspi_sensor_daemons/issues>


COPYRIGHT

       Copyright    ©    2016    Daniele De Marchi     License   GPLv3+:   GNU   GPL   version   3   or   later
       <http://gnu.org/licenses/gpl.html>.
       This is free software: you are free to change and redistribute it.  There is NO WARRANTY, to the extent permitted by law.


SEE ALSO

       Full documentation at: <https://github.com/demarchidaniele/raspi_sensor_daemons>
       LaCrosse TX23 protocol descrition <https://www.john.geek.nz/2012/08/la-crosse-tx23u-anemometer-communication-protocol>
