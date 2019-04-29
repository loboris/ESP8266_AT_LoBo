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

#ifndef __AT_OTA_H__
#define __AT_OTA_H__

#ifdef AT_CUSTOM_UPGRADE

#ifdef __cplusplus
extern "C" {
#endif

/*
 * `Accept-Encoding: identity` requests from server not to use compression
 * It is important when downloading the textual information
 * like version info or MD5 checksum
*/
// --- general http header ---------------------
#define HTTP_HEADER "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: ESP8266/Update client/1.2 \r\n\
Accept: */*\r\n\
Accept-Encoding: identity\r\n\r\n"
// ---------------------------------------------

// timeout for the initial connect and each receive callback (in ms)
#define OTA_NETWORK_TIMEOUT  10000

enum {
    OTA_STATUS_OK = 0,
    OTA_STATUS_FW_TO_BIG,
    OTA_STATUS_NO_VALID_RESPONSE,
    OTA_STATUS_FLASH_WRITE_ERROR,
    OTA_STATUS_ESPCONN_ERROR,
    OTA_STATUR_NO_RAM,
    OTA_STATUS_ESPCONN_DISCONECTED,
    OTA_STATUS_TIMEOUT,
    OTA_STATUS_DNS_FAILED,
};

enum {
    OTA_TYPE_UPGRADE = 0,
    OTA_TYPE_VERCHECK,
    OTA_TYPE_VERCHECKBOOT,
    OTA_TYPE_MD5,
    OTA_TYPE_MD5BOOT,
    OTA_TYPE_UPGBOOT,
    OTA_TYPE_MAX,
};

// callback method should take this format
typedef void (*ota_callback)(bool result);

extern uint8_t upgrade_debug;          // Print debug information if set to 1
extern uint8_t upgrade_flash_map;      // Flash map type used for requesting the firmware
extern uint8_t upgrade_type;           // 0: perform upgrade, 1: only version check is performed
extern uint8_t upgrade_use_ssl;        // use ssl connection is set to 1
extern uint32_t upgrade_flash_addr;    // SPI Flash address to update
extern char *upgrade_remote_host;      // Upgrade server IP address or domain name
extern uint16_t upgrade_remote_port;   // upgrade server port, if 0 default is used
extern uint8_t *upgrade_check_response;
extern uint32_t upgrade_content_len;

// function to perform the ota update
bool ICACHE_FLASH_ATTR at_ota_start(ota_callback callback);

#ifdef __cplusplus
}
#endif

#endif

#endif
