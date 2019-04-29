#!/bin/bash

ARG="$1"

export "PATH=${PWD}/../xtensa-lx106-elf/bin:$PATH"
export SDK_PATH=${PWD}/..
export BIN_PATH=${PWD}/../bin

echo ""
echo "====================================="
echo "Building ESP8266/ESP8285 AT firmwares"
echo "====================================="

rm -f ../bin/upgrade/*

AT_VERSION=$(cat include/user_config.h | grep "ESP8266_AT_LoBo" | cut -d'"' -f 2)


SPI_MODE="QIO"
SPI_SPEED=40

FLASH_SIZE=4
FW_TYPE=512
FLASH_MAP=4
FLASH_MAP_512=0

if [ "${ARG}" != "-V" ]; then
echo
# -------------------------
# Comment builds not needed
# -------------------------

BUILD_8285="yes"

BUILD_8266_MAP0="yes"
BUILD_8266_MAP2="yes"

BUILD_8266_MAP5="yes"
BUILD_8266_MAP6="yes"

BUILD_8266_MAP3="yes"
fi
BUILD_8266_MAP4="yes"

#======================
build_8285_firmware() {
    MAX_FILESIZE=495616
    # ==== Flash map 2, 1MB ESP8285 ==========================================================
    echo ""
    echo "Building firmware for 1MB Flash (ESP8285, DOUT spi mode)"

    cp -f ../lib/libmain_orig.a ../lib/libmain.a
    cp -f ../ld/eagle.app.v6_2_1.ld ../ld/eagle.app.v6.ld
    cp -f ../ld/eagle.app.v6_2_1.ld ../ld/eagle.app.v6.new.1024.app1.ld

    make clean > /dev/null 2>&1
  	make COMPILE=gcc BOOT=new APP=1 SPI_SPEED=40 SPI_MODE=DOUT SPI_SIZE_MAP=2 > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "========== Error =========="
        make clean > /dev/null 2>&1
        make COMPILE=gcc BOOT=new APP=1 SPI_SPEED=40 SPI_MODE=DOUT SPI_SIZE_MAP=2
        echo "========== Error =========="
        exit 1
    fi
    FILESIZE=$(stat -c%s ../bin/upgrade/user1.1024.new.2.bin)
    if [ "${FILESIZE}" -ge "${MAX_FILESIZE}" ]; then
        echo "  Warning: File to big (${FILESIZE} > ${MAX_FILESIZE})"
    else
        echo "  File size = ${FILESIZE}"
    fi
    echo "  esp8285_AT_1_2.bin OK"

    cp -f ../ld/eagle.app.v6_2_2.ld ../ld/eagle.app.v6.ld
    cp -f ../ld/eagle.app.v6_2_2.ld ../ld/eagle.app.v6.new.1024.app2.ld
    make clean > /dev/null 2>&1
    make COMPILE=gcc BOOT=new APP=2 SPI_SPEED=40 SPI_MODE=DOUT SPI_SIZE_MAP=2 > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "========== Error =========="
        make clean > /dev/null 2>&1
        make COMPILE=gcc BOOT=new APP=2 SPI_SPEED=40 SPI_MODE=DOUT SPI_SIZE_MAP=2
        echo "========== Error =========="
        exit 2
    fi
    echo "  esp8285_AT_2_2.bin OK"

    mv -f ../bin/upgrade/user1.1024.new.2.bin ../bin/upgrade/esp8285_AT_1_2.bin
    if [ $? -ne 0 ]; then
        echo "Error"
        exit 1
    fi
    mv -f ../bin/upgrade/user2.1024.new.2.bin ../bin/upgrade/esp8285_AT_2_2.bin
    if [ $? -ne 0 ]; then
        echo "Error"
        exit 2
    fi
    MD5CSUM=($(md5sum -b ../bin/upgrade/esp8285_AT_1_2.bin))
    printf "${MD5CSUM^^}" > ../bin/upgrade/esp8285_AT_1_2.md5
    printf "esp8285_AT_1_2: ${MD5CSUM^^}\r\n" >> ../bin/upgrade/version.txt
    MD5CSUM=($(md5sum -b ../bin/upgrade/esp8285_AT_2_2.bin))
    printf "${MD5CSUM^^}" > ../bin/upgrade/esp8285_AT_2_2.md5
    printf "esp8285_AT_2_2: ${MD5CSUM^^}\r\n" >> ../bin/upgrade/version.txt

    sleep 1
}

#============
set_names() {
	rm -f ../ld/eagle.app.v6.new.*.ld > /dev/null 2>&1
	rm -f ../ld/eagle.app.v6.ld > /dev/null 2>&1

    OUT_FILE_NAME1="esp8266_AT_1_${FLASH_MAP}"
    OUT_FILE_NAME2="esp8266_AT_2_${FLASH_MAP}"
    OUT_FILE1="../bin/upgrade/${OUT_FILE_NAME1}.bin"
    OUT_FILE2="../bin/upgrade/${OUT_FILE_NAME2}.bin"
    if [ ${FW_TYPE} -eq 512 ]; then
       	if [ ${FLASH_MAP_512} -eq 1 ]; then
	        LD_FILE1="../ld/eagle.app.v6_0_1.ld"
	        LD_FILE2="../ld/eagle.app.v6_0_1.ld"
		    OUT_FILE_NAME1="esp8266_AT_1_0"
		    OUT_FILE_NAME2="esp8266_AT_2_0"
		    OUT_FILE1="../bin/upgrade/${OUT_FILE_NAME1}.bin"
		    OUT_FILE2="../bin/upgrade/${OUT_FILE_NAME2}.bin"
	       	MAX_FILESIZE=479232
    	else
	        LD_FILE1="../ld/eagle.app.v6_2_1.ld"
	        LD_FILE2="../ld/eagle.app.v6_2_2.ld"
        	MAX_FILESIZE=495616
    	fi
    else
        LD_FILE1="../ld/eagle.app.v6_56_1024.ld"
	    LD_FILE2=""
        MAX_FILESIZE=983040
    fi

    if [ ${FLASH_MAP} -eq 6 ]; then
        cp -f ../lib/libmain_lobo.a ../lib/libmain.a
        OUT_TEMP1="../bin/upgrade/user1.4096.new.6.bin"
        OUT_TEMP2="../bin/upgrade/user2.4096.new.6.bin"
    elif [ ${FLASH_MAP} -eq 5 ]; then
        cp -f ../lib/libmain_lobo.a ../lib/libmain.a
        OUT_TEMP1="../bin/upgrade/user1.2048.new.5.bin"
        OUT_TEMP2="../bin/upgrade/user2.2048.new.5.bin"
    elif [ ${FLASH_MAP} -eq 4 ]; then
        cp -f ../lib/libmain_lobo.a ../lib/libmain.a
        OUT_TEMP1="../bin/upgrade/user1.4096.new.4.bin"
        OUT_TEMP2="../bin/upgrade/user2.4096.new.4.bin"
    elif [ ${FLASH_MAP} -eq 3 ]; then
        cp -f ../lib/libmain_lobo.a ../lib/libmain.a
        OUT_TEMP1="../bin/upgrade/user1.2048.new.3.bin"
        OUT_TEMP2="../bin/upgrade/user2.2048.new.3.bin"
    elif [ ${FLASH_MAP} -eq 2 ]; then
        cp -f ../lib/libmain_orig.a ../lib/libmain.a
        OUT_TEMP1="../bin/upgrade/user1.1024.new.2.bin"
        OUT_TEMP2="../bin/upgrade/user2.1024.new.2.bin"
    else
        echo "Wrong flash map selected"
        exit 1
    fi
}

#======================
build_8266_firmware() {
    set_names

    echo ""

    if [ ${FLASH_MAP_512} -eq 1 ]; then
	    echo "Building firmware for ${FLASH_SIZE} Flash, Flash map: ${FLASH_MAP} (${FW_TYPE} no OTA)"
	else
	    echo "Building firmware for ${FLASH_SIZE} Flash, Flash map: ${FLASH_MAP} (${FW_TYPE}+${FW_TYPE})"
	fi

	# prepare '.ld' files
    cp -f ${LD_FILE1} ../ld/eagle.app.v6.ld
    if [ ${FW_TYPE} -eq 512 ]; then
		cp -f ${LD_FILE1} ../ld/eagle.app.v6.new.1024.app1.ld
    else
		cp -f ${LD_FILE1} ../ld/eagle.app.v6.new.2048.ld
    fi

    make clean > /dev/null 2>&1
	if [ "${ARG}" == "-V" ]; then
    	make COMPILE=gcc BOOT=new APP=1 SPI_SPEED=${SPI_SPEED} SPI_MODE=${SPI_MODE} SPI_SIZE_MAP=${FLASH_MAP} SPI_SIZE_MAP_EX=${FLASH_MAP_512}
	else
    	make COMPILE=gcc BOOT=new APP=1 SPI_SPEED=${SPI_SPEED} SPI_MODE=${SPI_MODE} SPI_SIZE_MAP=${FLASH_MAP} SPI_SIZE_MAP_EX=${FLASH_MAP_512} > /dev/null 2>&1
	fi
    if [ $? -ne 0 ]; then
        echo "========== Error =========="
        make clean > /dev/null 2>&1
        make COMPILE=gcc BOOT=new APP=1 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE_MAP=${FLASH_MAP} SPI_SIZE_MAP_EX=${FLASH_MAP_512}
        echo "========== Error =========="
        exit 1
    fi

    mv -f ${OUT_TEMP1} ${OUT_FILE1}
    if [ $? -ne 0 ]; then
        echo "Error"
        exit 1
    fi
    FILESIZE=$(stat -c%s $OUT_FILE1)
    if [ "${FILESIZE}" -ge "${MAX_FILESIZE}" ]; then
        echo "  Warning: File to big (${FILESIZE} > ${MAX_FILESIZE})"
    else
        echo "  File size = ${FILESIZE}"
    fi
    echo "  ${OUT_FILE_NAME1}.bin OK"
    MD5CSUM=($(md5sum -b ${OUT_FILE1}))
    printf "${MD5CSUM^^}" > ../bin/upgrade/${OUT_FILE_NAME1}.md5
    printf "${OUT_FILE_NAME1}: ${MD5CSUM^^}\r\n" >> ../bin/upgrade/version.txt

    if [ ${FW_TYPE} -eq 512 ] && [ ${FLASH_MAP_512} -eq 0 ]; then
        cp -f ${LD_FILE2} ../ld/eagle.app.v6.ld
		cp -f ${LD_FILE2} ../ld/eagle.app.v6.new.1024.app2.ld

        make clean > /dev/null 2>&1
        make COMPILE=gcc BOOT=new APP=2 SPI_SPEED=${SPI_SPEED} SPI_MODE=${SPI_MODE} SPI_SIZE_MAP=${FLASH_MAP} > /dev/null 2>&1
        if [ $? -ne 0 ]; then
            echo "Error"
            exit 2
        fi
        mv -f ${OUT_TEMP2} ${OUT_FILE2}
        if [ $? -ne 0 ]; then
            echo "Error"
            exit 2
        fi
        echo "  ${OUT_FILE_NAME2}.bin OK"
        MD5CSUM=($(md5sum -b ${OUT_FILE2}))
        printf "${MD5CSUM^^}" > ../bin/upgrade/${OUT_FILE_NAME2}.md5
        printf "${OUT_FILE_NAME2}: ${MD5CSUM^^}\r\n" >> ../bin/upgrade/version.txt
    #else
    #    cp -f ${OUT_FILE1} ${OUT_FILE2}
    fi
    sleep 1
}


printf "${AT_VERSION}\r\n" > ../bin/upgrade/version.txt


#------------------------------------
if [ "${BUILD_8285}" == "yes" ]; then
    build_8285_firmware
fi
#------------------------------------

#------------------------------------------
if [ "${BUILD_8266_MAP0}" == "yes" ]; then
    FLASH_SIZE="512KB"
    FW_TYPE=512
    FLASH_MAP=2
    FLASH_MAP_512=1
    build_8266_firmware
fi
#------------------------------------------

#------------------------------------------
if [ "${BUILD_8266_MAP2}" == "yes" ]; then
    FLASH_SIZE="1MB"
    FW_TYPE=512
    FLASH_MAP=2
    FLASH_MAP_512=0
    build_8266_firmware
fi
#------------------------------------------

#------------------------------------------
if [ "${BUILD_8266_MAP3}" == "yes" ]; then
    FLASH_SIZE="2MB"
    FW_TYPE=512
    FLASH_MAP=3
    FLASH_MAP_512=0
    build_8266_firmware
fi
#------------------------------------------

#------------------------------------------
if [ "${BUILD_8266_MAP4}" == "yes" ]; then
    FLASH_SIZE="4MB"
    FW_TYPE=512
    FLASH_MAP=4
    FLASH_MAP_512=0
    build_8266_firmware
fi
#------------------------------------------

#------------------------------------------
if [ "${BUILD_8266_MAP5}" == "yes" ]; then
    FLASH_SIZE="2MB"
    FW_TYPE=1024
    FLASH_MAP=5
    FLASH_MAP_512=0
    build_8266_firmware
fi
#------------------------------------------

#------------------------------------------
if [ "${BUILD_8266_MAP6}" == "yes" ]; then
    FLASH_SIZE="4MB"
    FW_TYPE=1024
    FLASH_MAP=6
    FLASH_MAP_512=0
    build_8266_firmware
fi
#------------------------------------------


rm -f ../ld/eagle.app.v6.new.*.ld > /dev/null 2>&1
rm -f ../ld/eagle.app.v6.ld > /dev/null 2>&1
rm -f ../bin/upgrade/*.dump
rm -f ../bin/upgrade/*.S

make clean > /dev/null 2>&1

echo ""
echo "====================================="
echo "Finished."
