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

// ==== CHANGES by LoBo =====================================================
/*
 * Copyright (c)LoBo 2019
 *
 * ==========================================================================
 * Allow to set the SSS buffer size up to 16384 bytes:
 *
 * Changed sources: `third_party/include/mbedtls/espconn_mbedtls.h`
 *                  `third_party/include/ssl/app/espconn_ssl.h`
 *   ESPCONN_SECURE_MAX_SIZE is set to 16384
 *
 * See bellow about how the `at_setupCmdCipSslsize` function must be changed.
 *
 * If CONFIG_ENABLE_IRAM_MEMORY is set, there should be enough RAM to use it.
 * ==========================================================================
 *
 * ==========================================================================
 * Support for loading CA certificate from RAM buffer is added.
 * Changed sources: `include/user_interface.h`
 *                  `third_party/mbedtls/app/espconn_mbedtls.c`
 * ==========================================================================
 *
 * ==========================================================================
 * The `mbedtls` library must be recompiled for above changes to be enabled.
 * Run in `third_party` directory: `./make_lib.sh mbedtls` to recompile it.
 * ==========================================================================
 *
 * Support for loading from SPI flash positions above 1st MB, see bellow.
 *
 * Added AT+SNTPTIME which returns unit time and ANSI formated time string.
 *
 * Added Firmware upgrade support with many new options and features.
 *
*/


#include "osapi.h"
#include "at_custom.h"
#include "user_interface.h"
#ifdef AT_UPGRADE_SUPPORT
#include "at_upgrade.h"
#endif
#include "at_extra_cmd.h"

#if AT_SDIO_ENABLE
#include "driver/sdio_slv.h"
#endif


#ifdef CONFIG_ENABLE_IRAM_MEMORY
//--------------------------------------
uint32 user_iram_memory_is_enabled(void)
{
    return CONFIG_ENABLE_IRAM_MEMORY;
}
#endif


#if (SPI_FLASH_SIZE_MAP != 2)
//=====================================================================
/*
 * This enables loading from SPI flash positions above 1st MB
 * SDK library `libmain.a` has to be modified to in a way that
 * `Cache_Read_Enable_New` function is linked as weak, and the
 * new version of the `Cache_Read_Enable_New` function is defined here
 * 
 * cp libmain.a libmain_orig.a
 * xtensa-lx106-elf-objcopy -W Cache_Read_Enable_New libmain.a libmain_lobo.a
 * cp libmain_lobo.a libmain.a
 * 
 * Bootloader which sets the flash firmware address
 * in RTC memory must be used
 * 
 * more info about Flash mapping:
 * https://richard.burtons.org/2015/06/12/esp8266-cache_read_enable
*/

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// must be the same as in bootloader
#define RTC_USER_ADDR   0x60001140

//extern void Cache_Read_Disable(void);
extern void ets_printf(const char*, ...);
extern void Cache_Read_Enable(uint32_t, uint32_t, uint32_t);

uint8_t boot_mmap_1 = 0xff;
uint8_t boot_mmap_2 = 0xff;
volatile uint32_t *rtc_fw_addr = (uint32_t*)RTC_USER_ADDR + 8;

// this function must remain in iram
void IRAM_ATTR Cache_Read_Enable_New(void);

//----------------------------------------
void IRAM_ATTR Cache_Read_Enable_New(void)
{
    // Get the flash address saved by bootloader from RTC memory
    if (boot_mmap_1 == 0xff) {
        // first use
        uint32_t val = *rtc_fw_addr;
        val /= 0x100000;        // 1MB segment address

        boot_mmap_2 = val / 2;  // MB count
        boot_mmap_1 = val % 2;  // odd/even segment
        
        ets_printf("Boot mmap: %d,%d,%d\r\n", boot_mmap_1, boot_mmap_2, (user_iram_memory_is_enabled()) ? 0:1);
    }

    Cache_Read_Enable(boot_mmap_1, boot_mmap_2, (user_iram_memory_is_enabled()) ? 0:1);
}
//=====================================================================
#endif


//=====================================================================
/*
 * Replace `at_setupCmdCipSslsize` function with the following one
 * allowing to set the SSS buffer size up to 16384 bytes
 * The `at_setupCmdCipSslsize` function must be weakened in `libat.a`:
 * 
 * cp libat.a libat_orig.a
 * xtensa-lx106-elf-objcopy -W at_setupCmdCipSslsize libat.a libat_lobo.a
 * cp libat_lobo.a libat.a
 * 
*/

extern void ICACHE_FLASH_ATTR at_setupCmdCipSslsize(uint8_t id, char *pPara);

//-------------------------------------------------------------------
void ICACHE_FLASH_ATTR at_setupCmdCipSslsize(uint8_t id, char *pPara)
{
    int size = 0, err = 0, flag = 0;
    pPara++; // skip '='

    //get the 1st parameter, digit
    flag = at_get_next_int_dec(&pPara, &size, &err);

    if ((size < 2048) || (size > 16384)) {
        at_response_error();
        return;
    }

    if (*pPara != '\r') {
        at_response_error();
        return;
    }

    if (!espconn_secure_set_size(3, size)) {
        at_response_error();
        return;
    }

    at_response_ok();
}

//=====================================================================

/*
 * From ESP8266_NONOS_SDK_v2.1.0 onwards, when the DIO-to-QIO flash is not used,
 * users can add an empty function void user_spi_flash_dio_to_qio_pre_init(void) on
 * the application side to reduce iRAM usage.
*/
//-------------------------------------------
void user_spi_flash_dio_to_qio_pre_init(void)
{
}


#if AT_SDIO_ENABLE

#ifdef SDIO_DEBUG
static os_timer_t at_spi_check;
uint32 sum_len = 0;
extern void ICACHE_FLASH_ATTR at_spi_check_cb(void *arg);
#endif

uint32 at_fake_uart_rx(uint8* data,uint32 length);
typedef void (*at_fake_uart_tx_func_type)(const uint8*data,uint32 length);
bool at_fake_uart_enable(bool enable,at_fake_uart_tx_func_type at_fake_uart_tx_func);

typedef void (*at_custom_uart_rx_buffer_fetch_cb_type)(void);
void at_register_uart_rx_buffer_fetch_cb(at_custom_uart_rx_buffer_fetch_cb_type rx_buffer_fetch_cb);

extern void at_custom_uart_rx_buffer_fetch_cb(void);

sint8 ICACHE_FLASH_ATTR espconn_tcp_set_wnd(uint8 num);

//---------------------------------------------------------------------
void ICACHE_FLASH_ATTR at_sdio_response(const uint8*data,uint32 length)
{
    if((data == NULL) || (length == 0)) {
        return;
    }

    sdio_load_data(data,length);
}

//----------------------------------------------------
uint32 sdio_recv_data_callback(uint8* data,uint32 len)
{
    return at_fake_uart_rx(data,len);
}

#endif



//===== Main code =======================================================================

// Register new AT functions
//==================================
at_funcationType at_custom_cmd[] = {
    {"+SYSFLASHMAP",      12, NULL,               at_queryCmdFlashMap,     NULL,                      NULL},
    {"+SYSCPUFREQ",       11, NULL,               at_queryCmdSysCPUfreq,   at_setupCmdCPUfreq,        NULL},
    {"+TCPSERVER",        10, NULL,               at_queryCmdTCPServer,    at_setupCmdTCPServer,      NULL},
    {"+TCPSTART",          9, NULL,               at_queryCmdTCP,          at_setupCmdTCPConnConnect, NULL},
    {"+TCPSEND",           8, NULL,               at_queryCmdTCP,          at_setupCmdTCPSend,        NULL},
    {"+TCPCLOSE",          9, NULL,               at_queryCmdTCP,          at_setupCmdTCPClose,       NULL},
    {"+TCPSTATUS",        10, NULL,               at_queryCmdTCPStatus,    at_setupCmdTCPStatus,      at_queryCmdTCPStatus},
    {"+SSLCCONF",          9, NULL,               at_queryCmdTCPSSLconfig, at_setupCmdTCPSSLconfig,   NULL},
    {"+SSLLOADCERT",      12, NULL,               at_queryCmdTCPLoadCert,  at_setupCmdTCPLoadCert,    NULL},
    {"+SNTPTIME",          9, at_testCmdSNTPTime, at_queryCmdSNTPTime,     NULL,                      NULL},
#ifdef AT_CUSTOM_UPGRADE
    {"+UPDATEFIRMWARE",   15, at_testCmdFWupdate, at_queryCmdFWupdate,     at_setupCmdFWupdate,       at_exeCmdFWupdate},
    {"+UPDATEBOOT",       11, at_testCmdFWupdate, at_queryCmdFWupdate,     at_setupCmdFWupdate,       at_exeCmdFWupdateBoot},
    {"+UPDATECHECK",      12, NULL,               NULL,                    NULL,                      at_exeCmdFWVerCheck},
    {"+UPDATECHECKBOOT",  16, NULL,               NULL,                    NULL,                      at_exeCmdFWVerCheckBoot},
    {"+UPDATEGETCSUM",    14, NULL,               at_queryCmdFWGetMD5,     at_setupCmdFWGetMD5,       at_exeCmdFWGetMD5},
    {"+UPDATEGETCSUMBOOT",18, NULL,               at_queryCmdFWGetMD5,     at_setupCmdFWGetMD5,       at_exeCmdFWGetMD5boot},
    {"+UPDATEDEBUG",      12, NULL,               at_queryCmdFWDebug,      at_setupCmdFWDebug,        NULL},
    {"+BOOT",              5, NULL,               at_queryCmdBoot,         at_setupCmdBoot,           NULL}
#endif
};

// Define partitions table
//====================================================
static const partition_item_t at_partition_table[] = {
    { SYSTEM_PARTITION_BOOTLOADER,                      0x0000,                                             SECTOR_SIZE},
    { SYSTEM_PARTITION_OTA_1,                           SYSTEM_PARTITION_OTA_1_ADDR,                        SYSTEM_PARTITION_OTA_SIZE},
    #if (SPI_FLASH_SIZE_MAP_EX == 0)
    { SYSTEM_PARTITION_OTA_2,                           SYSTEM_PARTITION_OTA_2_ADDR,                        SYSTEM_PARTITION_OTA_SIZE},
    #endif

    { SYSTEM_PARTITION_SSL_CLIENT_CA,                   SYSTEM_PARTITION_SSL_CLIENT_CA_ADDR,                SECTOR_SIZE},
    { SYSTEM_PARTITION_SSL_CLIENT_CERT_PRIVKEY,         SYSTEM_PARTITION_SSL_CLIENT_CERT_PRIVKEY_ADDR,      SECTOR_SIZE},
    { SYSTEM_PARTITION_AT_PARAMETER,                    SYSTEM_PARTITION_AT_PARAMETER_ADDR,                 SECTOR_SIZE*3},

    #if (SPI_FLASH_SIZE_MAP_EX == 0)
    { SYSTEM_PARTITION_BOOT_PARAM,                      SYSTEM_PARTITION_BOOT_PARAMETER_ADDR,               SECTOR_SIZE},
    #endif

    { SYSTEM_PARTITION_RF_CAL,                          SYSTEM_PARTITION_RF_CAL_ADDR,                       SECTOR_SIZE},
    { SYSTEM_PARTITION_PHY_DATA,                        SYSTEM_PARTITION_PHY_DATA_ADDR,                     SECTOR_SIZE},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER,                SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR,             SECTOR_SIZE*3},

#ifdef CONFIG_AT_WPA2_ENTERPRISE_COMMAND_ENABLE
    { SYSTEM_PARTITION_WPA2_ENTERPRISE_CA,              SYSTEM_PARTITION_WPA2_ENTERPRISE_CA_ADDR,           SECTOR_SIZE},
    { SYSTEM_PARTITION_WPA2_ENTERPRISE_CERT_PRIVKEY,    SYSTEM_PARTITION_WPA2_ENTERPRISE_CERT_PRIVKEY_ADDR, SECTOR_SIZE},
#endif
};

// For ESP8266_NONOS_SDK_v3.0.0 and later versions this function must be added
//========================================
void ICACHE_FLASH_ATTR user_pre_init(void)
{
    system_uart_swap();
    system_set_os_print(0);
    if (!system_partition_table_regist(at_partition_table, sizeof(at_partition_table)/sizeof(at_partition_table[0]),SPI_FLASH_SIZE_MAP)) {
        os_printf("Failed to register system partition table!\r\n");
        while(1);
    }
}

//====================================
void ICACHE_FLASH_ATTR user_init(void)
{
    char buf[64] = {0};
    // Set maximum number of connections
    at_customLinkMax = 5;
    espconn_secure_set_size(3, 4096);
    #if AT_SDIO_ENABLE
    sdio_slave_init();
    sdio_register_recv_cb(sdio_recv_data_callback);
    #endif

    at_init();
    #if AT_SDIO_ENABLE
    at_register_uart_rx_buffer_fetch_cb(at_custom_uart_rx_buffer_fetch_cb);
    #endif

    os_sprintf(buf,"Compile time: "__DATE__" "__TIME__"\r\n"ESP_AT_LOBO_VERSION);
    at_set_custom_info(buf);
    #if AT_SDIO_ENABLE
    at_fake_uart_enable(TRUE,at_sdio_response);
    #endif

    os_delay_us(100000);
    system_uart_de_swap(); // swapped in `user_pre_init`
    #if AT_SDIO_ENABLE
    espconn_tcp_set_wnd(4);
    #endif
    at_port_print_irom_str("\r\nready\r\n");

    at_cmd_array_regist(&at_custom_cmd[0], sizeof(at_custom_cmd)/sizeof(at_custom_cmd[0]));

    #ifdef CONFIG_AT_SMARTCONFIG_COMMAND_ENABLE
    at_cmd_enable_smartconfig();
    #endif

    #ifdef CONFIG_AT_WPA2_ENTERPRISE_COMMAND_ENABLE
    at_cmd_enable_wpa2_enterprise();
    #endif

    #if AT_SDIO_ENABLE
    #ifdef SDIO_DEBUG
    os_timer_disarm(&at_spi_check);
    os_timer_setfn(&at_spi_check, (os_timer_func_t *)at_spi_check_cb, NULL);
    os_timer_arm(&at_spi_check, 1000, 1);
    os_printf("\r\ntimer start\r\n");
    #endif
    #endif
}

