# ESP8266 AT FIRMWARE

This repository contains a new build of ESP8266 AT Firmware with some special features.<br>
The repository contains all necessary tools to build the firmware, _xtensa-lx106 toolchain_, _SDK_, _bootloader sources_... <br>
Bash scripts for easy building and flashing are included.<br>
All sources are (hopefully) well documented, there should be no difficulties following the code and make same changes if needed.<br>

_There is no flash space to include both WPA Enterprise **and** SmartConfig into 512+512 Flash map._<br>
_Only one of them can be compiled, if needed at all-_<br>

> Maximal size of the flash file for     512, no OTA firmware is 0x75000 (479232, 468KB) bytes<br>
> Maximal size of the flash file for   512+512 firmware is 0x79000 (495616, 484KB) bytes<br>
> Maximal size of the flash file for 1024+1024 firmware is 0xF0000 (983040, 960KB) bytes<br>


[Documentation for new and changed **AT Commands**](https://github.com/loboris/ESP8266_AT_LoBo/blob/master/at_lobo/README.md)



## Main features:

* Built with the latest ESP8266 NON_OS SDK (v. 3.1)
* Can be installed on all ESP8266 supported SPI Flash sizes
* OTA update from any server, with MD5 checksum support, SSL support, version check, ...
  * Auto firmware partition select or forced update to any available partition
  * OTA **Bootloader** update is also supported
* Load any available firmware version, not only the latest, is supported
* Uses my own 2nd stage bootloader (full source available) for some special features
* New AT commands added
* Improved SSL/TLS support, up to 16384 bytes SSL buffer can be used enabling more SSL functionality
* Added support for loading CA certificate to RAM buffer
* Added support for loading the firmware from any location in SPI Flash, even above 2MB

## Supported SPI Flash sizes:

* 1MB 512+512 map, 2 OTA partitions
* 2MB 512+512 map, up to 4 OTA partitions
* 4MB 512+512 map, up to 8 OTA partitions
* 2MB 1024+1025 map, 2 OTA partitions
* 4MB 1024+1025 map, up to 4 OTA partitions
* 512K single firmware, no OTA available.

Larger Flash sizes can also be used.

## BUILDING

Bash script **build.sh** is provided for easy build of firmwares for all supported Flash sizes.<br>
It builds all firmwares binaries and creates version info and MD5 checksum files needed for OTA.<br>
To use it, change the working directory to `at_lobo` and run `.\build.sh`.

```
boris@UbuntuMate:/home/LoBo2_Razno/MAIX/ESP8266_AT_LoBo/at_lobo$ ./build.sh

=====================================
Building ESP8266/ESP8285 AT firmwares
=====================================


Building firmware for 1MB Flash (ESP8285, DOUT spi mode)
  File size = 432532
  esp8285_AT_1_2.bin OK
  esp8285_AT_2_2.bin OK

Building firmware for 512KB Flash, Flash map: 2 (512 no OTA)
  File size = 420388
  esp8266_AT_1_0.bin OK

Building firmware for 1MB Flash, Flash map: 2 (512+512)
  File size = 432532
  esp8266_AT_1_2.bin OK
  esp8266_AT_2_2.bin OK

Building firmware for 2MB Flash, Flash map: 3 (512+512)
  File size = 432612
  esp8266_AT_1_3.bin OK
  esp8266_AT_2_3.bin OK

Building firmware for 4MB Flash, Flash map: 4 (512+512)
  File size = 432612
  esp8266_AT_1_4.bin OK
  esp8266_AT_2_4.bin OK

Building firmware for 2MB Flash, Flash map: 5 (1024+1024)
  File size = 432612
  esp8266_AT_1_5.bin OK

Building firmware for 4MB Flash, Flash map: 6 (1024+1024)
  File size = 432612
  esp8266_AT_1_6.bin OK

=====================================
Finished.
boris@UbuntuMate:/home/LoBo2_Razno/MAIX/ESP8266_AT_LoBo/at_lobo$ 
```

```
boris@UbuntuMate:/home/LoBo2_Razno/MAIX/ESP8266_AT_LoBo/at_lobo$ ls -l ../bin/upgrade
total 4700
-rw-rw-r-- 1 boris boris 420388 tra  29 12:06 esp8266_AT_1_0.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8266_AT_1_0.md5
-rw-rw-r-- 1 boris boris 432532 tra  29 12:06 esp8266_AT_1_2.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8266_AT_1_2.md5
-rw-rw-r-- 1 boris boris 432612 tra  29 12:06 esp8266_AT_1_3.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8266_AT_1_3.md5
-rw-rw-r-- 1 boris boris 432612 tra  29 12:06 esp8266_AT_1_4.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8266_AT_1_4.md5
-rw-rw-r-- 1 boris boris 432612 tra  29 12:06 esp8266_AT_1_5.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8266_AT_1_5.md5
-rw-rw-r-- 1 boris boris 432612 tra  29 12:06 esp8266_AT_1_6.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8266_AT_1_6.md5
-rw-rw-r-- 1 boris boris 432532 tra  29 12:06 esp8266_AT_2_2.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8266_AT_2_2.md5
-rw-rw-r-- 1 boris boris 432612 tra  29 12:06 esp8266_AT_2_3.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8266_AT_2_3.md5
-rw-rw-r-- 1 boris boris 432612 tra  29 12:06 esp8266_AT_2_4.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8266_AT_2_4.md5
-rw-rw-r-- 1 boris boris 432532 tra  29 12:06 esp8285_AT_1_2.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8285_AT_1_2.md5
-rw-rw-r-- 1 boris boris 432532 tra  29 12:06 esp8285_AT_2_2.bin
-rw-rw-r-- 1 boris boris     32 tra  29 12:06 esp8285_AT_2_2.md5
-rw-rw-r-- 1 boris boris    574 tra  29 12:06 version.txt
```
The firmware files are created in `bin/upgrade` directory

## Flashing

The initial firmware must be flashed to the ESP8266.<br>
Bash script `flash.sh` is provided to make the flashing as easy as possible.<br>
The correct `flash.sh` options must be specified, depending on the Flash type used:<br>
Change the working directory to **`at_lobo`** and run **`flash.sh`** script<br>
<br>

> The default serial port used is `/dev/ttyUSB0`<br>
> If your board is attached to some other port, add the option **`-P <port>`**, where `<port` is serial port to which the bord is attached.<br>

<br>

> If your ESP8266/ESP8285 board cannot be reset by USB->UART adapter (for example MAIX M1W)<br>
> it is recommended to erese the chip first with `.\flash.sh -eo`,<br>
> then re-power it and execute one of the following commands with added **`-ne`** option<br>

<br>

Flash to ESP8285, 1MB, 512+512 map, in dout mode:
```
.\flash.sh -t 1MB -m dout
```
Flash to ESP8266, 1MB, 512+512 map, in qio mode:
```
.\flash.sh -t 1MB
```
Flash to ESP8266, 2MB, 512+512 map, in qio mode:
```
.\flash.sh -t 2MB
```
Flash to ESP8266, 4MB, 512+512 map, in qio mode:
```
.\flash.sh -t 4MB
```
Flash to ESP8266, 2MB, 1024+1024 map, in qio mode:
```
.\flash.sh -t 2MB-c1
```
Flash to ESP8266, 4MB, 1024+1024 map, in qio mode:
```
.\flash.sh -t 4MB-c1
```
Flash to ESP8266, 512KB, single firmware, no OTA:
```
.\flash.sh -t 512KB
```

## NOTE: Python 2/3 problem fix

Recent updates of various Linux distros, e.g. Ubuntu 20.04, have Python 3 as their default Python version. That's what is invoked with the ```python``` command, and in this case will cause the building scripts to fail. 

A simply workaround is to create a virtual environment with Python 2.

To do so: 

* install virtualenv if necessary with ```pip install virtualenv```
* find where your python2 is: ```which python2```, a possible outcome is ```/usr/bin/python2```.
* create a virtual environment with ```virtualenv -p /usr/bin/python2 venv```
* activate it with ```source venv/bin/activate```

Now if you just type python you'll see that it's 2.7.x and not 3.x.x, and ./build.sh will work.

To leave the virtual environment simply type ```deactivate```.

## NOTE: Flashing issues fix

Sometimes the module times out and the flashing doesn't work. A proven way to do it is as follows:

* use the FTDI breakout board and the breadboard power supply module (MB102/HW-131 kind)
* set the power supply to 5V (to power the ESP module)
* set the FTDI to 3.3V (RX/TX signals must be 0-3.3V only)
* connect GND of the FTDI and the GND of the ESP module to the GND rail (must be common ground)
* connect CHPD pin of the ESP module to 5V
* connect GPIO0 pin of the ESP module to 5V through 10k pull-up resistor

* connect a jumper wire to ESP module RST pin, for easy resetting

Now after the flash.sh is going and seems to be stuck on "Connecting....----...." reset the board by quickly attaching the loose RST wire to GND rail.
This might have to be done twice, first when the flash is erased and then when it's written to.

