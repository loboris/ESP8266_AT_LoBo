# ESP8266 AT FIRMWARE, new and changed AT commands

<br><br>

## AT+CIPSSLSIZE

Changed the range of values accepted to 2048 - **16384**<br>
A value of 8192 or more is sometimes required for successful SSL/TLS communication.


## AT+SYSFLASHMAP

_**Query**_<br>
Returns the current SPI Flash map, flash mode and description:<br>
```
AT+SYSFLASHMAP?
+SYSFLASHMAP:2,3,"esp8285 1MB 512+512"

OK
```

## AT+SYSCPUFREQ

_**Query**_<br>
Returns the current ESP8266 CPU frequency:<br>
```
AT+SYSCPUFREQ?
+SYSCPUFREQ:80

OK
```
_**Set**_<br>
Only 80 and 160 (MHz) can be set.<br>
```
AT+SYSCPUFREQ=160

OK
AT+SYSCPUFREQ?
+SYSCPUFREQ:160

OK
```

## AT+SNTPTIME

_**Query**_<br>
Returns the current SNTP accquired time: unix time stamp and ANSI formated time string<br>
```
AT+SNTPTIME?
+SNTPTIME:1556721502,2019-05-01 14:38:22

OK
```
_**Test**_<br>
```
AT+SNTPTIME=?
+SNTPTIME:<unix_timestamp>,YYYY-MM-DD HH:NN:SS

OK
```

## AT+SSLCCONF

This command has the same function as **AT+CIPSSLCCONF**, but is used with new **TCP** commands

## AT+SSLLOADCERT

CA certificate can be loaded into RAM and used instead of the default CA certificate from SPI Flash sector.
The certificate must be in the PEM format, for example (the certificate for all servers using _Let's Encrypt_):
```
-----BEGIN CERTIFICATE-----
MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/
MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT
DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow
SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT
GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC
AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF
q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8
SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0
Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA
a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj
/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T
AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG
CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv
bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k
c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw
VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC
ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz
MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu
Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF
AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo
uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/
wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu
X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG
PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6
KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==
-----END CERTIFICATE-----
```

_**Query**_<br>
If no certificate was loaded:<br>
```
AT+SSLLOADCERT?
+SSLLOADCERT:0

OK
```
If the certificate was loaded returns certificate sector number (hex encoded), certificate length and the certificate itself:
```
AT+SSLLOADCERT?
+SSLLOADCERT:0,007B,1646

-----BEGIN CERTIFICATE-----
MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/
MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT
DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow
SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT
GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC
AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF
q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8
SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0
Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA
a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj
/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T
AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG
CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv
bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k
c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw
VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC
ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz
MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu
Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF
AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo
uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/
wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu
X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG
PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6
KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==
-----END CERTIFICATE-----

OK
```

_**Set**_<br>

AT+SSLLOADCERT=crt_no[,length]

_`crt_no`_ only the value of `0` is accepted for now<br>
_`length`_ the length af the certificate to be entered. It can be omited, in which case the entered text must end with the '**^**' terminating character.<br>

Returns the '**>**' prompt after the Set command, after which the certificate text must be entered.



---

<br><br>
## TCP commands

**TCP** commands have the same functionality as `AT+CIPSTART`, `AT+CIPSEND` and `AT+CIPCLOSE`, but with one important difference, the two way synchronization is used in sending the received data to the host.<br>
When data is received from the remote server, ESP8266 informs the host and **waits for `receive comfirmation`**.<br>
Only after the confirmation is received, ESP8266 sends the data.<br>
After the data are processed by hosts, it sends the **`data processed confirmation`**, and only then the ESP8266 will try to accquire new data from server.

1. The ESP8266 <-> host communication is as follows:<br>
1. ESP8266 gets some data from remote server
1. ESP8266 sends the information to the host in the format: **+TCP,link_id,srv_id,data_length:**
1. Host sends the `receive comfirmation`, one character '**y**'
1. ESP8266 sends `data_lengths` data bytes
1. Host receives the data, process it and sends the `data processed confirmation`, one character '**r**'
1. ESP8266 continues to wait for new data from remote server

Instead of `receive comfirmation` (**y**), the host can also send `repeat request` '**c**' in which case ESP8266 sends the block again or `abort request` '**a**' in which case ESP8266 aborts the transmission and closes the remote connection<br>
Instead of `data processed confirmation` (**r**), the host can also send `abort request` '**a**' in which case ESP8266 aborts the transmission and closes the remote connection


All available commands **AT+TCPSTART**, **AT+TCPSEND** and **AT+TCPCLOSE** have the same syntax as the coresponding **AT+CIP...** commands in `AT+CIPMUX=1` mode, with the exception that only **"TCP"** and **"SSL"** connection types are allowed.<br>

**AT+TCPSTART=** accepts one additional paramerer at the end: **local_port**. If given, the connection is established from that local port.<br>

**AT+TCPSTART?**, **AT+TCPSEND?** or **AT+TCPCLOSE?** commands can be used to check if the TCP command are implemented. `+TCPCOMMANDS:1` is returned.<br>

<br>


## AT+TCPSERVER

This command has the same function as the _AT+CIPSERVER_ with the exception that **multiple** TCP servers can be created.<br>

When the new remote connection is accepted ESP8266 sends the message **srv_id,link_id,TCPconnect:IP_addr,port** (e.g. _1,3,TCPConnect:192.168.0.75,23638_)<br>

_**Set**_<br>

**`AT+TCPSERVER=<serv_id>,<mode>,[<port>],[<ssl>],[<maxconn>],[conn_timeout]`**

* _`srv_id`_  server id: 0 ~ 2 (up to 3 servers can be created)
* _`mode`_ 1:  creates the server; 0: deletes the server
* _`port`  port number, 333 is the default
* _`ssl`_  1: use SSL; 0: do not use SSL (default)
* _`max_con`_  maximal number of the remote connection that can be accepted (0 ~ 4); default: 4
* _`timeout`_  remote connection timeout in seconds (1 ~ 7200); default: 120


_**Query**_<br>

**`AT+TCPSERVER?`**

Report the TCPServer status:<br>

```
+TCPSERVER:servers_num
+TCPSERVER:srv_id,conn_num,max_con,port,ssl,timeout
+TCPCLIENT:link_id,IP_addr,port
```
> _+TCPSERVER:srv_id,..._ section is reported only if there are some opened servers <br>
> _+TCPCLIENT:_ section is reported only if there are some connected clients<br>


## AT+TCPSTATUS

**`AT+TCPSTATUS`** or **`AT+TCPSTATUS?`**

Reports the status in the same format as the _AT+CIPSTATUS_.<br>

**`AT+TCPSTATUS=link_id`**

Reports the status for the single connection in the following format:<br>

```
+TCPSTATUS:"remote_IP_addr",remote_port,srv_conn
```


---

<br><br>
# OTA Update


The new OTA Update procedure is developed fot this fork.

The update files can be placed on any Web server in Local network or on the Internet.<br>
The firmware expects the update files to be available in the Web server directory **ESP8266/Firmwares**.<br>
The files on the update server are as follows:<br>

| File name | Description |
| - | - |
| esp8285_AT_**a**_2.bin | Firmware binary for ESP8285 |
| esp8285_AT_**a**_2.md5 | Firmware MD5 cchecksum for the related binary file |
| esp8266_AT_**a**_**m**.bin | Firmware binary for ESP8266 |
| esp8266_AT_**a**_**m**.md5 | Firmware MD5 cchecksum for the related binary file |
| version.txt | Firmware version imformation |
| bootloader.bin | Bootloader firmware |
| bootloader.md5 | Bootloader firmware MD5 checksum |
| bootloaderver.txt | Bootloader version information |

> <br>_All files needed for update are generated by the **`build.sh`** script in the base directory and should be copyed to the server._
> <br>

Meaning of the firmware files parameters:

| Parameter | Meaning |
| - | - |
| **`m`** | SPI Flash map:<br>**2** 512+512 on 1MB flash<br>**3** 512+512 on 2MB flash<br>**4** 512+512 on 4MB flash<br>**5** 1024+1024 on 2MB flash<br>**6** 1024+1024 on 4MB flash |
| **`a`** | base firmware address in SPI Flash<br>**1** - 0x01000<br>**2** - 0x81000 |

When updating the firmware, the downloaded firmware will be checked for match with the MD5 file if it was downloaded with `AT+UPDATEGETCSUM` command.<br>
When updating the bootloader, MD5 **must** be downloaded first.<br>
It automatic restart after update is not set, you should execute **`AT+RST`** command for update to take effect.<br>


## AT+UPDATECHECK

_**Execute**_<br>
Check the firmware version available on the server. The number after _+UPDATECHECK:_ will be **1** if the new firmware is available<br>
```
AT+UPDATECHECK
+UPDATECHECK:0,"ESP8266_AT_LoBo v1.2.4","ESP8266_AT_LoBo v1.2.4"
ESP8266_AT_LoBo v1.2.4
esp8285_AT_1_2: BCCE94709FE138E4ED4E275F1911F38D
esp8285_AT_2_2: D6B9C8CE77C754746D9DC931EDF3F673
esp8266_AT_1_0: B8A170BED506B4006F5D85D409BC69FB
esp8266_AT_1_2: 7D56DB77FD637792506DC3AD4BF5E59E
esp8266_AT_2_2: 411D9D10683EDC722A95F50D4F0C4A62
esp8266_AT_1_3: 447F018B27C6BC2783A72A01D2B084B5
esp8266_AT_2_3: B373E7523E693FF4AF4872CC15BE9BF0
esp8266_AT_1_4: CC743A608879EAE1587FB66B2792A610
esp8266_AT_2_4: 76F51DAF0B68BB92749CBC3F573655BF
esp8266_AT_1_5: 935CFCDAD7B8A1E26E9156E337D40E6E
esp8266_AT_1_6: 4CEF43FC6CFF734F8D30054EA1875386

OK
```

## AT+UPDATECHECKBOOT

_**Execute**_<br>
Check the bootloader version available on the server. The number after _+UPDATECHECKBOOT:_ will be **1** if the new bootloader is available<br>
```
AT+UPDATECHECKBOOT
+UPDATECHECKBOOT:0,"LoBo ESP8266 Bootloader v1.2.0","LoBo ESP8266 Bootloader v1.2.0"

OK
```

## AT+UPDATEGETCSUM

_**Execute**_<br>
Get the firmware MD5 checksum from server.
```
AT+UPDATEGETCSUM
+UPDATEGETCSUM:1,"76F51DAF0B68BB92749CBC3F573655BF"
OK
```

_**Query**_<br>
Show the MD5 checksum downloaded from server.
```
AT+UPDATEGETCSUM?
+UPDATEGETCSUM:"76F51DAF0B68BB92749CBC3F573655BF"

OK
```

_**Set**_<br>
Delete the downloaded MD5 checksum if the parameter is `0`.
```
AT+UPDATEGETCSUM=0

OK
AT+UPDATEGETCSUM?
+UPDATEGETSCUM:""

OK
```

## AT+UPDATEGETCSUMBOOT

_**Execute**_<br>
Get the bootloader MD5 checksum from server.
```
AT+UPDATEGETCSUMBOOT
+UPDATEGETCSUMBOOT:1,"205836D305B12CBA2118A1F51BABFFC3"
OK
```

_**Query**_<br>
Show the MD5 checksum downloaded from server.
```
AT+UPDATEGETCSUMBOOT?
+UPDATEGETCSUM:"205836D305B12CBA2118A1F51BABFFC3"

OK
```

_**Set**_<br>
Delete the downloaded MD5 checksum if the parameter is `0`.
```
AT+UPDATEGETCSUMBOOT=0

OK
AT+UPDATEGETCSUMBOOT?
+UPDATEGETSCUM:""

OK
```

## AT+UPDATEDEBUG

Manage printing update debug information. When enabled, the download progress wil be printed:<br>
```
AT+UPDATEFIRMWARE
Started, please wait...
Connecting to: loboris.eu
Connected, requesting 'esp8266_AT_2_4.bin' for addr 081000
Response header received, downloading 432660 byte(s)
Download finished, received: 432660 (heap=45872)
OTA deinit
OTA finished
Checking firmware 1 at 0x081000
  Header ok.
  Check all sections ...
  OK, calculate MD5
+UPDATEFIRMWARE:1,ready for use

OK
```

_**Query**_<br>
Show the current debug state: `0` - disabled; `1` - enabled.
```
AT+UPDATEDEBUG?
+UPDATEDEBUG:0

OK
```

_**Set**_<br>

```
AT+UPDATEDEBUG=1

OK
```

## AT+UPDATEFIRMWARE

Perform the firmware update.<br>
Which firmware file to download will be determined automatically based on the SPI Flash map initially flashed into ESP8266.<br>
If the update fails, `+UPDATEFIRMWARE:0,<fail reason>` and `ERROR` will be returned.<br>

_**Test**_ command<br>
Show the possible options for the _Set_ command.<br>

```
AT+UPDATEFIRMWARE=?
+UPDATE:"<remote_host>",<reset_after>: 0|1,<force_part>: 0-7,<remote_port>: 1-65365,<ssl>: 0|1
+UPDATE:only the first parameter is mandatory

OK
```

_**Query**_ command<br>
Show the current update options.<br>

```
AT+UPDATEFIRMWARE?
+UPDATE:"loboris.eu",0,-1,0,0

OK
```

_**Set**_ command<br>
Set the update options.<br>
**`AT+UPDATEFIRMWARE="<remote_host>",<reset_after>,<force_part>,<remote_port>,<ssl>`**<br>

| Parameter | Function |
| - | - |
| `remote_host` | Update server domain or IP address |
| `reset_after` | Reset after succesfull update if set to `1`; default `0` |
| `force_part` | The Flash partition which will be updated is determined automatically.<br>If the Flash size allows for more than two OTA partitions, the desired partition number can be set by this parameter.<br>For automatic partition selection set it to `-1`. |
| `remote_port` | The dafault update server port is `80` or `443` if SSL is used.<br> If the update server runs on a different port, set it by this parameter.<br>For default port set this parameter to `0`. |
| `ssl` | Use **http** protocol for update if set to `0` (default).<br>Use **https** protocol for update if set to `1`. |

> <br>**The same options are used also for bootloader update.**

<br>

```
AT+UPDATEFIRMWARE="loboris.eu",0,-1,0,0

OK
```

_**Execute**_ command<br>

```
AT+UPDATEFIRMWARE
Started, please wait...
+UPDATEFIRMWARE:1,ready for use

OK
```

## AT+UPDATEBOOT

Perform the bootloader update.<br>
Prior to `AT+UPDATEBOOT` command, the bootloader MD5 checksum must be downloaded with `AT+UPDATEGETCSUMBOOT` or the update will fail.<br>
If the update fails, `+UPDATEBOOT:0,<fail reason>` and `ERROR` will be returned.<br>
The bootloader update is quite safe to use. The bootloader is first downloaded to RAM, checked for MD5 checksum match, and only if matched saved to SPI Flash. The only reason for it to fail can be power loss in the moment of Flash write.<br>
_**If the update fails, ESP8266 will not be able to boot!**_<br>

_**Execute**_<br>

```
AT+UPDATEDEBUG=1

OK
AT+UPDATEGETCSUMBOOT
Connecting to: loboris.eu
Connected, requesting bootloader MD5
Response header received, downloading 32 byte(s)
Download finished, received: 32 (heap=42088)
OTA deinit
OTA finished
+UPDATEGETCSUMBOOT:1,"205836D305B12CBA2118A1F51BABFFC3"
OK
OTA: Disconnected
AT+UPDATEBOOT
Started, please wait...
Connecting to: loboris.eu
Connected, requesting bootloader
Response header received, downloading 4080 byte(s)
Download finished, received: 4080 (heap=42000)
OTA deinit
OTA finished
Checking bootloader MD5 csum
+UPDATEBOOT:1

OK
```

---

## AT+BOOT

Manage the current boot configuration.<br>

_**Query**_ command<br>
Returns the current boot status and list all available boot partitions.<br>
+BOOT:`booted_partition`,`booted_flash_address`,`default_boot_partition`,`default_boot_flash_address`<br>
+BOOT:`partition_no`,`partition_address`,`flash_map`,`flash_mode`,`firmware_length`,`md5_checksum`<br>
...<br>

```
AT+BOOT?
+BOOT:0,001000,1,081000
+BOOT:0,001000,4,0,432660,CC743A608879EAE1587FB66B2792A610
+BOOT:1,081000,4,0,432660,76F51DAF0B68BB92749CBC3F573655BF

OK
```

_**Set**_ command<br>
Set the next boot partition.<br>
`AT+BOOT=part_no[,rst[,permanent]]`<br>

| Parameter | Function |
| - | - |
| `part_no` | Next boot from partition `part_no` |
| `rst` | _optional_; Reset immediately if set to `1` |
| `permanent` | _optional_; Make the selected partition the default one |

Example, booted from default partition `1`, next boot from `0`:

```
AT+BOOT?
+BOOT:1,081000,1,081000
+BOOT:0,001000,4,0,432660,CC743A608879EAE1587FB66B2792A610
+BOOT:1,081000,4,0,432660,76F51DAF0B68BB92749CBC3F573655BF

OK
AT+BOOT=0,1,0
+BOOT:RESTART NOW

OK

 ets Jan  8 2013,rst cause:2, boot mode:(3,6)

load 0x40108000, len 3056, room 16 
tail 0
chksum 0xd7
load 0x3fffb000, len 992, room 8 
tail 8
chksum 0xd7
csum 0xd7

==============================
LoBo ESP8266 Bootloader v1.2.0
==============================
   Flash map: 4, 4MB (512+512)
  Flash mode: QIO
Reset reason: Soft RESTART

Loading requested firmware (0)
  Address: 001000
Starting firmware 0, map 4 from 001000...

Boot mmap: 0,0,0

ready
WIFI CONNECTED
WIFI GOT IP
AT+BOOT?
+BOOT:0,001000,1,081000
+BOOT:0,001000,4,0,432660,CC743A608879EAE1587FB66B2792A610
+BOOT:1,081000,4,0,432660,76F51DAF0B68BB92749CBC3F573655BF

OK
```
