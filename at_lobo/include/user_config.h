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

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define SECTOR_SIZE             0x1000

#ifdef AT_UPGRADE_SUPPORT
#if (SPI_FLASH_SIZE_MAP_EX == 0)
#define AT_CUSTOM_UPGRADE
#endif
#endif

#define ESP_AT_LOBO_VERSION     "ESP8266_AT_LoBo v1.2.4"

// ---------------------------------------------------------------
// This adds ~22.5 KB to free system RAM, should be always enabled
// ---------------------------------------------------------------
// Enabling it may affect the system performance,
// because the Flash cache size is decreased to only 16 KB
// os_malloc, os_zalloc and os_calloc will allocate from iRAM first,
// and dRAM will be the next available memory when iRAM is used up.

#define CONFIG_ENABLE_IRAM_MEMORY                           1


// ====================================
// === Experimental, do not enable! ===
#define AT_SDIO_ENABLE          0
//#define SDIO_DEBUG
// ====================================


/*
 * ======================================================================
 * There is no flash space to include both WPA Enterprise and SmartConfig
 * into 512+512 Flash map! Chose only one of them if needed at all
 * ======================================================================
 *
 * There is almost no documentation about how to use those features,
 * so it is best to leave both commented, unless you know more about them
 *
 */

// -------------------
// Enable smart config
// -------------------
// Uses ~24.6 KB of Flash and 824 bytes of system RAM
//#define CONFIG_AT_SMARTCONFIG_COMMAND_ENABLE

// ---------------------
// Enable WPA Enterprise
// ---------------------
// It uses ~59.6 KB of Flash and ~5 KB of system RAM if enabled!
// Not possible on 512KB flash
#if (SPI_FLASH_SIZE_MAP_EX == 0)
//#define CONFIG_AT_WPA2_ENTERPRISE_COMMAND_ENABLE
#endif


// ==== Partitions definitions ==================================================================

/* --------------------------------------------------------------------------------------
 * Maximal size of the flash file for     512+0 firmware is 0x75000 (479232, 468KB) bytes
 * Maximal size of the flash file for   512+512 firmware is 0x79000 (495616, 484KB) bytes
 * Maximal size of the flash file for 1024+1024 firmware is 0xF0000 (983040, 960KB) bytes
 * --------------------------------------------------------------------------------------
*/

#define SYSTEM_PARTITION_BOOT_PARAM                         (SYSTEM_PARTITION_CUSTOMER_BEGIN + 1)

#if (SPI_FLASH_SIZE_MAP < 2) || (SPI_FLASH_SIZE_MAP > 6)
#error "The flash map is not supported"
#endif

#define SYSTEM_PARTITION_OTA_1_ADDR                         0x1000

#if (SPI_FLASH_SIZE_MAP_EX == 1)
// 512 KB flash, (NO OTA!)
#define SYSTEM_PARTITION_OTA_SIZE                           0x75000
#define SYSTEM_PARTITION_OTA_2_ADDR                         0x81000 // not used
#define SYSTEM_PARTITION_PARAM_START                        0x76000
// all params sectors bellow 0x80000
#define SYSTEM_PARTITION_RF_CAL_ADDR                        0x7B000
#define SYSTEM_PARTITION_PHY_DATA_ADDR                      0x7C000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR              0x7D000 // 3 sectors

#else

// 512+512 or 1024+1024 flash maps
#if (SPI_FLASH_SIZE_MAP > 4)
// 1024+1024 flash map, OTA
#define SYSTEM_PARTITION_OTA_SIZE                           0xF0000
#define SYSTEM_PARTITION_OTA_2_ADDR                         0x101000
#define SYSTEM_PARTITION_PARAM_START                        0xF5000

#else
// 512+512 flash map, OTA
#define SYSTEM_PARTITION_OTA_SIZE                           0x79000
#define SYSTEM_PARTITION_OTA_2_ADDR                         0x81000
#define SYSTEM_PARTITION_PARAM_START                        0x7B000
#endif // (SPI_FLASH_SIZE_MAP > 4)

#define SYSTEM_PARTITION_BOOT_PARAMETER_ADDR                0xFA000 // !MUST BE AT THE SAME ADDRESS AS IN BOOTLOADER!
#define SYSTEM_PARTITION_RF_CAL_ADDR                        0xFB000
#define SYSTEM_PARTITION_PHY_DATA_ADDR                      0xFC000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR              0xFD000 // 3 sectors

#endif // (SPI_FLASH_SIZE_MAP_EX == 1)

#define SYSTEM_PARTITION_SSL_CLIENT_CA_ADDR                 (SYSTEM_PARTITION_PARAM_START + 0x0000)
#define SYSTEM_PARTITION_SSL_CLIENT_CERT_PRIVKEY_ADDR       (SYSTEM_PARTITION_PARAM_START + 0x1000)
#define SYSTEM_PARTITION_AT_PARAMETER_ADDR                  (SYSTEM_PARTITION_PARAM_START + 0x2000) // 3 sectors

#ifdef CONFIG_AT_WPA2_ENTERPRISE_COMMAND_ENABLE

#if (SPI_FLASH_SIZE_MAP < 5)
#define SYSTEM_PARTITION_WPA2_ENTERPRISE_CA_ADDR            0x80000
#else
#define SYSTEM_PARTITION_WPA2_ENTERPRISE_CA_ADDR            (SYSTEM_PARTITION_PARAM_START - 0x2000)
#endif

#define SYSTEM_PARTITION_WPA2_ENTERPRISE_CERT_PRIVKEY_ADDR  (SYSTEM_PARTITION_PARAM_START - 0x1000)

#endif

// ==============================================================================================

#endif
