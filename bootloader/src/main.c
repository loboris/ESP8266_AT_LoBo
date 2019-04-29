/***********************************************************
 * Source Code BOOTLOADER for ESP8266
 * by LoBo, 04/2019 (https://github.com/loboris)
 * based on Bintechnology BOOTLOADER
     https://github.com/ezequieldonhauser/ESP8266_BOOTLOADER
************************************************************
*/

#include "helper.h"
#include "c_types.h"
#include "eagle_soc.h"

//bootloader version, update both!
#define BOOTLOADER_VERSION	"1.2.0"
#define BOOTLOADER_VERNUM	0x00010200

// SET THIS TO 1 IF THE BOARD CRYSTAL IS 26 MHz
#define CRYSTAL_IS_26MHZ        1

#define FAIL_IF_NO_MD5_CHECK    0
#define FAIL_IF_NOT_SAME_MAP    1

//PIN 2  = GPIO2 = LED
#define PIN_LED             2
#define MUX_LED             PERIPHS_IO_MUX_GPIO2_U
#define FUN_LED             FUNC_GPIO2

//PIN 0 = GPIO0  = BUTTON
#define PIN_BTN             0
#define MUX_BTN             PERIPHS_IO_MUX_GPIO0_U
#define FUN_BTN             FUNC_GPIO0

#define CHECKSUM_INIT       0xEF
#define HEADER_MAGIC        0xEA
#define SECTION_MAGIC       0xE9
#define SECTOR_SIZE         0x1000

#define BOOT_FW_MAGIC       0x5AA5C390
#define BOOT_FW_MAGIC_MASK  0xFFFFFFF0

#if NO_OTA_SUPPORT == 0

// ---------------------------------------------------
// Address of the Boot parameters sector
// The same sector must be defined in user application
#define BOOT_PARAM_ADDR     0xFA000
// ---------------------------------------------------

#define MAX_APP_PART        8
#define FW_PART_INC         0x80000
#define FW_PART_OFFSET      0x1000

#endif
/*
 * Data at RTC_USER_ADDR (all data are uint32_t, 4 byte aligned):
 * 0: RTC_USER_ADDR +  0:   User requested partition to boot from, BOOT_FW_MAGIC + part_no
 * 1: RTC_USER_ADDR +  4:   Currently booted partition, BOOT_FW_MAGIC + part_no
 * 2: RTC_USER_ADDR +  8:   Currently booted partition SPI flash address, 0x1000, 0x81000, 0x101000, 0x181000, ...
 * 3: RTC_USER_ADDR + 12:   Boot counter
 * 4: RTC_USER_ADDR + 16:   Bootloader version
*/
#define RTC_USER_ADDR       0x60001140


enum rst_reason {
    REASON_DEFAULT_RST		= 0,
    REASON_WDT_RST			= 1,
    REASON_EXCEPTION_RST	= 2,
    REASON_SOFT_WDT_RST   	= 3,
    REASON_SOFT_RESTART 	= 4,
    REASON_DEEP_SLEEP_AWAKE	= 5,
    REASON_EXT_SYS_RST      = 6
};

typedef struct {
    uint8 magic;
    uint8 count;
    uint8 flags1;   // flash spi mode (0=qio; 1=qout; 2=dio; 3=dout)
    uint8 flags2;   // (flash map << 4)
    uint32 entry;
}binary_header;

typedef struct {
    uint32 address;
    uint32 length;
}section_header;

#if NO_OTA_SUPPORT == 0
typedef struct {
    uint8 md5[16];
}md5_t;

/*
 * ESP8266 Flash modes:
 * --------------------
    0:  512 KB flash: 256+256
    1:  256 KB flash
    2:  1 MB flash: 512+512
    3:  2 MB flash: 512+512
    4:  4 MB flash: 512_512
    5:  2 MB flash: 1024+1024
    6:  4 MB flash: 1024+1024
    7:  8 MB flash: 1024+1024
    8: 16 MB flash: 1024+1024
*/

typedef struct {
    uint32  boot_part;                  // active boot partition, BOOT_FW_MAGIC + part_no (0 ~ 7)
    uint32  boot_addr;                  // flash address of the active boot partition (0=0x1000; 1=0x81000; 2=0x101000; ...)
    uint32  part_length[MAX_APP_PART];  // length of the partition firmware
    md5_t   part_md5[MAX_APP_PART];     // MD5 checksum of the partition firmware (16 bytes)
    uint8_t part_type[MAX_APP_PART];    // ((Flash map << 4) | flash mode)
}boot_info_t;
#endif

static uint8 boot_flash_map = 0xFF;
static uint8 boot_flash_mode = 0xFF;


// The bootloader entry point
//=================================================================================
void USED bootloader_main(void)
{
    //this application don't use .data section
    //almost every data will be in stack, when we
    //return from bootloader_task() the stack will be
    //where the internal bootloader left us.
    __asm volatile
    (
        "l32r a2, _vec_base\n"		//load the vector base to a2
        "wsr.vecbase a2\n"			//set vecbase register with that value
        "mov a15, a0\n"				//store return addr
        "call0 bootloader_task\n"	//call bootloader_task() to check and load APP
        "mov a0, a15\n"				//restore return addr
        "jx a2\n"					//jump into the APP entry point
    );
}
//=================================================================================

// Check the firmware at given flash address
// Return the address to the firmware entry point if good
//-----------------------------------------------------------------------------------------------------------
#if NO_OTA_SUPPORT == 0
uint32 USED check_load_binary(uint32 flash_addr, uint8 app_n, boot_info_t *boot_info, uint8 save, uint8 *map)
#else
uint32 USED check_load_binary(uint32 flash_addr)
#endif
{
    uint32 i, j;
    uint32 remaining;
    uint32 readlen;
    uint32 load_addr;
    binary_header header;
    section_header section;
    uint8 buffer[256];
    uint8 checksum;
    uint8 flash_map, flash_map1;
#if NO_OTA_SUPPORT == 0
    uint8 flash_mode;
    struct MD5Context context;
    uint8 digest[16];
    uint32 total;
    uint32 flash_end;
    uint32 flash_start = flash_addr;
#endif

    ets_printf("  Address: %06X\r\n", flash_addr);
    // read the first header 
    if (SPIRead(flash_addr, &header, sizeof(binary_header))) goto exit_spi_err;

    // check the header magic and number of sections
    if ((header.magic != HEADER_MAGIC) || (header.count == 0)) {
        ets_printf("    No firmware\r\n");
        return 0;
    }

    flash_addr += sizeof(binary_header);

    // ignore the section ROM, jump to the end of this section
    if (SPIRead(flash_addr, &section, sizeof(section_header))) goto exit_spi_err;
    flash_addr += sizeof(section_header);
    flash_addr += section.length;

    // read the second header
    if (SPIRead(flash_addr, &header, sizeof(binary_header))) goto exit_spi_err;

    //check the header magic and number of sections
    if ((header.magic != SECTION_MAGIC) || (header.count == 0)) {
        ets_printf("    Wrong section\r\n");
        return 0;
    }

    // Get firmware flash map and mode
    flash_map = header.flags2 & 0xF0;
#if NO_OTA_SUPPORT == 0
    flash_mode = header.flags1 & 0x0F;
#endif
    #if FAIL_IF_NOT_SAME_MAP
    if (boot_flash_map != 0xFF) {
        flash_map1 = flash_map >> 4;
        if ((flash_map1 != boot_flash_map) && (!((flash_map1 == 2) && (boot_flash_map == 0)))) {
            ets_printf("    Wrong flash mode\r\n");
            return 0;
        }
    }
    #endif

    // init checksum
    flash_addr += sizeof(binary_header);
    checksum = CHECKSUM_INIT;

    // calculate the checksum of all sections
    for(i=0; i<header.count; i++) {
        // read section header
        if (SPIRead(flash_addr, &section, sizeof(section_header))) goto exit_spi_err;
        
        flash_addr += sizeof(section_header);
        load_addr = section.address;
        remaining = section.length;

        //calculate the checksum
        while (remaining > 0) {
            // read the length of the buffer
            readlen = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
            // an read into the local buffer
            if (SPIRead(flash_addr, buffer, readlen)) goto exit_spi_err;

            // load into the specific RAM address
            ets_memcpy((uint32*)load_addr, (uint32*)buffer, readlen);

            // add to checksum
            for(j=0; j<readlen; j++) {
                checksum ^= buffer[j];
            }
            
            // update the controls
            flash_addr += readlen;
            load_addr += readlen;
            remaining -= readlen;
        }
    }

    // read the last byte of the binary, contains the checksum byte
    // Align to 4 bytes
    flash_addr = (flash_addr | 0xF) - 3;
    if (SPIRead(flash_addr, buffer, 4) != 0) goto exit_spi_err;

    // check if calculated and read checksums matches
    if (buffer[3] != checksum) {
        ets_printf("    Wrong CSUM\r\n");
        return 0;
    }

    // ==== Everything checked and OK! ====
#if NO_OTA_SUPPORT == 1
    return header.entry;
#else
    // -------------------------------------------------
    // Calculate MD5 checksum of the firmware flash area
    // -------------------------------------------------
    flash_end = flash_addr + 8; // there are 4 more bytes after the checksum byte
    total = 0;
    flash_addr = flash_start;
    
    MD5Init(&context);
    remaining = flash_end-flash_start;

    while (remaining) {
        readlen = (remaining >= 256) ? 256 : remaining;
        if (SPIRead(flash_addr, buffer, readlen) != 0) goto exit_spi_err;
        MD5Update(&context, buffer, readlen);
        flash_addr += readlen;
        total += readlen;
        remaining -= readlen;
    }
    MD5Final(digest, &context);

    if (ets_memcmp(digest, (boot_info_t *)boot_info->part_md5[app_n].md5, 16) != 0) {
        // MD5 does not match firmware in boot config
        ets_printf("    Wrong MD5");
        if (save) {
            ets_printf(", updating\r\n");
            // read boot config sector
            flash_addr = BOOT_PARAM_ADDR;
            if (SPIRead(BOOT_PARAM_ADDR, boot_info, sizeof(boot_info_t))) goto exit_spi_err;
            // save new firmware data into boot parameters
            if (*map == 0xFF) {
                boot_info->boot_addr = flash_start;
                boot_info->boot_part = (BOOT_FW_MAGIC | app_n);
                *map = flash_map | flash_mode;
                ets_printf("    Default firmware set\r\n");
            }
            boot_info->part_length[app_n] = total;
            boot_info->part_type[app_n] = flash_map | flash_mode;
            ets_memcpy(boot_info->part_md5[app_n].md5, digest, 16);

            if (SPIEraseSector(BOOT_PARAM_ADDR / SECTOR_SIZE)) goto exit_spi_err;
            if (SPIWrite(BOOT_PARAM_ADDR, boot_info, sizeof(boot_info_t))) goto exit_spi_err;
            ets_printf("    Config sector saved\r\n");
        }
        else {
            ets_printf("!\r\n");
            return 0;
        }
    }
    
    return header.entry;
#endif

exit_spi_err:
    ets_printf("    SPI error\r\n");
    return 0;
}

//-----------------------------
void USED hard_wdt_config(void)
{
    //start the HARD WDT to restart if something goes wrong
    WDT_CTRL &= 0x7e;	//Disable WDT
    INTC_EDGE_EN |= 1;	//0x3ff00004 |= 1
    WDT_REG1 = 0xb;		//WDT timeout
    WDT_REG2 = 0xd;
    WDT_CTRL |= 0x38;   // single stage, 
    WDT_CTRL &= 0x79;
    WDT_CTRL |= 1;		//Enable WDT
}

//-----------------------
void USED blink_led(void)
{
    uint32_t led_state=0;
    uint32_t state_counter=1000;
    uint32_t timeout = 1000;

    PIN_FUNC_SELECT(MUX_LED, FUN_LED);
    while(1) {
        if (state_counter) state_counter--;
        else {
            if (timeout > 0) {
                WDT_FEED = WDT_FEED_MAGIC; // reset watchdog
                timeout--;
            }
            state_counter = 2000000;
            if (led_state) {
                GPIO_OUTPUT_SET(PIN_LED, 1);
                led_state=0;
            }
            else {
                GPIO_OUTPUT_SET(PIN_LED, 0);
                led_state=1;
            }
        }
    }
}

//--------------------------
uint32 USED get_button(void)
{
    //get status of the button
    //return 1 - button pressed, pin=down (low)

    PIN_FUNC_SELECT(MUX_BTN, FUN_BTN);
    PIN_PULLUP_EN(MUX_BTN); // pull-up
    gpio_output_set(0, 0, 0, 1<<PIN_BTN);
    ets_delay_us(1000);
    return (GPIO_INPUT_GET(PIN_BTN)==0);
}

//-------------------------------------------
static enum rst_reason get_reset_reason(void)
{
    // reset reason is stored @ offset 0 in system rtc memory
    volatile uint32_t *rtc_rst = (uint32_t*)0x60001100;
    return *rtc_rst;
}

//-----------------------------------
static uint32 get_reset_excause(void)
{
    volatile uint32_t *rtc_excause = (uint32_t*)0x60001104;
    return *rtc_excause;
}

//---------------------------------------
static uint32 get_rtc_userdata(uint8 idx)
{
    volatile uint32_t *rtc_ruserdata = (uint32_t*)RTC_USER_ADDR;
    return *(rtc_ruserdata + (idx*4));
}

//--------------------------------------------------
static void set_rtc_userdata(uint8 idx, uint32 data)
{
    volatile uint32_t *rtc_wuserdata = (uint32_t*)RTC_USER_ADDR;
    *(rtc_wuserdata + (idx*4)) = data;
}

///---------------------------
static void print_flash_info()
{
    if (boot_flash_map == 0xFF) return;

#if NO_OTA_SUPPORT == 1
    ets_printf("   Flash map: 2, 512KB (NO OTA)\r\n");
    ets_printf("  Flash mode: ");
    if (boot_flash_mode == 0) ets_printf("QIO");
    else if (boot_flash_mode == 1) ets_printf("QOUT");
    else if (boot_flash_mode == 2) ets_printf("DIO");
    else if (boot_flash_mode == 3) ets_printf("DOUT");
    else ets_printf("?");

    if ((boot_flash_map == 2) && (boot_flash_mode == 3)) ets_printf(", [ESP8285]");
    ets_printf("\r\n");
#else
    uint16 fsize = 4;
    if (boot_flash_map == 2) fsize = 1;
    else if ((boot_flash_map == 3) || (boot_flash_map == 5)) fsize = 2;

    if (boot_flash_map >= 5) ets_printf("   Flash map: %d, %dMB (1024+1024)\r\n", boot_flash_map, fsize);
    else ets_printf("   Flash map: %d, %dMB (512+512)\r\n", boot_flash_map, fsize);

    ets_printf("  Flash mode: ");
    if (boot_flash_mode == 0) ets_printf("QIO");
    else if (boot_flash_mode == 1) ets_printf("QOUT");
    else if (boot_flash_mode == 2) ets_printf("DIO");
    else if (boot_flash_mode == 3) ets_printf("DOUT");
    else ets_printf("?");

    if ((boot_flash_map == 2) && (boot_flash_mode == 3)) ets_printf(", [ESP8285]");
    ets_printf("\r\n");
#endif
}

/*
 * The bootloader task
 * do all the stuff, check and load the firmware
 * and return the address of the entry point,
 * if get some error stay blinking the system led forever.
 */
//========================================
uint32 NOINLINE USED bootloader_task(void)
{
    binary_header header;
    uint32 entry_point;
    uint32 rst_reason = get_reset_reason();
    uint32 rst_exccause = get_reset_excause();
#if NO_OTA_SUPPORT != 1
    uint32 entry_point_temp;
    uint32 app_addr_temp;
    uint32 app_addr;
    uint8 i, n;
    uint8 app_n;
    uint8 map = 0xFF;
    uint32 load_app = get_rtc_userdata(0);
    boot_info_t boot_info;
#endif

    //start the hardware watchdog
    hard_wdt_config();

    // soft reset doesn't reset PLL/divider, so leave as configured
    if (rst_reason != REASON_SOFT_RESTART) {
        #if CRYSTAL_IS_26MHZ
        // otherwise, set baudrate to 115200
        // with 26 MHz crystal the original baudrate is 115200*26/40=74880

        // specify the correct frequency for counting time in ets_delay_us(),
        // since it counts CPU cycles and multiplies by the specified number in MHz ...
        ets_update_cpu_frequency(52);
        uart_div_modify(0, (52000000)/115200);
        ets_delay_us(5000);

        //ets_printf("\r\nChange CPU clock from %d MHz to 80 MHz\r\n", ets_get_cpu_frequency());
        // delay for the characters to leave the UART
        ets_delay_us(5000);
        // Switch pll to 80MHz from 26 MHz crystal
        rom_i2c_writeReg(103,4,1,136);
        rom_i2c_writeReg(103,4,2,145);

        // The clock is now 80 MHz
        ets_update_cpu_frequency(80);
        uart_div_modify(0, (80000000)/115200);
        //UART0_CLKDIV = 80000000/115200;
        ets_delay_us(1000);
        #endif
    }

    ets_set_user_start(0);
    ets_delay_us(100);

    // save boot counter in user RAM
    if ((get_rtc_userdata(3) & 0xFFFFFFF0) != BOOT_FW_MAGIC) {
        set_rtc_userdata(3, BOOT_FW_MAGIC | 1);
    }
    else {
        set_rtc_userdata(3, BOOT_FW_MAGIC | (((get_rtc_userdata(3) & 0x0F) + 1) & 0x0F));
    }
    // save bootloader version in user RAM
    set_rtc_userdata(4, 0xA5000000 | BOOTLOADER_VERNUM);

    // read the spi flash map and mode
    if (SPIRead(0, (uint32 *)&header, sizeof(binary_header)) == 0) {
        if (header.magic == SECTION_MAGIC) {
            boot_flash_map = header.flags2 >> 4;
            boot_flash_mode =header.flags1 & 0x0F;
        }
    }

    // print some info
    ets_printf("\r\n==============================\r\n");
    ets_printf("LoBo ESP8266 Bootloader v"BOOTLOADER_VERSION"\r\n");
    ets_printf("==============================\r\n");
    //ets_printf("   SPI Flash: Id=%08X, %dMB\r\n", flashchip->deviceId, flashchip->chip_size / 0x100000);
    print_flash_info();
    ets_printf("Reset reason: ");
    if (rst_reason == REASON_WDT_RST) ets_printf("WDT");
    else if (rst_reason == REASON_EXCEPTION_RST) ets_printf("EXCEPTION (%d)", rst_exccause);
    else if (rst_reason == REASON_SOFT_WDT_RST) ets_printf("Soft WDT");
    else if (rst_reason == REASON_SOFT_RESTART) ets_printf("Soft RESTART");
    else if (rst_reason == REASON_DEEP_SLEEP_AWAKE) ets_printf("Deep-sleep AWAKE");
    else if (rst_reason == REASON_EXT_SYS_RST) ets_printf("Ext reset");
    else ets_printf("POWER_ON");
    ets_printf("\r\n\r\n");
    ets_delay_us(1000);

#if NO_OTA_SUPPORT == 1
    // ===============================================================
    // ==== 512KB flash, only one firmware, try to load it ===========
    ets_printf("NO OTA SUPPORT, loading firmware...\r\n");
    if ((get_rtc_userdata(3) & 0x0F) >= 15) goto exit_err;
    entry_point = check_load_binary(0x01000);
    if (entry_point != 0) goto exit_ok;
    goto exit_err;
    // ===============================================================
#else
    // ==== Read boot parameters sector ====
    if (SPIRead(BOOT_PARAM_ADDR, &boot_info, sizeof(boot_info_t)) != 0) {
        ets_printf("Error reading bootloader config sector\r\n");
        goto exit_err;
    }
    // Check if this is the 1st boot ever (no boot configuration written yet)
    if ((boot_info.boot_part < BOOT_FW_MAGIC) || (boot_info.boot_part >= (BOOT_FW_MAGIC + MAX_APP_PART))) goto boot_1st;

    // ============================================================================
    // ==== If Boot partition was selected in user application, try to load it ====
    // ============================================================================
    if ((load_app & BOOT_FW_MAGIC_MASK) == BOOT_FW_MAGIC) {
        // load the firmware selected from the application
        app_n = (uint8)(load_app & 0x07);
        ets_printf("Loading requested firmware (%d)\r\n", app_n);
        if (!((boot_flash_map > 4) && (app_n % 2))) {
            map = boot_info.part_type[app_n];
            // reset the user request
            set_rtc_userdata(0, 0);

            app_addr = (app_n * FW_PART_INC) + FW_PART_OFFSET;
            entry_point = check_load_binary(app_addr, app_n, &boot_info, 0, &map);
            if (entry_point != 0) {
                // Make this partition permanent if requested (bit #7 set)
                if ((load_app & 0x08) == 0x08) {
                    boot_info.boot_addr = app_addr;
                    boot_info.boot_part = (BOOT_FW_MAGIC | app_n);
                    SPIEraseSector(BOOT_PARAM_ADDR / SECTOR_SIZE);
                    SPIWrite(BOOT_PARAM_ADDR, &boot_info, sizeof(boot_info_t));
                    ets_printf("    Selected as default\r\n");
                }
                goto exit_ok;
            }
        }
        else {
            ets_printf("  Address not allowed\r\n");
        }
        ets_printf("    Error\r\n\r\n");
    }

    // ====================================================================
    // ==== Check the boot config to determine which firmware to start ====
    // ====================================================================
    if ((boot_info.boot_part & BOOT_FW_MAGIC) == BOOT_FW_MAGIC) {
        // configured boot firmware exists
        app_n = (uint8)(boot_info.boot_part & 0x07);
        map = boot_info.part_type[app_n];

        // check if the button was pressed or max number of resets reached
        if ((get_button()) || ((get_rtc_userdata(3) & 0x0F) >= 15)) {
            // try to load other firmware than configured
            ets_printf("Button or max resets reached!\r\n", app_n);
            i = 1;
            if (boot_flash_map > 4) i = 2;
            for (n=0; n<MAX_APP_PART; n+=i) {
                if ((boot_flash_map == 2) && (n > 1)) break;
                if (((boot_flash_map == 3) || (boot_flash_map == 5)) && (n > 3)) break;
                if (n != app_n) {
                    // check and load the firmware from address
                    app_addr = (n * FW_PART_INC) + FW_PART_OFFSET;
                    entry_point = check_load_binary(app_addr, n, &boot_info, 0, &map);
                    if (entry_point != 0) {
                        ets_printf("    found firmware\r\n", n, app_addr);
                        app_n = n;
                        map = boot_info.part_type[app_n];
                        goto exit_ok;
                    }
                }
            }
            ets_printf("    not found\r\n\r\n");
            goto exit_err;
        }
        // check and load the configured firmware
        app_addr = (app_n * FW_PART_INC) + FW_PART_OFFSET;
        ets_printf("Loading configured firmware (%d)\r\n", app_n);
        entry_point = check_load_binary(app_addr, app_n, &boot_info, 0, &map);
        if (entry_point != 0) goto exit_ok;
        // Failed, try once more
        ets_delay_us(10000);
        entry_point = check_load_binary(app_addr, app_n, &boot_info, 0, &map);
        if (entry_point != 0) goto exit_ok;

        ets_printf("    Error loading\r\n\r\n");
    }

boot_1st:
    // ==== Try to find the loadable binary ====
    ets_printf("No configured firmware found, check all...\r\n");
    entry_point = 0;
    app_n = 0;
    app_addr = 0;
    map = 0xFF;
    i = 1;
    if (boot_flash_map > 4) i = 2;
    set_rtc_userdata(3, BOOT_FW_MAGIC | 1);
    for (n=0; n<MAX_APP_PART; n+=i) {
        if ((boot_flash_map == 2) && (n > 1)) break;
        if (((boot_flash_map == 3) || (boot_flash_map == 5)) && (n > 3)) break;
        // check and load the binary firmware from address
        app_addr_temp = (n * FW_PART_INC) + FW_PART_OFFSET;
        // it may be the first time the firmware runs, allow saving boot config
        entry_point_temp = check_load_binary(app_addr_temp, n, &boot_info, 1, &map);
        if (entry_point_temp != 0) {
            if (entry_point == 0) {
                entry_point = entry_point_temp;
                app_n = n;
                app_addr = app_addr_temp;
                map = boot_info.part_type[app_n];
                ets_printf("    Selected as default\r\n");
            }
        }
    }
    if (entry_point != 0) goto exit_ok;
#endif

exit_err:
    // error loading firmware
    ets_printf("FATAL ERROR, no firmware found\r\n\r\n");

    // call the blink LED forever
    blink_led();

    return 0; // will not arrive here

exit_ok:
#if NO_OTA_SUPPORT == 1
    ets_printf("Starting firmware, map %d from 01000...\r\n\r\n", (boot_flash_map >> 4));
#else
    ets_printf("Starting firmware %d, map %d from %06x...\r\n\r\n", app_n, (map >> 4), app_addr);
    set_rtc_userdata(1, BOOT_FW_MAGIC | app_n); // save current firmware
    set_rtc_userdata(2, app_addr);              // and firmware address in user RAM
#endif
    ets_delay_us(100);
    return entry_point;
}
