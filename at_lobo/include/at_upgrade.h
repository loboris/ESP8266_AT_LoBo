/*
 * OTA firmware update for ESP8266
 * Copyright LoBo 2019
 * 
 * Based on rBoot OTA sample code for ESP8266 by Richard A Burton
*/

/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __AT_UPGRADE_H__
#define __AT_UPGRADE_H__

#ifdef AT_CUSTOM_UPGRADE

// must be the same as in bootloader
#define BOOT_FW_MAGIC           0x5AA5C390
#define HEADER_MAGIC            0xEA
#define SECTION_MAGIC           0xE9
#define MAX_APP_PART            8
#define FW_PART_INC             0x80000
#define FW_PART_OFFSET          0x1000
#define CHECKSUM_INIT           0xEF
#define RTC_USER_ADDR           0x60001140

typedef struct {
    uint8 md5[16];
}md5_t;

typedef struct {
    uint32  boot_part;                  // active boot partition (0=0x1000; 1=0x81000; 2=0x101000; ...)
    uint32  boot_addr;                  // flash addres of the active boot partition
    uint32  part_length[MAX_APP_PART];  // length of the partition firmware
    md5_t   part_md5[MAX_APP_PART];     // MD5 checksum of the partition firmware
    uint8_t part_type[MAX_APP_PART];    // Firmware type: 1: 512K, 2: 1024K, 0: unknown
}boot_info_t;


extern boot_info_t *boot_info;

void free_boot_info();
bool read_boot_info();
int32_t get_rtc_curr_fw();
uint32_t get_rtc_curr_fw_addr();

void at_exeCmdFWupdate(uint8_t id);
void at_exeCmdFWupdateBoot(uint8_t id);
void at_queryCmdFWupdate(uint8_t id);
void at_setupCmdFWupdate(uint8_t id, char *pPara);
void at_testCmdFWupdate(uint8_t id);

void at_exeCmdFWVerCheck(uint8_t id);
void at_exeCmdFWVerCheckBoot(uint8_t id);

void at_exeCmdFWGetMD5(uint8_t id);
void at_exeCmdFWGetMD5boot(uint8_t id);
void at_queryCmdFWGetMD5(uint8_t id);
void at_setupCmdFWGetMD5(uint8_t id, char *pPara);

void at_queryCmdFWDebug(uint8_t id);
void at_setupCmdFWDebug(uint8_t id, char *pPara);

void at_setupCmdBoot(uint8_t id, char *pPara);
void at_queryCmdBoot(uint8_t id);

#endif

#endif
