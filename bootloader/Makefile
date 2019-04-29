####################################
### LoBo ESP8266/8285 BOOTLOADER ###
####################################

NO_OTA_SUPPORT?=0

#compiler and linker parameters
GCC_OPTIONS=-std=c99 -Wall -O1 -mtext-section-literals -mlongcalls	    \
	-nostdlib -fno-builtin -flto -Wl,-static -g -ffunction-sections	    \
	-fdata-sections -Wl,--gc-sections -Isrc -L. -Tbootloader_linker.ld  \

#directories
SRC_FILES=src/main.c src/vector.S
ELF_FILE=build/output.elf
TEXT_FILE=build/text.out
DATA_FILE=build/data.out
RODATA_FILE=build/rodata.out
BIN_FILE=build/bootloader.bin
SYM_FILE=build/output.sym
DMP_FILE=build/output.dmp

GCC_OPTIONS+=-DNO_OTA_SUPPORT=$(NO_OTA_SUPPORT)

#main target
all:
	xtensa-lx106-elf-gcc $(GCC_OPTIONS) -o $(ELF_FILE) $(SRC_FILES)
	xtensa-lx106-elf-objcopy --only-section .text -O binary $(ELF_FILE) $(TEXT_FILE)
	xtensa-lx106-elf-objcopy --only-section .rodata -O binary $(ELF_FILE) $(RODATA_FILE)
	xtensa-lx106-elf-nm -g $(ELF_FILE) > $(SYM_FILE)
	xtensa-lx106-elf-objdump -a -f -h -D $(ELF_FILE) > $(DMP_FILE)
	python gen_binary.py $(SYM_FILE) $(TEXT_FILE) $(RODATA_FILE) $(BIN_FILE)
	#@rm -f $(TEXT_FILE)
	#@rm -f $(RODATA_FILE)
	#@rm -f $(SYM_FILE)
	@echo "---------------------------------------"
	@echo "-------- COMPILED successfully --------"
	@echo "---------------------------------------"

#clean files target
clean:
	rm -R -f build/*
