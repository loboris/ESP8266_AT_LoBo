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

/*
 * Some new AT commands
 * Copyright (c)LoBo 2019
 *
 *
*/

#ifndef __AT_EXTRA_CMD_H__
#define __AT_EXTRA_CMD_H__

#include "c_types.h"

void at_queryCmdFlashMap(uint8_t id);
void at_queryCmdSysCPUfreq(uint8_t id);
void at_setupCmdCPUfreq(uint8_t id, char *pPara);
void at_queryCmdSNTPTime(uint8_t id);
void at_testCmdSNTPTime(uint8_t id);

void at_setupCmdTCPConnConnect(uint8_t id, char *pPara);
void at_setupCmdTCPSend(uint8_t id, char *pPara);
void at_setupCmdTCPClose(uint8_t id, char *pPara);
void at_queryCmdTCP(uint8_t id);

void at_setupCmdTCPSSLconfig(uint8_t id, char *pPara);
void at_queryCmdTCPSSLconfig(uint8_t id);

void at_setupCmdTCPLoadCert(uint8_t id, char *pPara);
void at_queryCmdTCPLoadCert(uint8_t id);

#endif
