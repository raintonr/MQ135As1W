# Deprecated

This code should be considered deprecated in favour of the following:

https://github.com/raintonr/arduino-sensors-for-loxone

**Old code is left intact here for archive purposes only.**

## MQ135As1W

Modified version of Jiri Jasa's Arduino Pro Mini 1-Wire MQ135 interface. I would have cloned his repo but there wasn't one :(

This version basically does away with the MQ135 library in favour of simply dumping a raw analog value to the 1-Wire interface. As the Arduino Pro Mini ADC doesn't have a greater resolution than the DS2438 didn't see that being a problem. Any final value manipulation can be done in the recieving system.

### Original Version

Please see comments in the code, and also:

https://www.youtube.com/watch?v=v9EQMbyk62w

http://jirin.deso.cz/MQ135_to_1-Wire/
