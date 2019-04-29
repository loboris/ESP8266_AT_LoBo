#ifndef ROM_FUNCTIONS_H
#define ROM_FUNCTIONS_H

#include "c_types.h"
#include "eagle_soc.h"

//------------------------------------------------------------------------------
// ROM FUNCTIONS
//------------------------------------------------------------------------------
typedef struct{
	uint32_t	deviceId;		//+00
	uint32_t	chip_size;    	//+04 chip size in byte
	uint32_t	block_size;		//+08
	uint32_t	sector_size;	//+0c
	uint32_t	page_size;		//+10
	uint32_t	status_mask;	//+14
} SpiFlashChip;

typedef enum {
    SPI_FLASH_RESULT_OK,
    SPI_FLASH_RESULT_ERR,
    SPI_FLASH_RESULT_TIMEOUT
} SpiFlashOpResult;

extern SpiFlashChip * flashchip; // in ROM: 0x3fffc714

extern void ets_set_user_start(void (*user_start_fn)());
extern void ets_update_cpu_frequency(uint8);
extern uint8_t ets_get_cpu_frequency();
extern void ets_printf(char*, ...);
extern void ets_delay_us(int);
extern void ets_memset(void*, uint8, uint32);
extern void ets_memcpy(void*, const void*, uint32);
extern int ets_memcmp(const void *str1, const void *str2, unsigned int nbyte);
extern void software_reset(void);
extern void gpio_output_set(uint32_t,uint32_t,uint32_t,uint32_t);
extern uint32 gpio_input_get(void);
extern void uart_div_modify(uint32_t, uint32_t);

extern int rom_get_power_db(void);
extern void rom_en_pwdet(int);
extern void rom_i2c_writeReg(uint32 block, uint32 host_id, uint32 reg_add, uint32 data);
extern void rom_i2c_writeReg_Mask(uint32 block, uint32 host_id, uint32 reg_add, uint32 Msb, uint32 Lsb, uint32 indata);
extern uint8 rom_i2c_readReg_Mask(uint32 block, uint32 host_id, uint32 reg_add, uint32 Msb, uint32 Lsb);
extern uint8 rom_i2c_readReg(uint32 block, uint32 host_id, uint32 reg_add);

extern uint32 SPIRead(uint32 addr, void *outptr, uint32 len);
extern uint32 SPIWrite(uint32 addr, void *inptr, uint32 len);
extern uint32_t SPIEraseChip();
extern uint32_t SPIEraseBlock(uint32_t block_num);
extern uint32_t SPIEraseSector(uint32_t sector_num);
SpiFlashOpResult SPI_read_status(SpiFlashChip *sflashchip, uint32_t *sta);

extern void ets_wdt_init(void);
extern void ets_wdt_enable(uint32_t mode, uint32_t arg1, uint32_t arg2);
extern void ets_wdt_disable(void);
extern void ets_wdt_restore(uint32_t mode);
extern uint32_t ets_wdt_get_mode(void);

int ets_sprintf(char *str, const char *format, ...)  __attribute__ ((format (printf, 2, 3)));
#define os_sprintf_plus  ets_sprintf
#define os_sprintf(buf, fmt, ...) os_sprintf_plus(buf, fmt, ##__VA_ARGS__)

void dtm_params_init(void * sleep_func, void * int_func);
void dtm_set_params(int a2, int time_ms, int a4, int a5, int a6);
void rtc_enter_sleep(void);


struct MD5Context
{
    uint32_t buf[4];
    uint32_t bits[2];
    uint8_t in[64];
};

extern void MD5Init(struct MD5Context *ctx);
extern void MD5Update(struct MD5Context *ctx, void *buf, uint32_t len);
extern void MD5Final(uint8_t digest[16], struct MD5Context *ctx);



#define UART_START_CLOCK	(80000000)



#define GPIO_OUTPUT_SET(gpio_no, bit_value)			\
	gpio_output_set((bit_value)<<gpio_no,			\
	((~(bit_value))&0x01)<<gpio_no, 1<<gpio_no,0)	\


//------------------------------------------------------------------------------
// REGISTERS
//------------------------------------------------------------------------------

//DPORT REGISTERS
#define INTC_EDGE_EN	*(volatile uint32_t *)0x3FF00004

//RTC REGISTERS
#define WDT_CTRL		*(volatile uint32_t *)0x60000900
#define WDT_REG1		*(volatile uint32_t *)0x60000904
#define WDT_REG2		*(volatile uint32_t *)0x60000908
#define WDT_FEED		*(volatile uint32_t *)0x60000914
#define WDT_FEED_MAGIC	0x73

#endif
