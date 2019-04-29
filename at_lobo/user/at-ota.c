/*
 * OTA firmware update for ESP8266
 * Copyright (c)LoBo 2019
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

#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"

#include "at-ota.h"
#include "at_custom.h"

#ifdef AT_CUSTOM_UPGRADE

#ifdef __cplusplus
extern "C" {
#endif

#define UPGRADE_FLAG_IDLE		0x00
#define UPGRADE_FLAG_START		0x01
#define UPGRADE_FLAG_FINISH		0x02
// must be the same as in bootloader
#define FW_PART_INC             0x80000
#define FW_PART_OFFSET          0x1000

#define FW_MAXSIZE              0xF0000
#define FW_MAXSIZE_512          0x70000
#define VER_MAXSIZE             2048

typedef struct {
    uint32_t flash_addr;    // SPI Flash address to update (changes during update)
    ota_callback callback;  // user callback when completed
    uint32_t total_len;     // total received length counter
    struct espconn *conn;   // espconn control block structure pointer
    ip_addr_t ip;           // update server IP address
    uint8_t *sector_buffer; // buffer for flash sector write
    uint32_t sector_idx;    // sector buffer index
    uint8_t error;          // the operation status
} upgrade_status;

// Global variables
uint8_t upgrade_debug = 0;                  // Print debug information if set to 1
uint8_t upgrade_flash_map = 0;              // Flash map type used for requesting the firmware
uint8_t upgrade_type = 0;                   // 0: perform upgrade, 1: only version check is performed
uint8_t upgrade_use_ssl = 0;                // use SSL connection is set to 1
uint32_t upgrade_flash_addr = 0xF000000;    // SPI Flash address to update
char *upgrade_remote_host = "";             // Upgrade server IP address or domain name
uint16_t upgrade_remote_port = 0;           // upgrade server port, if 0 default is used
uint8_t *upgrade_check_response = NULL;
uint32_t upgrade_content_len = 0;

static upgrade_status *upgrade;
static os_timer_t ota_timer;
static upgrade_flag = 0;

// clean up at the end of the update
// will call the user call back to indicate completion
//------------------------------------
void ICACHE_FLASH_ATTR at_ota_deinit()
{
    // save only remaining bits of interest from upgrade struct
    // then we can clean it up early, so disconnect callback
    // can distinguish between us calling it after update finished
    // or being called earlier in the update process
    bool result;
    ota_callback callback = upgrade->callback;
    struct espconn *conn = upgrade->conn;

    os_timer_disarm(&ota_timer);

    if (upgrade_debug) {
        at_port_print_irom_str("OTA deinit\r\n");
    }

    if (upgrade_check_response != NULL) os_free(upgrade_check_response);
    upgrade_check_response = NULL;

    // prepare the response buffer if it was a version check
    if ((upgrade_flag == UPGRADE_FLAG_FINISH) && (callback) && (upgrade->sector_idx > 0)) {
        result = false;
        if (((upgrade_type == OTA_TYPE_VERCHECK) || (upgrade_type == OTA_TYPE_VERCHECKBOOT)) &&
            (upgrade->sector_idx < VER_MAXSIZE)) result = true;
        else if (((upgrade_type == OTA_TYPE_MD5) || (upgrade_type == OTA_TYPE_MD5BOOT)) &&
                 (upgrade->sector_idx == 32)) result = true;
        else if ((upgrade_type == OTA_TYPE_UPGBOOT) && (upgrade->sector_idx == upgrade_content_len)) result = true;

        if (result) {
            upgrade_check_response = (uint8_t *)os_zalloc(upgrade->sector_idx+2);
            if (upgrade_check_response) {
                os_memcpy(upgrade_check_response, upgrade->sector_buffer, upgrade->sector_idx);
                upgrade_check_response[upgrade->sector_idx] = 0;
            }
            else {
                if (upgrade_debug) {
                    at_port_print_irom_str("Failed to create response buffer, no RAM\r\n");
                }
            }
        }
    }

    // clean up
    os_free(upgrade->sector_buffer);
    os_free(upgrade);
    upgrade = 0;

    // if connected, disconnect and clean up connection
    if (conn) {
        if (upgrade_use_ssl > 0) espconn_secure_disconnect(conn);
        else espconn_disconnect(conn);
    }

    // check for completion
    if (upgrade_flag == UPGRADE_FLAG_FINISH) result = true;
    else {
        upgrade_flag = UPGRADE_FLAG_IDLE;
        result = false;
    }

    if (upgrade_debug) {
        at_port_print_irom_str("OTA finished\r\n");
    }
    // call user call back
    if (callback) {
        callback(result);
    }

}

// write received data to the sector buffer
// when the sector buffer is full, write it to the flash
//-----------------------------------------------------------------------------
static bool ICACHE_FLASH_ATTR _write_data(uint8_t *data, unsigned short length)
{
    char info[80] = {'\0'};
    if (length == 0) return true;
    if (upgrade_type >= OTA_TYPE_MAX) return true;

    // copy data to sector buffer
    if (length >= (SECTOR_SIZE - upgrade->sector_idx)) {
        if (upgrade_type == OTA_TYPE_UPGRADE) {
            // sector buffer full, write to flash
            uint32_t in_buf = SECTOR_SIZE - upgrade->sector_idx;
            uint32_t remain = length - in_buf;
            os_memcpy(upgrade->sector_buffer + upgrade->sector_idx, data, in_buf);

            // write sector to flash
            if (upgrade->flash_addr > (upgrade_flash_addr + upgrade_content_len)) {
                if (upgrade_debug) {
                    at_port_print_irom_str("Flash address exceeds firmware length\r\n");
                }
                return false;
            }
            if (spi_flash_erase_sector(upgrade->flash_addr / SECTOR_SIZE)) goto exit_err;
            if (spi_flash_write(upgrade->flash_addr, (uint32_t *)upgrade->sector_buffer, SECTOR_SIZE)) goto exit_err;
            upgrade->flash_addr += SECTOR_SIZE;
            // copy remaining bytes to sector buffer
            if (remain) os_memcpy(upgrade->sector_buffer, data + in_buf, remain);
            upgrade->sector_idx = remain;
        }
        else {
            // If it is not the firmware update, just ignore the data
        }
    }
    else {
        if (upgrade_type == OTA_TYPE_UPGRADE) {
            // receiving firmware
            os_memcpy(upgrade->sector_buffer + upgrade->sector_idx, (uint8_t*)data, length);
            upgrade->sector_idx += length;
        }
        else if (upgrade_type == OTA_TYPE_UPGBOOT) {
            // receiving boot sector
            if (length < (SECTOR_SIZE - upgrade->sector_idx)) {
                os_memcpy(upgrade->sector_buffer + upgrade->sector_idx, (uint8_t*)data, length);
                upgrade->sector_idx += length;
            }
        }
        else if ((upgrade_type < OTA_TYPE_MAX) && (length < (SECTOR_SIZE - upgrade->sector_idx))) {
            // receiving version or MD5
            os_memcpy(upgrade->sector_buffer + upgrade->sector_idx, (uint8_t*)data, length);
            upgrade->sector_idx += length;
        }
    }
    return true;

exit_err:
    if (upgrade_debug) {
        at_port_print_irom_str("Flash write error\r\n");
    }
    return false;
}

// called when connection receives data
//--------------------------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR upgrade_recvcb(void *arg, char *pusrdata, unsigned short length)
{
    char *ptrData, *ptrLen, *ptr;
    char info[80] = {'\0'};

    // disarm the timer
    os_timer_disarm(&ota_timer);

    // first reply (response header)?
    if (upgrade_content_len == 0) {
        // valid http response?
        if ((ptrLen = (char*)os_strstr(pusrdata, "Content-Length: "))
            && (ptrData = (char*)os_strstr(ptrLen, "\r\n\r\n"))
            && (os_strncmp(pusrdata + 9, "200", 3) == 0)) {

            // end of header/start of data
            ptrData += 4;
            // length of data AFTRE header in this chunk (content data)
            length -= (ptrData - pusrdata);
            // update running total of download length
            upgrade->total_len += length;
            // write content data to the sector buffer
            if (!_write_data((uint8_t *)ptrData, length)) {
                at_ota_deinit();
                return;
            }
            // work out total download size
            ptrLen += 16;
            ptr = (char *)os_strstr(ptrLen, "\r\n");
            *ptr = '\0'; // destructive
            upgrade_content_len = atoi(ptrLen);
            // Check maximum allowed file size
            if ((upgrade_content_len >= FW_MAXSIZE) ||
                (((upgrade_flash_map>>4) < 5) && (upgrade_content_len >= FW_MAXSIZE)) ||
                (((upgrade_type == OTA_TYPE_VERCHECK) ||(upgrade_type == OTA_TYPE_VERCHECKBOOT)) && (upgrade_content_len >= VER_MAXSIZE)) ||
                ((upgrade_type == OTA_TYPE_UPGBOOT) && (upgrade_content_len >= SECTOR_SIZE)) ||
                (((upgrade_type == OTA_TYPE_MD5) || (upgrade_type == OTA_TYPE_MD5BOOT)) && (upgrade_content_len != 32))) {
                if (upgrade_debug) {
                    at_port_print_irom_str("Content length error\r\n");
                }
                upgrade->error = OTA_STATUS_FW_TO_BIG;
                at_ota_deinit();
                return;
            }
            if (upgrade_debug) {
                os_sprintf(info, "Response header received, downloading %d byte(s)\r\n", upgrade_content_len);
                at_port_print(info);
            }
        }
        else {
            // fail, not a valid http header/non-200 response/etc.
            if (upgrade_debug) {
                at_port_print_irom_str("Not a valid http header/non-200 response\r\n");
            }
            upgrade->error = OTA_STATUS_NO_VALID_RESPONSE;
            at_ota_deinit();
            return;
        }
    }
    else {
        // not the first chunk, process it
        upgrade->total_len += length;
        // write received data to the sector buffer
        if (!_write_data((uint8_t *)pusrdata, length)) {
            upgrade->error = OTA_STATUS_FLASH_WRITE_ERROR;
            at_ota_deinit();
            return;
        }
    }

    // check if we are finished
    if (upgrade->total_len == upgrade_content_len) {
        if ((upgrade->sector_idx > 0) && (upgrade_type == OTA_TYPE_UPGRADE)) {
            // write remaining data in sector buffer
            // write buffer to flash
            if (spi_flash_erase_sector(upgrade->flash_addr / SECTOR_SIZE)) {
                upgrade->error = OTA_STATUS_FLASH_WRITE_ERROR;
                at_ota_deinit();
                return;
            }
            if (spi_flash_write(upgrade->flash_addr, (uint32_t *)upgrade->sector_buffer, upgrade->sector_idx)) {
                upgrade->error = OTA_STATUS_FLASH_WRITE_ERROR;
                at_ota_deinit();
                return;
            }
        }
        upgrade_flag = UPGRADE_FLAG_FINISH;
        upgrade->error = OTA_STATUS_OK;
        // Finished, clean up and call user callback
        if (upgrade_debug) {
            os_sprintf(info, "Download finished, received: %d (heap=%d)\r\n", upgrade->total_len, system_get_free_heap_size());
            at_port_print(info);
        }
        at_ota_deinit();
    }
    else if ((upgrade->conn->state != ESPCONN_READ) || (upgrade->total_len > upgrade_content_len)) {
        // fail, but how do we get here? premature end of stream?
        if (upgrade_debug) {
            at_port_print_irom_str("Download failed\r\n");
        }
        upgrade->error = OTA_STATUS_ESPCONN_ERROR;
        at_ota_deinit();
    }
    else {
        // timer for next receive
        os_timer_setfn(&ota_timer, (os_timer_func_t *)at_ota_deinit, 0);
        os_timer_arm(&ota_timer, OTA_NETWORK_TIMEOUT, 0);
    }
}

// disconnect callback, clean up the connection
// we also call this ourselves
//-------------------------------------------------------
static void ICACHE_FLASH_ATTR upgrade_disconcb(void *arg)
{
    // use passed ptr, as upgrade struct may have gone by now
    if (upgrade_debug) {
        at_port_print_irom_str("OTA: Disconnected\r\n");
    }
    struct espconn *conn = (struct espconn*)arg;

    os_timer_disarm(&ota_timer);
    if (conn) {
        if (conn->proto.tcp) {
            os_free(conn->proto.tcp);
        }
        os_free(conn);
    }

    // is upgrade struct still around?
    // if so disconnect was from remote end, or we called
    // ourselves to cleanup a failed connection attempt
    // must ensure disconnect was for this upgrade attempt,
    // not a previous one! this call back is async so another
    // upgrade struct may have been created already
    if (upgrade && (upgrade->conn == conn)) {
        // mark connection as gone
        upgrade->conn = 0;
        // end the update process
        if (upgrade_debug) {
            at_port_print_irom_str("OTA: Disconnected from remote\r\n");
        }
        at_ota_deinit();
    }
}

// successfully connected to update server, send the request
//---------------------------------------------------------
static void ICACHE_FLASH_ATTR upgrade_connect_cb(void *arg)
{
    uint8_t *request;
    char esp_name[8] = {'\0'};
    char ext[4] = {'\0'};
    uint8_t user_n = (((upgrade_flash_addr / FW_PART_INC) % 2) == 0) ? 1 : 2;

    // disable the timeout
    os_timer_disarm(&ota_timer);

    if (((upgrade_flash_map >> 4) == 2) && ((upgrade_flash_map & 0x0F) == 3)) os_sprintf(esp_name, "esp8285");
    else os_sprintf(esp_name, "esp8266");
    if (upgrade_type == OTA_TYPE_MD5) os_sprintf(ext, "md5");
    else if (upgrade_type == OTA_TYPE_UPGRADE) os_sprintf(ext, "bin");

    // register connection callbacks
    espconn_regist_disconcb(upgrade->conn, upgrade_disconcb);
    espconn_regist_recvcb(upgrade->conn, upgrade_recvcb);

    // http request string
    request = (uint8_t *)os_zalloc(512);
    if (!request) {
        if (upgrade_debug) {
            at_port_print_irom_str("No RAM for request string!\r\n");
        }
        upgrade->error = OTA_STATUR_NO_RAM;
        at_ota_deinit();
        return;
    }

    if (upgrade_type == OTA_TYPE_VERCHECK) {
        if (upgrade_debug) {
            at_port_print_irom_str("Connected, version check\r\n");
        }
        os_sprintf(request, "GET /ESP8266/firmwares/version.txt HTTP/1.0\r\nHost: %s\r\n"HTTP_HEADER, upgrade_remote_host);
    }
    else if (upgrade_type == OTA_TYPE_VERCHECKBOOT) {
        if (upgrade_debug) {
            at_port_print_irom_str("Connected, boot version check\r\n");
        }
        os_sprintf(request, "GET /ESP8266/firmwares/bootloaderver.txt HTTP/1.0\r\nHost: %s\r\n"HTTP_HEADER, upgrade_remote_host);
    }
    else if (upgrade_type == OTA_TYPE_MD5BOOT) {
        if (upgrade_debug) {
            at_port_print_irom_str("Connected, requesting bootloader MD5\r\n");
        }
        os_sprintf(request, "GET /ESP8266/firmwares/bootloader.md5 HTTP/1.0\r\nHost: %s\r\n"HTTP_HEADER, upgrade_remote_host);
    }
    else if (upgrade_type == OTA_TYPE_UPGBOOT) {
        if (upgrade_debug) {
            at_port_print_irom_str("Connected, requesting bootloader\r\n");
        }
        os_sprintf(request, "GET /ESP8266/firmwares/bootloader.bin HTTP/1.0\r\nHost: %s\r\n"HTTP_HEADER, upgrade_remote_host);
    }
    else {
        if (upgrade_debug) {
            os_sprintf(request, "Connected, requesting '%s_AT_%d_%d.%s' for addr %06X\r\n",
                    esp_name, user_n, upgrade_flash_map >> 4, ext, upgrade_flash_addr);
            at_port_print(request);
        }
        os_sprintf(request, "GET /ESP8266/firmwares/%s_AT_%d_%d.%s HTTP/1.0\r\nHost: %s\r\n"HTTP_HEADER,
                esp_name, user_n, upgrade_flash_map >> 4, ext, upgrade_remote_host);
    }

    // send the http request, with timeout for reply
    os_timer_setfn(&ota_timer, (os_timer_func_t *)at_ota_deinit, 0);
    os_timer_arm(&ota_timer, OTA_NETWORK_TIMEOUT, 0);
    if (upgrade_use_ssl > 0) espconn_secure_send(upgrade->conn, request, os_strlen((char*)request));
    else espconn_send(upgrade->conn, request, os_strlen((char*)request));
    os_free(request);
}

// connection attempt timed out
//------------------------------------------------
static void ICACHE_FLASH_ATTR connect_timeout_cb()
{
    if (upgrade_debug) {
        at_port_print_irom_str("Connect timeout.\r\n");
    }
    // not connected so don't call disconnect on the connection
    // but call our own disconnect callback to do the cleanup
    if (upgrade) upgrade_disconcb(upgrade->conn);
}

// call back for lost connection
//----------------------------------------------------------------------
static void ICACHE_FLASH_ATTR upgrade_recon_cb(void *arg, sint8 errType)
{
    if (upgrade_debug) {
        char buf[64] = {0};
        os_sprintf(buf, "Connection error (%d)\r\n", errType);
        at_port_print(buf);
    }

    // not connected so don't call disconnect on the connection
    // but call our own disconnect callback to do the cleanup
    if (upgrade) upgrade_disconcb(upgrade->conn);
}

// call back for dns lookup
//----------------------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR upgrade_resolved(const char *name, ip_addr_t *ip, void *arg)
{
    if (ip == 0) {
        if (upgrade_debug) {
            at_port_print_irom_str("DNS lookup failed for: ");
            at_port_print(upgrade_remote_host);
            at_port_print_irom_str("\r\n");
        }
        // not connected so don't call disconnect on the connection
        // but call our own disconnect callback to do the cleanup
        upgrade->error = OTA_STATUS_DNS_FAILED;
        upgrade_disconcb(upgrade->conn);
        return;
    }

    // set up connection
    upgrade->conn->type = ESPCONN_TCP;
    upgrade->conn->state = ESPCONN_NONE;
    upgrade->conn->proto.tcp->local_port = espconn_port();
    if (upgrade_remote_port > 0) upgrade->conn->proto.tcp->remote_port = upgrade_remote_port;
    else if (upgrade_use_ssl > 0) upgrade->conn->proto.tcp->remote_port = 443;
    else upgrade->conn->proto.tcp->remote_port = 80;
    *(ip_addr_t*)upgrade->conn->proto.tcp->remote_ip = *ip;

    espconn_set_opt(upgrade->conn, 0x0B);
    // set connection callbacks
    espconn_regist_connectcb(upgrade->conn, upgrade_connect_cb);
    espconn_regist_reconcb(upgrade->conn, upgrade_recon_cb);

    if (upgrade_debug) {
        at_port_print_irom_str("Connecting to: ");
        at_port_print(upgrade_remote_host);
        if (upgrade_use_ssl > 0) at_port_print_irom_str(" (SSL)");
        at_port_print_irom_str("\r\n");
    }
    // try to connect
    if (upgrade_use_ssl > 0) {

        espconn_secure_connect(upgrade->conn);
    }
    else espconn_connect(upgrade->conn);

    // set connection timeout timer
    os_timer_disarm(&ota_timer);
    os_timer_setfn(&ota_timer, (os_timer_func_t *)connect_timeout_cb, 0);
    os_timer_arm(&ota_timer, OTA_NETWORK_TIMEOUT, 0);
}

// start the ota process, with user supplied options
//--------------------------------------------------------
bool ICACHE_FLASH_ATTR at_ota_start(ota_callback callback)
{
    uint8_t slot;
    err_t result;

    // check if not already updating
    if (upgrade_flag == UPGRADE_FLAG_START) {
        if (upgrade_debug) {
            at_port_print_irom_str("Already started !\r\n");
        }
        return false;
    }

    // create upgrade status structure
    upgrade = (upgrade_status*)os_zalloc(sizeof(upgrade_status));
    if (!upgrade) {
        if (upgrade_debug) {
            at_port_print_irom_str("No ram for upgrade structure!\r\n");
        }
        return false;
    }

    // store the callback
    upgrade->callback = callback;

    // create sector sector_buffer
    upgrade->sector_buffer = (uint8_t *)os_zalloc(SECTOR_SIZE);
    if (!upgrade->sector_buffer) {
        if (upgrade_debug) {
            at_port_print_irom_str("No ram for sector buffer!\r\n");
        }
        os_free(upgrade);
        return false;
    }
    upgrade->sector_idx = 0;

    // create connection
    upgrade->conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
    if (!upgrade->conn) {
        if (upgrade_debug) {
            at_port_print_irom_str("No ram for espconn structure!\r\n");
        }
        os_free(upgrade->sector_buffer);
        os_free(upgrade);
        return false;
    }
    upgrade->conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    if (!upgrade->conn->proto.tcp) {
        if (upgrade_debug) {
            at_port_print_irom_str("No ram protocol structure!\r\n");
        }
        os_free(upgrade->conn);
        os_free(upgrade->sector_buffer);
        os_free(upgrade);
        return false;
    }

    upgrade->flash_addr = upgrade_flash_addr;
    upgrade->error = OTA_STATUS_OK;
    upgrade_check_response = NULL;
    upgrade_flag = UPGRADE_FLAG_START;
    upgrade_content_len = 0;

    // DNS lookup
    result = espconn_gethostbyname(upgrade->conn, upgrade_remote_host, &upgrade->ip, upgrade_resolved);
    if (result == ESPCONN_OK) {
        // host name is already cached or is actually a dotted decimal IP address
        upgrade_resolved(0, &upgrade->ip, upgrade->conn);
    } else if (result == ESPCONN_INPROGRESS) {
        // lookup taking place, will call upgrade_resolved on completion
    } else {
        if (upgrade_debug) {
            at_port_print_irom_str("DNS error!\r\n");
        }
        os_free(upgrade->conn->proto.tcp);
        os_free(upgrade->conn);
        os_free(upgrade->sector_buffer);
        os_free(upgrade);
        return false;
    }

    return true;
}

#ifdef __cplusplus
}
#endif

#endif
