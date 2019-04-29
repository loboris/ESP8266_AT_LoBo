/********************************************
 *  BINTECHNOLOGY BOOTLOADER LINKER SCRIPT  *
 ********************************************/

MEMORY
{
	iram : org = 0x40108000, len = 0x4000
	dram : org = 0x3FFFB000, len = 0x1000
}

ENTRY(bootloader_main)

SECTIONS
{
	.text : ALIGN(4)
	{
		_text_start = ABSOLUTE(.);
		_vec_base = ABSOLUTE(.);
		KEEP(*(.VectorBase.text));
		. = ALIGN(16);
		KEEP(*(.DebugExceptionVector.text));
		. = ALIGN(16);
		KEEP(*(.NMIExceptionVector.text));
		. = ALIGN(16);
		KEEP(*(.KernelExceptionVector.text));
		LONG(0x00000000);
		LONG(0x00000000);
		LONG(0x00000000);
		LONG(0x00000000);
		. = ALIGN(16);
		KEEP(*(.UserExceptionVector.text));
		LONG(0x00000000);
		LONG(0x00000000);
		LONG(0x00000000);
		LONG(0x00000000);
		. = ALIGN(16);
		KEEP(*(.DoubleExceptionVector.text));
		. = ALIGN(16);
		*(.text);
		*(.text.*);
		. = ALIGN(16);
		_text_end = ABSOLUTE(.);
	} > iram

    .rodata : ALIGN(4)
    {
		_rodata_start = ABSOLUTE(.);
		*(.rodata);
		*(.rodata.*);
		. = ALIGN(16);
		_rodata_end = ABSOLUTE(.);
    } > dram

    .bss ALIGN(8) (NOLOAD) : ALIGN(4)
    {
		_bss_start = ABSOLUTE(.);
		*(.bss);
		*(.bss.*);
		. = ALIGN(16);
		_bss_end = ABSOLUTE(.);
    } > dram
}

/* get ROM code address */
INCLUDE "bootloader_rom.ld"
