# Open Source Bootloader for ESP8266

## Compile

You can open this code with the Eclipse, or just run make to compile it.

To compile run in this directory:<br>

```
.\build.sh
```

## Output

The binary is generated in the "build" folder as "bootloader.bin".<br>
The script generates OTA and NON-OTA binaries, version info file and MD% checksum files.<br>
All generated files will be copyed to the `../bin` directory to be used by AT firmware.<br>

## Example

```
./build.sh
rm -R -f build/*
xtensa-lx106-elf-gcc -std=c99 -Wall -O1 -mtext-section-literals -mlongcalls -nostdlib -fno-builtin -flto -Wl,-static -g -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -L. -Tbootloader_linker.ld  -DNO_OTA_SUPPORT=1 -o build/output.elf src/main.c src/vector.S
xtensa-lx106-elf-objcopy --only-section .text -O binary build/output.elf build/text.out
xtensa-lx106-elf-objcopy --only-section .rodata -O binary build/output.elf build/rodata.out
xtensa-lx106-elf-nm -g build/output.elf > build/output.sym
xtensa-lx106-elf-objdump -a -f -h -D build/output.elf > build/output.dmp
python gen_binary.py build/output.sym build/text.out build/rodata.out build/bootloader.bin
==================================================================
[.rodata 560 Bytes][.bss 8 Bytes]
------------------------------------------------------------------
[SRAM (.rodata): 568 Bytes][SRAM (.bss): 3528 Bytes]
------------------------------------------------------------------
[IRAM USED: 1680 Bytes][IRAM LIB: 14704 Bytes]
==================================================================
#@rm -f build/text.out
#@rm -f build/rodata.out
#@rm -f build/output.sym
---------------------------------------
-------- COMPILED successfully --------
---------------------------------------
OK, ver='1.2.0'

rm -R -f build/*
xtensa-lx106-elf-gcc -std=c99 -Wall -O1 -mtext-section-literals -mlongcalls -nostdlib -fno-builtin -flto -Wl,-static -g -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -L. -Tbootloader_linker.ld  -DNO_OTA_SUPPORT=0 -o build/output.elf src/main.c src/vector.S
xtensa-lx106-elf-objcopy --only-section .text -O binary build/output.elf build/text.out
xtensa-lx106-elf-objcopy --only-section .rodata -O binary build/output.elf build/rodata.out
xtensa-lx106-elf-nm -g build/output.elf > build/output.sym
xtensa-lx106-elf-objdump -a -f -h -D build/output.elf > build/output.dmp
python gen_binary.py build/output.sym build/text.out build/rodata.out build/bootloader.bin
==================================================================
[.rodata 992 Bytes][.bss 8 Bytes]
------------------------------------------------------------------
[SRAM (.rodata): 1000 Bytes][SRAM (.bss): 3096 Bytes]
------------------------------------------------------------------
[IRAM USED: 3056 Bytes][IRAM LIB: 13328 Bytes]
==================================================================
#@rm -f build/text.out
#@rm -f build/rodata.out
#@rm -f build/output.sym
---------------------------------------
-------- COMPILED successfully --------
---------------------------------------
OK, ver: '1.2.0', file size: 4080 bytes
```

