#!/bin/bash

export "PATH=${PWD}/../xtensa-lx106-elf/bin:$PATH"

make clean
make NO_OTA_SUPPORT=1

if [ $? -ne 0 ]; then
    echo "Error"
    exit 1
else
    BOOT_VERSION=$(cat src/main.c | grep "#define BOOTLOADER_VERSION" | cut -d'"' -f 2)
    echo "OK, ver='${BOOT_VERSION}'"
    cp -f build/bootloader.bin ${PWD}/../bin/bootloader_noota.bin
    MD5CSUM=($(md5sum -b ${PWD}/../bin/bootloader_noota.bin))
    printf "${MD5CSUM^^}" > ${PWD}/../bin/bootloader_noota.md5
fi

echo ""

make clean
make NO_OTA_SUPPORT=0

if [ $? -ne 0 ]; then
    echo "Error"
    exit 1
else
    BOOT_VERSION=$(cat src/main.c | grep "#define BOOTLOADER_VERSION" | cut -d'"' -f 2)
    cp -f build/bootloader.bin ${PWD}/../bin/bootloader.bin
    MD5CSUM=($(md5sum -b ${PWD}/../bin/bootloader.bin))
    printf "${MD5CSUM^^}" > ${PWD}/../bin/bootloader.md5
    printf "LoBo ESP8266 Bootloader v${BOOT_VERSION}\r\n" > ${PWD}/../bin/bootloaderver.txt

    FILESIZE=$(stat -c%s build/bootloader.bin)
	echo "OK, ver: '${BOOT_VERSION}', file size: ${FILESIZE} bytes"
fi
