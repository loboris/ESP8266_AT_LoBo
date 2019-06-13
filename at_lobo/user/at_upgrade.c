/*
 * OTA firmware update for ESP8266
 * Copyright LoBo 2019
 * 
 * 04/2019
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
#include "at_custom.h"
#include "at-ota.h"
#include "at_upgrade.h"

#ifdef AT_CUSTOM_UPGRADE

#define REMOTE_UPDATE_HOST      "loboris.eu"

/*
 * Data at RTC_USER_ADDR (all data are uint32_t, 4 byte alligned):
 * RTC_USER_ADDR +  0:   User requested partition to boot from, BOOT_FW_MAGIC + part_no
 * RTC_USER_ADDR +  4:   Currently booted partition, BOOT_FW_MAGIC + part_no
 * RTC_USER_ADDR +  8:   Currently booted partition SPI flash address, 0x1000, 0x81000, 0x101000, 0x181000, ...
 * RTC_USER_ADDR + 12:   Boot counter
 * RTC_USER_ADDR + 16:   Bootloader version
*/

typedef struct {
    uint8 magic;
    uint8 count;
    uint8 flags1;
    uint8 flags2;
    uint32 entry;
}binary_header_t;

typedef struct {
    uint32 address;
    uint32 length;
}section_header_t;

struct MD5Context
{
    uint32_t buf[4];
    uint32_t bits[2];
    uint8_t in[64];
};

extern void MD5Init(struct MD5Context *ctx);
extern void MD5Update(struct MD5Context *ctx, void *buf, uint32_t len);
extern void MD5Final(uint8_t digest[16], struct MD5Context *ctx);

static uint8_t update_reset = 0;
static int8_t update_forced_fw = -1;
static uint8_t update_md5[36] = {0};

boot_info_t *boot_info;


//--------------------------------------------------------------
LOCAL void ICACHE_FLASH_ATTR set_rtc_nextboot(uint8_t boot_part)
{
    volatile uint32_t *rtc_userdata = (uint32_t*)RTC_USER_ADDR;
    if (boot_part >= MAX_APP_PART) *rtc_userdata = 0;
    else *rtc_userdata = BOOT_FW_MAGIC | (boot_part & 0x07);
}

//-----------------------------------------
int32_t ICACHE_FLASH_ATTR get_rtc_curr_fw()
{
    volatile uint32_t *rtc_cfw = (uint32_t*)RTC_USER_ADDR + 4;
    uint32_t curr_fw = *(rtc_cfw);
    if ((curr_fw >= BOOT_FW_MAGIC) & (curr_fw < (BOOT_FW_MAGIC + MAX_APP_PART))) {
        return (int32_t)(curr_fw & 0x0F);
    }
    return -1;
}

//-----------------------------------------------
uint32_t ICACHE_FLASH_ATTR get_rtc_curr_fw_addr()
{
    if (get_rtc_curr_fw() < 0) return 0;
    volatile uint32_t *rtc_caddr = (uint32_t*)RTC_USER_ADDR + 8;
    return *(rtc_caddr);
}

//----------------------------------------------
int32_t ICACHE_FLASH_ATTR get_rtc_bootver_addr()
{
    volatile uint32_t *rtc_cboot = (uint32_t*)RTC_USER_ADDR + 16;
    uint32_t curr_boot = *(rtc_cboot);
    if ((curr_boot & 0xFF000000) == 0xA5000000) {
        return (int32_t)(curr_boot & 0x00FFFFFF);
    }
    return -1;
}

//-------------------------------------
void ICACHE_FLASH_ATTR free_boot_info()
{
    if (boot_info) {
        os_free(boot_info);
        boot_info = NULL;
    }
}

//-------------------------------------
bool ICACHE_FLASH_ATTR read_boot_info()
{
    boot_info = (boot_info_t *)os_zalloc(sizeof(boot_info_t));
    if (!boot_info) return false;

    // Read the current boot config
    if (spi_flash_read(SYSTEM_PARTITION_BOOT_PARAMETER_ADDR, (uint32_t *)&boot_info->boot_part, sizeof(boot_info_t))) {
        os_free(boot_info);
        boot_info = NULL;
        return false;
    }
    return true;
}

// Check the firmware at given flash address
// Return the address to the firmware entry point if good
//------------------------------------------------------------
LOCAL uint32_t ICACHE_FLASH_ATTR check_firmware(uint8_t app_n)
{
    uint32_t flash_addr = upgrade_flash_addr;
    uint32_t i, j;
    uint32_t remaining;
    uint32_t readlen;
    binary_header_t header;
    section_header_t section;
    uint8_t buffer[256] = {0};
    uint8_t checksum;
    uint8_t flash_map;
    uint8_t flash_mode;
    struct MD5Context context;
    uint8_t digest[16] = {0};
    uint32_t total;
    uint32_t flash_end;
    uint32_t flash_start = flash_addr;

    //read the first header
    if (spi_flash_read(flash_addr, (uint32_t *)&header, sizeof(binary_header_t))) goto exit_err;

    //check the header magic and number of sections
    if ((header.magic!=HEADER_MAGIC) || (header.count==0)) {
        if (upgrade_debug) {
            at_port_print_irom_str("  Header error\r\n");
        }
        return 0;
    }
    if (upgrade_debug) {
        at_port_print_irom_str("  Header ok.\r\n");
    }

    flash_addr += sizeof(binary_header_t);

    //ignore the section ROM0 jump to the end of this section
    if (spi_flash_read(flash_addr, (uint32_t *)&section, sizeof(section_header_t))) goto exit_err;

    flash_addr += sizeof(section_header_t);
    flash_addr += section.length;

    //read the second header
    if (spi_flash_read(flash_addr, (uint32_t *)&header, sizeof(binary_header_t))) goto exit_err;

    //check the header magic and number of sections
    if ((header.magic!=SECTION_MAGIC) || (header.count==0)) {
        if (upgrade_debug) {
            at_port_print_irom_str("  Section error\r\n");
        }
        return 0;
    }

    flash_map = header.flags2 & 0xF0;
    flash_mode = header.flags1 & 0x0F;
    if ((flash_map | flash_mode) != upgrade_flash_map) {
        if (upgrade_debug) {
            at_port_print_irom_str("  Flash map not as expected.\r\n");
            return 0;
        }
    }

    flash_addr += sizeof(binary_header_t);
    checksum = CHECKSUM_INIT;

    if (upgrade_debug) {
        at_port_print_irom_str("  Check all sections ");
    }
    //calculate the checksum of all sections
    for(i=0; i<header.count; i++)
    {
        if (upgrade_debug) {
            at_port_print_irom_str(".");
        }
        //read section header
        if (spi_flash_read(flash_addr, (uint32_t *)&section, sizeof(section_header_t))) goto exit_err;
        
        flash_addr += sizeof(section_header_t);
        remaining = section.length;

        //calculate the checksum
        while (remaining > 0) {
            //read the len of the buffer
            readlen = (remaining < sizeof(buffer)) ? remaining:sizeof(buffer);
            
            //read to the local buffer
            if (spi_flash_read(flash_addr, (uint32_t *)buffer, readlen)) goto exit_err;

            //add to chksum
            for(j=0; j<readlen; j++)
                checksum ^= buffer[j];
            
            //update the controls
            flash_addr += readlen;
            remaining -= readlen;
        }
    }
    if (upgrade_debug) {
        at_port_print_irom_str("\r\n");
    }

    //read the last byte of the binary
    //that contains the checksum byte
    flash_addr = (flash_addr | 0xF) - 3;
    if (spi_flash_read(flash_addr, (uint32_t *)buffer, 4) != 0) goto exit_err;

    if (buffer[3] != checksum) {
        if (upgrade_debug) {
            at_port_print_irom_str("  Checksum error\r\n");
        }
        return 0;
    }

    // ==== Everything checked and OK! ====

    // -------------------------------------------------
    // Calculate MD5 checksum of the firmware flash area
    // -------------------------------------------------
    if (upgrade_debug) {
        at_port_print_irom_str("  OK, calculate MD5\r\n");
    }

    // Read the current boot config
    if (!read_boot_info()) {
        if (upgrade_debug) {
            at_port_print_irom_str("  Error reading boot config sector\r\n");
        }
        return 0;
    }

    flash_end = flash_addr + 8; // there are 4 more bytes after the checksum byte
    total = 0;
    flash_addr = flash_start;
    
    MD5Init(&context);
    remaining = flash_end-flash_start;

    while (remaining)
    {
        readlen = (remaining >= 256) ? 256 : remaining;
        if (spi_flash_read(flash_addr, (uint32_t *)buffer, readlen) != 0) goto exit_err;
        MD5Update(&context, (uint32_t *)buffer, readlen);
        flash_addr += readlen;
        total += readlen;
        remaining -= readlen;
    }
    MD5Final(digest, &context);

    if (os_strlen(update_md5) == 32) {
        // We have the MD5 checksum which must match the calculated one
        for (i=0; i<16; i++) {
            os_sprintf(buffer + (i*2), "%02X", digest[i]);
        }
        buffer[32] = 0;
        if (os_memcmp(update_md5, buffer, 32) != 0) {
            free_boot_info();
            if (upgrade_debug) {
                at_port_print_irom_str("  MD5 checksum not matched\r\n");
            }
            return 0;
        }
    }
    // Save new firmware data
    boot_info->boot_addr = flash_start;
    boot_info->boot_part = (BOOT_FW_MAGIC | app_n);
    boot_info->part_length[app_n] = total;
    boot_info->part_type[app_n] = flash_map | flash_mode;
    os_memcpy(boot_info->part_md5[app_n].md5, digest, 16);

    flash_addr = SYSTEM_PARTITION_BOOT_PARAMETER_ADDR;
    if (spi_flash_erase_sector(SYSTEM_PARTITION_BOOT_PARAMETER_ADDR / SECTOR_SIZE)) goto exit_err;
    if (spi_flash_write(SYSTEM_PARTITION_BOOT_PARAMETER_ADDR, (uint32_t *)&boot_info->boot_part, sizeof(boot_info_t))) goto exit_err;
    
    free_boot_info();
    return header.entry;

exit_err:
    free_boot_info();
    if (upgrade_debug) {
        os_sprintf(buffer, "  SPI Flash error (%06X)\r\n", flash_addr);
        at_port_print(buffer);
    }

    return 0;
}

//----------------------------------------------
static bool ICACHE_FLASH_ATTR update_setparams()
{
    uint32_t fw_addr = 0;
    uint32_t fw_ok = 0;

    // Read the boot configuration
    if (!read_boot_info()) return false;

    // Get the current partition info
    uint32_t fw_n = get_rtc_curr_fw();
    uint8_t flash_map = boot_info->part_type[fw_n] >> 4;
    uint8_t flash_mode = boot_info->part_type[fw_n] & 0x0F;

    if (update_forced_fw >= 0) {
        // user requested update to the specified partition
        if (update_forced_fw != fw_n) {
            fw_n = update_forced_fw;
            update_forced_fw = -1;
            if (flash_map < 5) {
                // 512+512 firmwares can be located at half MB addresses
                fw_addr = (fw_n * FW_PART_INC) + FW_PART_OFFSET;
            }
            else {
                // 1024+1024 firmwares can only be at 1MB addresses
                fw_addr = (fw_n * FW_PART_INC *2) + FW_PART_OFFSET;
            }
        }
        else {
            free_boot_info();
            return false;
        }
    }
    else {
        // Auto determine which partition to update
        if (flash_map < 5) {
            // 512+512 firmware, alternate between 0 and 1
            if (fw_n == 0) fw_n = 1;
            else fw_n = 0;
        }
        else {
            // 1024+1024 firmware, alternate between 0 and 2
            if (fw_n == 0) fw_n = 2;
            else fw_n = 0;
        }
        fw_addr = (fw_n * FW_PART_INC) + FW_PART_OFFSET;
    }

    if (os_strlen(upgrade_remote_host) == 0) {
        os_sprintf(upgrade_remote_host, "%s", REMOTE_UPDATE_HOST);
    }

    upgrade_flash_addr = fw_addr;
    upgrade_flash_map = (flash_map << 4) | flash_mode;

    free_boot_info();
    return true;
}

//--------------------------------------------------------------------
LOCAL uint8_t ICACHE_FLASH_ATTR check_bootloader(uint8_t *boot_buffer)
{
    struct MD5Context context;
    uint8_t digest[16] = {0};
    uint8_t md5_str[36] = {0};
    uint32_t i;
    // -----------------------------------------------
    // Calculate MD5 checksum of the bootloader buffer
    // -----------------------------------------------

    MD5Init(&context);
    MD5Update(&context, (uint32_t *)boot_buffer, upgrade_content_len);
    MD5Final(digest, &context);

    if (os_strlen(update_md5) == 32) {
        // We have the MD5 checksum which must match the calculated one
        for (i=0; i<16; i++) {
            os_sprintf(md5_str + (i*2), "%02X", digest[i]);
        }
        md5_str[32] = 0;
        if (os_memcmp(update_md5, md5_str, 32) == 0) return 1;
        if (upgrade_debug) {
            at_port_print_irom_str("  MD5 checksum not matched\r\n");
        }
    }
    else if (upgrade_debug) {
        at_port_print_irom_str("  MD5 checksum not downloaded\r\n");
    }
    return 0;
}

//-----------------------------------------------------------
static void ICACHE_FLASH_ATTR OtaUpdate_CallBack(bool result)
{
    if (result) {
        uint8_t buffer[80] = {0};
        uint32_t fw_ok = 0;
        int32_t fw_n = (upgrade_flash_addr - FW_PART_OFFSET) / FW_PART_INC;

        os_delay_us(1000);
        // Check the firmware and save boot configuration
        if (upgrade_debug) {
            os_sprintf(buffer, "Checking firmware %d at 0x%06X\r\n", fw_n, upgrade_flash_addr);
            at_port_print(buffer);
        }

        fw_ok = check_firmware(fw_n);
        at_leave_special_state();
        if (fw_ok > 0) {
            at_port_print_irom_str("+UPDATEFIRMWARE:1,ready for use\r\n");
            at_response_ok();

            if (update_reset == 1) {
                at_port_print_irom_str("+UPDATEFIRMWARE:1,RESTART NOW\r\n");
                os_delay_us(500000);
                system_restart();
            }
        }
        else {
            at_port_print_irom_str("+UPDATEFIRMWARE:0,new firmware check failed\r\n");
            at_response_error();
        }
    }
    else {
        at_port_print_irom_str("+UPDATEFIRMWARE:0,firmware upgrade failed\r\n");
        at_leave_special_state();
        at_response_error();
    }
}

//---------------------------------------------------------------
static void ICACHE_FLASH_ATTR OtaUpdateBoot_CallBack(bool result)
{
    uint8_t boot_ok = 0;
    if ((result) && (upgrade_check_response != NULL)) {
        // Check the firmware and save boot configuration
        if (upgrade_debug) {
            at_port_print_irom_str("Checking bootloader MD5 csum\r\n");
        }

        boot_ok = check_bootloader(upgrade_check_response);
        if (boot_ok > 0) {
            // ==============================================================
            // ==== ! The correct flash map and flash mode MUST be set ! ====
            upgrade_check_response[2] = upgrade_flash_map & 0x0F;
            upgrade_check_response[3] = upgrade_flash_map & 0xF0;
            // ==============================================================
            if (spi_flash_erase_sector(0) == 0) {
                if (spi_flash_write(0, (uint32_t *)upgrade_check_response, upgrade_content_len) == 0) {

                    at_port_print_irom_str("+UPDATEBOOT:1\r\n");
                    at_leave_special_state();
                    at_response_ok();
                    if (update_reset == 1) {
                        // erase bootloader configuration
                        spi_flash_erase_sector(SYSTEM_PARTITION_BOOT_PARAMETER_ADDR / SECTOR_SIZE);
                        // erase all configuration settings
                        spi_flash_erase_sector(SYSTEM_PARTITION_RF_CAL_ADDR / SECTOR_SIZE);
                        spi_flash_erase_sector(SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR / SECTOR_SIZE);
                        spi_flash_erase_sector((SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR+0x1000) / SECTOR_SIZE);
                        spi_flash_erase_sector((SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR+0x2000) / SECTOR_SIZE);
                        spi_flash_erase_sector(SYSTEM_PARTITION_AT_PARAMETER_ADDR / SECTOR_SIZE);
                        spi_flash_erase_sector((SYSTEM_PARTITION_AT_PARAMETER_ADDR+0x1000) / SECTOR_SIZE);
                        spi_flash_erase_sector((SYSTEM_PARTITION_AT_PARAMETER_ADDR+0x2000) / SECTOR_SIZE);

                        at_port_print_irom_str("+UPDATEBOOT:1,RESTART NOW\r\n");
                        os_delay_us(500000);
                        system_restart();
                    }
                    return;
                }
            }
            at_port_print_irom_str("+UPDATEBOOT:!!WRITE TO FLASH FAILED!!\r\n");
            at_port_print_irom_str("+UPDATEBOOT:0\r\n");
            at_leave_special_state();
            at_response_error();
            return;
        }
    }
    at_port_print_irom_str("+UPDATEBOOT:0\r\n");
    at_leave_special_state();
    at_response_error();
}

//-----------------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR OtaVerCheck_MD5_CallBack(bool result, uint32_t fw_addr)
{
    uint8_t buffer[128] = {0};
    uint8_t new_ver[64] = {0};
    uint8_t *ver_end;
    int matched, len;
    os_delay_us(10000);

    if ((result) && (upgrade_check_response != NULL)) {
        if (upgrade_type == OTA_TYPE_VERCHECK) {
            ver_end = os_strchr(upgrade_check_response, '\r');
            if (ver_end != NULL) {
                len = ver_end - upgrade_check_response;
                if (len == os_strlen(ESP_AT_LOBO_VERSION)) {
                    os_memcpy(new_ver, upgrade_check_response, len);
                    matched = (os_strcmp(new_ver, ESP_AT_LOBO_VERSION) == 0) ? 0 : 1;
                    os_sprintf(buffer, "+UPDATECHECK:%d,\"%s\",\"%s\"\r\n", matched, new_ver, ESP_AT_LOBO_VERSION);
                    at_port_print(buffer);
                    at_port_print(upgrade_check_response);

                    os_free(upgrade_check_response);
                    upgrade_check_response = NULL;
                    at_leave_special_state();
                    at_response_ok();
                    return;
                }
            }
            at_port_print(upgrade_check_response);
            os_free(upgrade_check_response);
            upgrade_check_response = NULL;
        }
        else if (upgrade_type == OTA_TYPE_VERCHECKBOOT) {
            uint8_t old_ver[64] = {0};
            int32_t boot_ver = get_rtc_bootver_addr();
            os_sprintf(old_ver, "LoBo ESP8266 Bootloader v%d.%d.%d", boot_ver >> 16, (boot_ver >> 8) & 0xFF, boot_ver & 0xFF);
            ver_end = os_strchr(upgrade_check_response, '\r');
            if (ver_end != NULL) {
                len = ver_end - upgrade_check_response;
                if (len == os_strlen(old_ver)) {
                    os_memcpy(new_ver, upgrade_check_response, len);
                    matched = (os_strcmp(new_ver, old_ver) == 0) ? 0 : 1;
                    os_sprintf(buffer, "+UPDATECHECKBOOT:%d,\"%s\",\"%s\"\r\n", matched, new_ver, old_ver);
                    at_port_print(buffer);

                    os_free(upgrade_check_response);
                    upgrade_check_response = NULL;
                    at_leave_special_state();
                    at_response_ok();
                    return;
                }
            }
            at_port_print(upgrade_check_response);
            os_free(upgrade_check_response);
            upgrade_check_response = NULL;
        }
        else {
            os_memset(update_md5, 0, sizeof(update_md5));
            os_memcpy(update_md5, upgrade_check_response, 32);
            os_sprintf(buffer, "+UPDATEGET%s:1,\"%s\"", (upgrade_type == OTA_TYPE_MD5BOOT) ? "CSUMBOOT" : "CSUM", update_md5);
            at_port_print(buffer);
            at_leave_special_state();
            at_response_ok();
            return;
        }
    }

    if (upgrade_type == OTA_TYPE_VERCHECK) {
        at_port_print_irom_str("+UPDATECHECK:0,Error\r\n");
    }
    else if (upgrade_type == OTA_TYPE_VERCHECKBOOT) {
        at_port_print_irom_str("+UPDATECHECKBOOT:0,Error\r\n");
    }
    else {
        os_memset(update_md5, 0, sizeof(update_md5));
        os_sprintf(buffer, "+UPDATEGET%s:0, Error", (upgrade_type == OTA_TYPE_MD5BOOT) ? "CSUMBOOT" : "CSUM");
        at_port_print(buffer);
    }
    at_leave_special_state();
    at_response_error();
}

//-----------------------------------------------------------
static void ICACHE_FLASH_ATTR updateVerCheckMD5(uint8_t type)
{
    if (os_strlen(upgrade_remote_host) == 0) {
        os_sprintf(upgrade_remote_host, "%s", REMOTE_UPDATE_HOST);
    }

    if (!update_setparams()) {
        if (upgrade_debug) {
            at_port_print_irom_str("Error, cannot set update parameters\r\n");
        }
        at_response_error();
        return;
    }
    upgrade_type = type;

    at_enter_special_state();
    // start the process
    if (!at_ota_start((ota_callback)OtaVerCheck_MD5_CallBack)) {
        if (upgrade_debug) {
            at_port_print_irom_str("Error starting OTA process\r\n");
        }
        at_leave_special_state();
        at_response_error();
        return;
    }
}

//====================================================
void ICACHE_FLASH_ATTR at_exeCmdFWVerCheck(uint8_t id)
{
    upgrade_use_ssl = 0;
    updateVerCheckMD5(OTA_TYPE_VERCHECK);
}

//========================================================
void ICACHE_FLASH_ATTR at_exeCmdFWVerCheckBoot(uint8_t id)
{
    upgrade_use_ssl = 0;
    updateVerCheckMD5(OTA_TYPE_VERCHECKBOOT);
}

//==================================================
void ICACHE_FLASH_ATTR at_exeCmdFWGetMD5(uint8_t id)
{
    os_memset(update_md5, 0, sizeof(update_md5));
    upgrade_use_ssl = 0;
    updateVerCheckMD5(OTA_TYPE_MD5);
}

//======================================================
void ICACHE_FLASH_ATTR at_exeCmdFWGetMD5boot(uint8_t id)
{
    os_memset(update_md5, 0, sizeof(update_md5));
    upgrade_use_ssl = 0;
    updateVerCheckMD5(OTA_TYPE_MD5BOOT);
}

//====================================================
void ICACHE_FLASH_ATTR at_queryCmdFWGetMD5(uint8_t id)
{
    char buf[64] = {'\0'};
    os_sprintf(buf, "+UPDATEGETSCUM:\"%s\"\r\n", update_md5);
    at_port_print(buf);
    at_response_ok();
}

//=================================================================
void ICACHE_FLASH_ATTR at_setupCmdFWGetMD5(uint8_t id, char *pPara)
{
    int32_t val = -1, err = 0, flag = 0;
    pPara++; // skip '='

    //get the first parameter
    flag = at_get_next_int_dec(&pPara, &val, &err);
    if (*pPara != '\r') {
        at_response_error();
        return;
    }

    if (val == 0) {
        os_memset(update_md5, 0, sizeof(update_md5));
    }

    at_response_ok();
}

//-----------------------------------------------------
static void ICACHE_FLASH_ATTR startUpdate(uint8_t type)
{
    bool is_started = false;
    if (!update_setparams()) {
        if (upgrade_debug) {
            at_port_print_irom_str("Error, cannot set update parameters\r\n");
        }
        at_response_error();
        return;
    }
    upgrade_type = type;

    at_enter_special_state();
    // start the upgrade process
    if (type == OTA_TYPE_UPGBOOT) is_started = at_ota_start((ota_callback)OtaUpdateBoot_CallBack);
    else is_started = at_ota_start((ota_callback)OtaUpdate_CallBack);
    if (!is_started) {
        if (upgrade_debug) {
            at_port_print_irom_str("Error starting OTA process\r\n");
        }
        at_leave_special_state();
        at_response_error();
    }
    at_port_print_irom_str("Started, please wait...\r\n");
}

//==================================================
void ICACHE_FLASH_ATTR at_exeCmdFWupdate(uint8_t id)
{
    upgrade_use_ssl = 0;
    startUpdate(OTA_TYPE_UPGRADE);
}

//======================================================
void ICACHE_FLASH_ATTR at_exeCmdFWupdateBoot(uint8_t id)
{
    upgrade_use_ssl = 0;
    startUpdate(OTA_TYPE_UPGBOOT);
}

//===================================================
void ICACHE_FLASH_ATTR at_queryCmdFWDebug(uint8_t id)
{
    char buf[16] = {'\0'};
    os_sprintf(buf, "+UPDATEDEBUG:%d\r\n", upgrade_debug);
    at_port_print(buf);
    at_response_ok();
}

//================================================================
void ICACHE_FLASH_ATTR at_setupCmdFWDebug(uint8_t id, char *pPara)
{
    int32_t dbg = -1, err = 0, flag = 0;
    pPara++; // skip '='
    //get the first parameter (debug flag), digit
    flag = at_get_next_int_dec(&pPara, &dbg, &err);

    if (*pPara != '\r') {
        at_response_error();
        return;
    }

    if (dbg < 0) {
        at_response_error();
        return;
    }

    upgrade_debug = dbg & 1;
    at_response_ok();
}

// AT+UPDATEFIRMWARE="upgrade_remote_host"[,reset_after[,forced_part[,port[,ssl]]]]
//=================================================================
void ICACHE_FLASH_ATTR at_setupCmdFWupdate(uint8_t id, char *pPara)
{
    int upd_rst = -1, fw_n = -1, port = -1, ssl = -1;
    int err = 0, flag = 0;
    uint8 buffer[32] = {0};
    uint8 flash_map = system_get_flash_size_map();

    pPara++; // skip '='

    //get the 1st parameter (IP address or domain name), string
    flag = at_data_str_copy(buffer, &pPara, 31);
    if (flag < 3) goto exit_err;
    if (*pPara == '\r') goto exit_ok;
    if (*pPara != ',') goto exit_err;
    pPara++; // skip ','

    //get the 2nd parameter (reset flag), digit
    flag = at_get_next_int_dec(&pPara, &upd_rst, &err);
    if (err != 0) upd_rst = -1;
    if (flag == FALSE) goto exit_ok;
    if (*pPara != ',') goto exit_ok;
    pPara++; // skip ','

    //get the 3rd parameter (part num)
    flag = at_get_next_int_dec(&pPara, &fw_n, &err);
    if (err != 0) fw_n = -1;
    if (flag == FALSE) goto exit_ok;
    if (*pPara != ',') goto exit_ok;
    pPara++; // skip ','

    //get the 4th parameter (port)
    flag = at_get_next_int_dec(&pPara, &port, &err);
    if (err != 0) port = -1;
    if (flag == FALSE) goto exit_ok;
    if (*pPara != ',') goto exit_ok;
    pPara++; // skip ','

    //get the 5th parameter (port)
    flag = at_get_next_int_dec(&pPara, &ssl, &err);
    if (err != 0) ssl = -1;
    if (flag == FALSE) goto exit_ok;
    // check if the last parameter
    if (*pPara != '\r') goto exit_err;

exit_ok:
    // Check parameters
    os_sprintf(upgrade_remote_host, "%s", buffer);

    if (upd_rst >= 0) update_reset = (uint8_t)(upd_rst == 1);

    if (ssl >= 0) upgrade_use_ssl = (uint8_t)(ssl == 1);

    if ((port >= 1) && (port < 65566)) upgrade_remote_port = (uint16_t)port;

    if ((fw_n >= 0) && (fw_n < MAX_APP_PART)) {
        if (fw_n == get_rtc_curr_fw()) goto exit_err; // cannot update the running firmware
        if (flash_map == 2) {
            // 1MB flas has only 2 partitions
            if (fw_n > 1) goto exit_err;
        }
        else if ((flash_map == 3) || (flash_map == 4)) {
            // 512+512 firmware, check if the requested partition is available
            if ((fw_n > 2) && (flash_map == 2)) goto exit_err;
            if ((fw_n > 7) && (flash_map == 4)) goto exit_err;
        }
        else {
            // 1024+1024 firmware, check if the requested partition is available
            if ((flash_map % 2) != 0) goto exit_err; // cannot update odd partition
            if ((fw_n > 2) && (flash_map == 5)) goto exit_err;
            if ((fw_n > 6) && (flash_map == 6)) goto exit_err;
        }
        update_forced_fw = (int8_t)fw_n;
    }

    at_response_ok();
    return;

exit_err:
    at_response_error();
    return;
}

//====================================================
void ICACHE_FLASH_ATTR at_queryCmdFWupdate(uint8_t id)
{
    uint8_t buffer[64] = {0};
    os_sprintf(buffer, "+UPDATE:\"%s\",%d,%d,%d,%d\r\n",
               (upgrade_remote_host[0] == '\0') ? REMOTE_UPDATE_HOST : upgrade_remote_host, update_reset, update_forced_fw, upgrade_remote_port, upgrade_use_ssl);
    at_port_print(buffer);
    at_response_ok();
}

//====================================================
void ICACHE_FLASH_ATTR at_testCmdFWupdate(uint8_t id)
{
    at_port_print_irom_str("+UPDATE:\"<remote_host>\",<reset_after>: 0|1,<force_part>: 0-7,<remote_port>: 1-65365,<ssl: 0|1\r\n");
    at_port_print_irom_str("+UPDATE:only the first parameter is mandatory\r\n");
    at_response_ok();
}

// AT+BOOT=part_no[,rst[,permanent]]
// if 'permanent' is set to 8, set the selected partition as default
//=============================================================
void ICACHE_FLASH_ATTR at_setupCmdBoot(uint8_t id, char *pPara)
{
    uint32_t bootapp = 0, rst = 0, perm = 0, err = 0, flag = 0;
    uint8 buffer[40] = {0};
    pPara++; // skip '='

    //get the first parameter (bootapp no)
    // digit
    flag = at_get_next_int_dec(&pPara, &bootapp, &err);

    if (*pPara == ',') {
        pPara++;
        flag = at_get_next_int_dec(&pPara, &rst, &err);
        if (*pPara == ',') {
            pPara++;
            flag = at_get_next_int_dec(&pPara, &perm, &err);
            if (perm != 0x08) perm = 0;
        }
    }

    if (*pPara != '\r') {
        at_response_error();
        return;
    }

    if ((bootapp < 0) || (bootapp >= MAX_APP_PART)) {
        at_response_error();
        return;
    }

    set_rtc_nextboot((uint8_t)bootapp | perm);

    if (rst == 1) {
        at_port_print_irom_str("+BOOT:RESTART NOW\r\n");
        os_delay_us(500000);
        system_restart();
    }
    at_response_ok();
}

//================================================
void ICACHE_FLASH_ATTR at_queryCmdBoot(uint8_t id)
{
    uint8_t buffer[64] = {0};
    uint8_t md5[36] = {0};
    uint8_t i, n;

    // Read the boot configuration
    if (!read_boot_info()) {
        at_response_error();
        return;
    }

    // Print the current partition number
    os_sprintf(buffer, "+BOOT:%d,%06X,%d,%06X\r\n", get_rtc_curr_fw(), get_rtc_curr_fw_addr(), boot_info->boot_part & 0x07, boot_info->boot_addr);
    at_port_print(buffer);
    // print all loadable roms boot_info
    for (n=0; n<MAX_APP_PART; n++) {
        if (boot_info->part_length[n] < 0xF0000) {
            os_memset(md5, 0, sizeof(md5));
            for (i=0; i<16;i++) {
                os_sprintf(md5 + (i*2), "%02X", boot_info->part_md5[n].md5[i]);
            }
            os_sprintf(buffer, "+BOOT:%d,%06X,%d,%d,%d,%s\r\n",
                    n, (n * FW_PART_INC) + FW_PART_OFFSET, boot_info->part_type[n] >> 4, boot_info->part_type[n] & 0x0F, boot_info->part_length[n], md5);
            at_port_print(buffer);
        }
    }

    free_boot_info();
    at_response_ok();
}

#endif
