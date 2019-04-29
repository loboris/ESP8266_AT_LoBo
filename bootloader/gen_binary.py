#!/usr/bin/python

import string
import sys
import os
import re
import binascii
import struct
import zlib

CHECKSUM_INIT = 0xEF
chk_sum = CHECKSUM_INIT
blocks = 0

def write_file(file_name,data):
	if file_name is None:
		print 'file_name cannot be none\n'
		sys.exit(0)

	fp = open(file_name,'ab')

	if fp:
		fp.seek(0,os.SEEK_END)
		fp.write(data)
		fp.close()
	else:
		print '%s write fail\n'%(file_name)

def combine_bin(file_name,dest_file_name,start_offset_addr,need_chk):
	global chk_sum
	global blocks
	if dest_file_name is None:
		print 'dest_file_name cannot be none\n'
		sys.exit(0)

	if file_name:
		fp = open(file_name,'rb')
		if fp:
			########## write text ##########
			fp.seek(0,os.SEEK_END)
			data_len = fp.tell()
			if data_len:
				if need_chk:
					tmp_len = (data_len + 3) & (~3)
				else:
					tmp_len = (data_len + 15) & (~15)
				data_bin = struct.pack('<II',start_offset_addr,tmp_len)
				write_file(dest_file_name,data_bin)
				fp.seek(0,os.SEEK_SET)
				data_bin = fp.read(data_len)
				write_file(dest_file_name,data_bin)
				if need_chk:
					for loop in range(len(data_bin)):
						chk_sum ^= ord(data_bin[loop])
				tmp_len = tmp_len - data_len
				if tmp_len:
					data_str = ['00']*(tmp_len)
					data_bin = binascii.a2b_hex(''.join(data_str))
					write_file(dest_file_name,data_bin)
					if need_chk:
						for loop in range(len(data_bin)):
							chk_sum ^= ord(data_bin[loop])
							blocks = blocks + 1
			fp.close()
		else:
			print '!!!Open %s fail!!!'%(file_name)

def gen_appbin():
	global chk_sum
	global crc_sum
	global blocks
	if len(sys.argv) != 5:
		print 'Usage: gen_appbin.exe app.sym text.bin rodata.bin flash.bin'
		sys.exit(0)

	app_sym_file	= sys.argv[1]
	text_bin_name	= sys.argv[2]
	rodata_bin_name	= sys.argv[3]
	flash_bin_name	= sys.argv[4]

	flash_data_line  = 16
	data_line_bits = 0xf
	BIN_MAGIC_FLASH  = 0xE9
	data_str = ''
	sum_size = 0

	fp = file(app_sym_file)
	if fp is None:
		print "open sym file error\n"
		sys.exit(0)

	lines = fp.readlines()
	fp.close()

	entry_addr = None
	p = re.compile('(\w*)(\sT\s)(bootloader_main)$')
	for line in lines:
		m = p.search(line)
		if m != None:
			entry_addr = m.group(1)
			# print entry_addr

	if entry_addr is None:
		print 'no entry point!!'
		sys.exit(0)

	rodata_start_addr = '0'
	p = re.compile('(\w*)(\sA\s)(_rodata_start)$')
	for line in lines:
		m = p.search(line)
		if m != None:
			rodata_start_addr = m.group(1)

	var_rodata_start_addr = None
	p = re.compile('(\w*)(\sA\s)(_rodata_start)$')
	for line in lines:
		m = p.search(line)
		if m != None:
			var_rodata_start_addr = m.group(1)

	if var_rodata_start_addr == None:
		print "[ERRO] no entry point (_rodata_start)\n"
		sys.exit(1)

	var_rodata_end_addr = None
	p = re.compile('(\w*)(\sA\s)(_rodata_end)$')
	for line in lines:
		m = p.search(line)
		if m != None:
			var_rodata_end_addr = m.group(1)

	if var_rodata_end_addr == None:
		print "[ERRO] no entry point (_rodata_end)\n"
		sys.exit(1)

	var_bss_start_addr = None
	p = re.compile('(\w*)(\sA\s)(_bss_start)$')
	for line in lines:
		m = p.search(line)
		if m != None:
			var_bss_start_addr = m.group(1)

	if var_bss_start_addr == None:
		print "[ERRO] no entry point (_bss_start)\n"
		sys.exit(1)

	var_bss_end_addr = '0'
	p = re.compile('(\w*)(\sA\s)(_bss_end)$')
	for line in lines:
		m = p.search(line)
		if m != None:
			var_bss_end_addr = m.group(1)

	if var_bss_end_addr == None:
		print "[ERRO] no entry point (_bss_end)\n"
		sys.exit(1)

	var_text_start_addr = None
	p = re.compile('(\w*)(\sA\s)(_text_start)$')
	for line in lines:
		m = p.search(line)
		if m != None:
			var_text_start_addr = m.group(1)

	if var_text_start_addr == None:
		print "[ERRO] no entry point (_text_start)\n"
		sys.exit(1)

	var_text_end_addr = None
	p = re.compile('(\w*)(\sA\s)(_text_end)$')
	for line in lines:
		m = p.search(line)
		if m != None:
			var_text_end_addr = m.group(1)

	if var_text_end_addr == None:
		print "[ERRO] no entry point (_text_end)\n"
		sys.exit(1)

	byte2=0x00
	byte3=0x00
	data_bin = struct.pack('<BBBBI',BIN_MAGIC_FLASH,2,byte2,byte3,long(entry_addr,16))
	sum_size = len(data_bin)
	write_file(flash_bin_name,data_bin)

	# text.bin
	combine_bin(text_bin_name,flash_bin_name,long(var_text_start_addr,16),1)

	# rodata.bin
	combine_bin(rodata_bin_name,flash_bin_name,long(rodata_start_addr,16),1)

	# write checksum header
	sum_size = os.path.getsize(flash_bin_name) + 1
	sum_size = flash_data_line - (data_line_bits&sum_size)
	if sum_size:
		data_str = ['00']*(sum_size)
		data_bin = binascii.a2b_hex(''.join(data_str))
		write_file(flash_bin_name,data_bin)
	write_file(flash_bin_name,chr(chk_sum & 0xFF))

	#print the information about memory
	var_text_len	= int(var_text_end_addr, 16) - int(var_text_start_addr, 16)
	var_rodata_len	= int(var_rodata_end_addr, 16) - int(var_rodata_start_addr, 16)
	var_bss_len		= int(var_bss_end_addr, 16) - int(var_bss_start_addr, 16)
	sram_usado = var_rodata_len + var_bss_len
	sram_sobra = int("0x1000", 16) - sram_usado
	iram_usado = var_text_len
	iram_sobra = int("0x4000", 16) - iram_usado
	print "=================================================================="
	print "[.rodata",var_rodata_len,"Bytes][.bss",var_bss_len,"Bytes]"
	print "------------------------------------------------------------------"
	print "[SRAM (.rodata):",sram_usado,"Bytes][SRAM (.bss):",sram_sobra,"Bytes]"
	print "------------------------------------------------------------------"
	print "[IRAM USED:",iram_usado,"Bytes][IRAM LIB:",iram_sobra,"Bytes]"
	print "=================================================================="

if __name__=='__main__':
	gen_appbin()
