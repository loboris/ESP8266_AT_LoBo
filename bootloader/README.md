# Open Source Bootloader for ESP8266

This bootloader is part of bintechnology ESP8266 applications, and now it is open source.

# Compile

You can open this code with the Netbeans IDE, or just call make to compile it

obs: you don't need the Espressif SDK to compile this project

toolchain: xtensa-lx106-elf (GCC) 4.8.2

# Output

The binary is generated in the "build" folder as "bootloader.bin"

# Description

When ESP8266 is powerup and the IOs are properly set to boot from external flash, the internal bootloader will check and load the the binary that is stored in the first block of the flash at address 0x00000, that is the place where you will store this bootloader. This bootloader as default will try to load the binary from flash at address 0x81000, or from 0x01000 if the first one fails. There is more details about the load binaries process...
More details soon...
