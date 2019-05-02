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

#include "osapi.h"
#include "mem.h"
#include "at_custom.h"
#include "user_interface.h"
#include "espconn.h"
#ifdef AT_UPGRADE_SUPPORT
#include "at_upgrade.h"
#endif
#include "driver/uart.h"
#include "at_extra_cmd.h"

#define TCPCONN_MAX_CONN            5
#define TCP_INPUT_TIMEOUT_MS        10000
#define TCP_MAX_CERTS               1
#define TCPINPUT_FINISH_TYPE_SEND   0
#define TCPINPUT_FINISH_TYPE_LOAD   1
#define TCPINPUT_TERMINATE_CHAR     '^'
#define TCP_CERT_HEAD_SIZE          32
#define TCP_CERT_LEN_SIZE           2
#define TCP_DATA_TIMEOUT_MS         5000

typedef struct {
    uint8_t ssl;
    uint8_t connected;
    uint16_t keepalive;
    uint16_t port;
    struct espconn *conn;
    ip_addr_t ip;
} tcpconn_t;

static uint8_t tcp_sslconfig = 0;
static tcpconn_t *tcpconns[TCPCONN_MAX_CONN] = { NULL };

static uint8_t *tcp_input_buf = NULL;
static uint8_t tcp_input_conn_no = 0;
static uint8_t tcp_input_finish_type = 0;
static uint16_t tcp_input_len = 0;
static uint16_t tcp_input_buf_len = 0;
static uint16_t tcp_input_buf_idx = 0;
static os_timer_t tcp_timer;

//-------------------------------------------------------------
static const char* ICACHE_FLASH_ATTR flashmap_desc(uint8_t map)
{
    switch(map) {
        case FLASH_SIZE_8M_MAP_512_512:
            #if (SPI_FLASH_SIZE_MAP_EX == 1)
            return "512K NO OTA";
            #else
            return "1MB 512+512";
            #endif
        case FLASH_SIZE_16M_MAP_512_512:
            return "2MB 512+512";
        case FLASH_SIZE_32M_MAP_512_512:
            return "4MB 512+512";
        case FLASH_SIZE_16M_MAP_1024_1024:
            return "2MB 1024+1024";
        case FLASH_SIZE_32M_MAP_1024_1024:
            return "4MB 1024+1024";
        default:
            return "Unsupported";
    }
}

//====================================================
void ICACHE_FLASH_ATTR at_queryCmdFlashMap(uint8_t id)
{
    uint8_t buffer[64] = {0};
#ifdef AT_CUSTOM_UPGRADE
    char esp_name[8] = {'\0'};
    int32_t curr_part = get_rtc_curr_fw();
    if (curr_part < 0) {
        at_response_error();
        return;
    }
    if (!read_boot_info()) {
        at_response_error();
        return;
    }

    if (boot_info->part_type[curr_part] == 0x23) os_sprintf(esp_name, "esp8285");
    else os_sprintf(esp_name, "esp8266");
    os_sprintf(buffer, "+SYSFLASHMAP:%d,%d,\"%s %s\"\r\n",
               boot_info->part_type[curr_part] >> 4, boot_info->part_type[curr_part] & 0x0F, esp_name, flashmap_desc(boot_info->part_type[curr_part] >> 4));
    free_boot_info();
#else
    uint8_t fmap = system_get_flash_size_map();
    os_sprintf(buffer, "+SYSFLASHMAP:%d,\"%s\"\r\n", fmap, flashmap_desc(fmap));
#endif
    at_port_print(buffer);
    at_response_ok();

}

// Query CPU frequency
//======================================================
void ICACHE_FLASH_ATTR at_queryCmdSysCPUfreq(uint8_t id)
{
    uint8_t buffer[8] = {0};
    uint8_t freq = system_get_cpu_freq();
    os_sprintf(buffer, "+SYSCPUFREQ:%d\r\n", freq);
    at_port_print(buffer);
    at_response_ok();
}

// Set the CPU frequency, 80 or 160 MHz
//================================================================
void ICACHE_FLASH_ATTR at_setupCmdCPUfreq(uint8_t id, char *pPara)
{
    uint32_t freq = 0, err = 0, flag = 0;
    uint8_t buffer[40] = {0};
    pPara++; // skip '='

    //get the first parameter (cpu freq), digit
    flag = at_get_next_int_dec(&pPara, &freq, &err);
    if (*pPara != '\r') {
        at_response_error();
        return;
    }
    if ((freq != 80) && (freq != 160)) {
        at_response_error();
        return;
    }

    if (system_update_cpu_freq(freq) == 0) {
        at_response_error();
        return;
    }

    at_response_ok();
}

#include <time.h>
struct tm * sntp_localtime(const time_t * tim_p);


// Query SNTP time, return UNIX time and ANSI date time string
//====================================================
void ICACHE_FLASH_ATTR at_queryCmdSNTPTime(uint8_t id)
{
    uint8_t buffer[64] = {0};
    char timebuf[80];
    uint32_t sntp_time = sntp_get_current_timestamp();
    if (sntp_time == 0) {
        at_port_print_irom_str("+SNTPTIME:Enable SNTP first (AT+CIPSNTPCFG)\r\n");
        at_response_error();
        return;
    }
    struct tm *info = sntp_localtime((const time_t *)&sntp_time);

    os_sprintf(buffer, "+SNTPTIME:%u,%04d-%02d-%02d %02d:%02d:%02d\r\n",
            sntp_time, info->tm_year+1900, info->tm_mon+1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
    at_port_print(buffer);
    at_response_ok();
}

//===================================================
void ICACHE_FLASH_ATTR at_testCmdSNTPTime(uint8_t id)
{
    at_port_print_irom_str("+SNTPTIME:<unix_timestamp>,YYYY-MM-DD HH:NN:SS\r\n");
    at_response_ok();
}


// ===== New TCP connection support ==========================================================

//-----------------------------------------------------------------------
void ICACHE_FLASH_ATTR uart0_tx_buffer(uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++) {
        uart_tx_one_char(UART0, buf[i]);
    }
}

// called when connection receives data
// send alert message to master and wait for request
//--------------------------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_recvcb(void *arg, char *pusrdata, unsigned short length)
{
    uint8_t tcp_n;
    struct espconn *conn = (struct espconn *)arg;
    char info[32] = {'\0'};
    uint8_t ch = 0;
    int tmo = TCP_DATA_TIMEOUT_MS;

    for (tcp_n=0; tcp_n<TCPCONN_MAX_CONN; tcp_n++) {
        if (tcpconns[tcp_n]->conn == conn) break;
    }

    // send the request prompt
    os_sprintf(info, "\r\n+TCP,%d,%d:", tcp_n, length);
    at_port_print(info);
    // wait for request
    system_soft_wdt_stop();
    while (tmo > 0) {
        os_delay_us(1000);
        //ch = uart_rx_one_char_block(); // blocking
        //system_soft_wdt_feed();
        ch = 0;
        uart_rx_one_char(&ch);
        if ((ch == 'R') || (ch == 'A')) break;
        tmo--;
    }
    system_soft_wdt_restart();
    if ((tmo == 0) || (ch == 'A')) {
        // timeout receiving confirmation or abort requested
        if ((tcpconns[tcp_n]) && (tcpconns[tcp_n]->connected)) {
            if (tcpconns[tcp_n]->ssl > 0) espconn_secure_disconnect(conn);
            else espconn_disconnect(conn);
        }
    }
    else {
        uart0_tx_buffer(pusrdata, length);
    }

    tmo = 1000;
    system_soft_wdt_stop();
    while (tmo > 0) {
        os_delay_us(1000);
        ch = 0;
        uart_rx_one_char(&ch);
        if ((ch == 'r') || (ch == 'A')) break;
        tmo--;
    }
    system_soft_wdt_restart();
}

// Free memory used by tcpconn structure
//----------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_cleanup(uint8_t tcp_n)
{
    os_free(tcpconns[tcp_n]->conn->proto.tcp);
    os_free(tcpconns[tcp_n]->conn);
    os_free(tcpconns[tcp_n]);
    tcpconns[tcp_n] = NULL;
}

// Disconnect callback
//-------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_disconcb(void *arg)
{
    uint8_t tcp_n;
    char info[16] = {'\0'};
    struct espconn *conn = (struct espconn *)arg;
    for (tcp_n=0; tcp_n<TCPCONN_MAX_CONN; tcp_n++) {
        if (tcpconns[tcp_n]->conn == conn) break;
    }

    tcpconn_cleanup(tcp_n);

    os_sprintf(info, "%d,CLOSED\r\n", tcp_n);
    at_port_print(info);
}

// call back for lost connection
//----------------------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_recon_cb(void *arg, sint8 errType)
{
    uint8_t tcp_n;
    char info[16] = {'\0'};
    struct espconn *conn = (struct espconn *)arg;
    for (tcp_n=0; tcp_n<TCPCONN_MAX_CONN; tcp_n++) {
        if (tcpconns[tcp_n]->conn == conn) break;
    }

    if (!tcpconns[tcp_n]->connected) {
        at_leave_special_state();
        at_response_error();
    }
    else {
        os_sprintf(info, "%d,CLOSED\r\n", tcp_n);
        at_port_print(info);
    }
    tcpconn_cleanup(tcp_n);
}

// successfully connected
//---------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_connect_cb(void *arg)
{
    uint8_t tcp_n;
    struct espconn *conn = (struct espconn *)arg;
    char info[16] = {'\0'};
    for (tcp_n=0; tcp_n<TCPCONN_MAX_CONN; tcp_n++) {
        if (tcpconns[tcp_n]->conn == conn) break;
    }

    // register connection callbacks
    espconn_regist_disconcb(conn, tcpconn_disconcb);
    espconn_regist_recvcb(conn, tcpconn_recvcb);
    if (tcpconns[tcp_n]->keepalive) {
        espconn_set_keepalive(tcpconns[tcp_n]->conn, ESPCONN_KEEPIDLE, &tcpconns[tcp_n]->keepalive);
    }

    tcpconns[tcp_n]->connected = 1;
    os_sprintf(info, "%d,CONNECT\r\n", tcp_n);
    at_port_print(info);

    at_leave_special_state();
    at_response_ok();
}

// call back for DNS lookup
//----------------------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_resolved(const char *name, ip_addr_t *ip, void *arg)
{
    struct espconn *conn = (struct espconn *)arg;
    uint8_t tcp_n;
    for (tcp_n=0; tcp_n<TCPCONN_MAX_CONN; tcp_n++) {
        if (tcpconns[tcp_n]->conn == conn) break;
    }

    if (ip == 0) {
        // Domain name not resolved, exit with error
        tcpconn_cleanup(tcp_n);
        at_leave_special_state();
        at_response_error();
        return;
    }

    // set up the connection
    conn->type = ESPCONN_TCP;
    conn->state = ESPCONN_NONE;
    conn->proto.tcp->local_port = espconn_port();
    conn->proto.tcp->remote_port = tcpconns[tcp_n]->port;
    *(ip_addr_t*)conn->proto.tcp->remote_ip = *ip;

    // set connection options
    /*
    ESPCONN_REUSEADDR = 0x01,   free memory after TCP disconnection. Need not wait for 2 minutes
    ESPCONN_NODELAY = 0x02,     disable nagle algorithm during TCP data transmission, thus quickening the data transmission
    ESPCONN_COPY = 0x04,        enable espconn_regist_write_finish. Enter write finish callback once the data has been sent using espconn_send
    ESPCONN_KEEPALIVE = 0x08,   enable TCP keep alive
    */
    espconn_set_opt(conn, 0x0B); // ESPCONN_REUSEADDR | ESPCONN_NODELAY | ESPCONN_KEEPALIVE
    // set connection callbacks
    espconn_regist_connectcb(conn, tcpconn_connect_cb);
    espconn_regist_reconcb(conn, tcpconn_recon_cb);

    // try to connect
    if (tcpconns[tcp_n]->ssl > 0) espconn_secure_connect(conn);
    else espconn_connect(conn);
}

//AT+TCPSTART=<link ID>,<type>,<remote IP>,<remoteport>[,<TCP keep alive>]
//=======================================================================
void ICACHE_FLASH_ATTR at_setupCmdTCPConnConnect(uint8_t id, char *pPara)
{
    int port = 0;
    int err = 0, flag = 0;
    int conn_no = 0, ssl = 0, keepalive = 0;
    char type[4] = {0};
    char domain[32] = {0};
    err_t result;

    pPara++; // skip '='

    //get the 1st parameter (conn number)
    flag = at_get_next_int_dec(&pPara, &conn_no, &err);
    if (err != 0) goto exit_err;
    if ((conn_no < 0) || (conn_no >= TCPCONN_MAX_CONN)) goto exit_err;
    if (tcpconns[conn_no] != NULL) goto exit_err;
    if (*pPara != ',') goto exit_err;
    pPara++; // skip ','

    //get the 2nd parameter (connection type), string
    flag = at_data_str_copy(type, &pPara, 3);
    if (flag != 3) goto exit_err;
    if (os_memcmp(type, "TCP", 3) == 0) ssl = 0;
    else if (os_memcmp(type, "SSL", 3) == 0) ssl = 1;
    else  goto exit_err;
    if (*pPara != ',') goto exit_err;
    pPara++; // skip ','

    //get the 3rd parameter (domain), string
    flag = at_data_str_copy(domain, &pPara, 31);
    if (flag < 3) goto exit_err;
    if (*pPara != ',') goto exit_err;
    pPara++; // skip ','

    //get the 4th parameter (port)
    flag = at_get_next_int_dec(&pPara, &port, &err);
    if (err != 0) goto exit_err;
    if ((port < 1) || (port > 65365)) goto exit_err;

    // check if the last parameter
    if (*pPara == ',') {
        pPara++; // skip ','
        //get the optional 5th parameter (keepalive)
        flag = at_get_next_int_dec(&pPara, &keepalive, &err);
        if (err != 0) goto exit_err;
        if ((keepalive < 0) || (keepalive >= 7200)) goto exit_err;
    }
    if (*pPara != '\r') goto exit_err;

    // create tcpconn structure
    tcpconn_t *tcpconn = (tcpconn_t *)os_zalloc(sizeof(tcpconn_t));
    if (!tcpconn) goto exit_err;

    // create connection
    tcpconn->conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
    if (!tcpconn->conn) {
        os_free(tcpconn);
        goto exit_err;
    }
    tcpconn->conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    if (!tcpconn->conn->proto.tcp) {
        os_free(tcpconn->conn);
        os_free(tcpconn);
        goto exit_err;
    }

    tcpconns[conn_no] = tcpconn;
    tcpconn->port = port;
    tcpconn->connected = 0;
    tcpconn->keepalive = keepalive;
    tcpconn->ssl = ssl;

    if (ssl) {
        if (tcp_sslconfig & 2) espconn_secure_ca_enable(1, SYSTEM_PARTITION_SSL_CLIENT_CA_ADDR / SECTOR_SIZE);
        else espconn_secure_ca_disable(1);
    }

    at_enter_special_state();
    // DNS lookup
    result = espconn_gethostbyname(tcpconn->conn, domain, &tcpconn->ip, tcpconn_resolved);
    if (result == ESPCONN_OK) {
        // host name is already cached or is actually a dotted decimal IP address
        // call tcpconn_resolved to start the connection
        tcpconn_resolved(0, &tcpconn->ip, tcpconn->conn);
    }
    else if (result == ESPCONN_INPROGRESS) {
        // lookup taking place, will call tcpconn_resolved on completion
    }
    else {
        // error
        tcpconn_cleanup(conn_no);
        at_leave_special_state();
        goto exit_err;
    }

    return;

exit_err:
    at_response_error();
    return;
}

//AT+TCPCLOSE=<link ID>
//=================================================================
void ICACHE_FLASH_ATTR at_setupCmdTCPClose(uint8_t id, char *pPara)
{
    int tcp_n = 0, err = 0, flag = 0;

    pPara++; // skip '='

    //get the 1st parameter (conn number)
    flag = at_get_next_int_dec(&pPara, &tcp_n, &err);
    if (err != 0) goto exit_err;
    if ((tcp_n < 0) || (tcp_n >= TCPCONN_MAX_CONN)) goto exit_err;
    if (tcpconns[tcp_n] == NULL) goto exit_err;

    // check if the last parameter
    if (*pPara != '\r') goto exit_err;

    if (tcpconns[tcp_n]->connected) {
        if (tcpconns[tcp_n]->ssl > 0) espconn_secure_disconnect(tcpconns[tcp_n]->conn);
        else espconn_disconnect(tcpconns[tcp_n]->conn);
    }

    at_response_ok();
    return;

exit_err:
    at_response_error();
    return;
}

// input from UART has finished
// process according to input type
//----------------------------
static void tcp_input_finish()
{
    bool is_ok = true;
    if (tcp_input_finish_type == TCPINPUT_FINISH_TYPE_SEND) {
        if (tcpconns[tcp_input_conn_no]->ssl > 0) espconn_secure_send(tcpconns[tcp_input_conn_no]->conn, tcp_input_buf, tcp_input_len);
        else espconn_send(tcpconns[tcp_input_conn_no]->conn, tcp_input_buf, tcp_input_len);
    }
    else if (tcp_input_finish_type == TCPINPUT_FINISH_TYPE_LOAD) {
        // Create header and length in RAM sector
        os_sprintf(tcp_input_buf, "TLS.ca_x509.cer");
        uint16_t *clen = (uint16_t *)(tcp_input_buf + TCP_CERT_HEAD_SIZE);
        *clen = tcp_input_len;

        // Create RAM buffer
        espconn_in_ram_sector.buffer = (uint8_t *)os_zalloc(tcp_input_len+TCP_CERT_HEAD_SIZE+TCP_CERT_LEN_SIZE+2);
        if (espconn_in_ram_sector.buffer) {
            os_memcpy(espconn_in_ram_sector.buffer, tcp_input_buf, tcp_input_len+TCP_CERT_HEAD_SIZE+TCP_CERT_LEN_SIZE);
            espconn_in_ram_sector.sector = SYSTEM_PARTITION_SSL_CLIENT_CA_ADDR / SECTOR_SIZE;
            espconn_in_ram_sector.size = tcp_input_len+TCP_CERT_HEAD_SIZE+TCP_CERT_LEN_SIZE+1;
        }
        else is_ok = false;
    }

    os_free(tcp_input_buf);
    os_timer_disarm(&tcp_timer);
    tcp_input_buf = NULL;
    // change UART0 Rx back for AT
    at_register_uart_rx_intr(NULL);
    if (is_ok) at_response_ok();
    else at_response_error();
    at_leave_special_state();
}

// input waiting timeout
//-----------------------------
static void tcp_input_timeout()
{
    os_timer_disarm(&tcp_timer);

    if (tcp_input_finish_type == TCPINPUT_FINISH_TYPE_LOAD) {
        espconn_in_ram_sector.sector = 0;
        espconn_in_ram_sector.size = 0;
        espconn_in_ram_sector.buffer = NULL;
    }
    os_free(tcp_input_buf);
    tcp_input_buf = NULL;
    // change UART0 Rx back for AT
    at_register_uart_rx_intr(NULL);
    at_response_error();
    at_leave_special_state();
}

// UART interrupt routine for user input
//---------------------------------------------------
static void user_uart_rx_intr(uint8* data, int32 len)
{
    char buf[64] = {0};
    uint8_t *end_ptr;
    uint8_t *start_ptr = tcp_input_buf;
    uint16_t max_len = tcp_input_buf_len - tcp_input_buf_idx;
    uint16_t cpy_len = (max_len > len) ? len : max_len;
    // for load certificate type input start has an offset
    if (tcp_input_finish_type == TCPINPUT_FINISH_TYPE_LOAD)
        start_ptr = tcp_input_buf+TCP_CERT_HEAD_SIZE+TCP_CERT_LEN_SIZE;

    // Copy received data to input buffer
    if ((tcp_input_buf) && (tcp_input_buf_len > 0) && (cpy_len > 0)) {
        os_memcpy(tcp_input_buf+tcp_input_buf_idx, data, cpy_len);
        tcp_input_buf_idx += cpy_len;
    }
    // Check if finished
    if (tcp_input_len == 0) {
        // check for terminating character
        end_ptr = os_strchr(start_ptr, TCPINPUT_TERMINATE_CHAR);
        if (end_ptr != NULL) {
            tcp_input_len = (uint16_t)(end_ptr - start_ptr);
            *end_ptr = 0;
            tcp_input_finish();
        }
    }
    else if (tcp_input_buf_idx >= tcp_input_len) {
        tcp_input_finish();
    }
}

//AT+TCPSEND=<link ID>[,<length>]
//================================================================
void ICACHE_FLASH_ATTR at_setupCmdTCPSend(uint8_t id, char *pPara)
{
    int tcp_n = 0, len = 0, err = 0, flag = 0;

    pPara++; // skip '='

    //get the 1st parameter (conn number)
    flag = at_get_next_int_dec(&pPara, &tcp_n, &err);
    if (err != 0) goto exit_err;
    if ((tcp_n < 0) || (tcp_n >= TCPCONN_MAX_CONN)) goto exit_err;
    if (tcpconns[tcp_n] == NULL) goto exit_err;

    if (*pPara == ',') {
        pPara++; // skip ','
        //get the 2nd parameter (length)
        flag = at_get_next_int_dec(&pPara, &len, &err);
        if (err != 0) goto exit_err;
        if ((len < 0) || (len > 2048)) goto exit_err;
    }
    // check if the last parameter
    if (*pPara != '\r') goto exit_err;

    if (tcp_input_buf != NULL) {
        os_free(tcp_input_buf);
        tcp_input_buf = NULL;
    }
    // Create input buffer
    if (len > 0) {
        tcp_input_buf_len = len;
        tcp_input_buf = (uint8_t *)os_zalloc(len);
    }
    else {
        tcp_input_buf_len = 512;
        tcp_input_buf = (uint8_t *)os_zalloc(512);
    }
    if (!tcp_input_buf) goto exit_err;

    tcp_input_finish_type = TCPINPUT_FINISH_TYPE_SEND;
    tcp_input_buf_idx = 0;
    tcp_input_len = len;
    tcp_input_conn_no = tcp_n;

    at_port_print_irom_str(">");

    os_timer_setfn(&tcp_timer, (os_timer_func_t *)tcp_input_timeout, 0);
    os_timer_arm(&tcp_timer, TCP_INPUT_TIMEOUT_MS, 0);

    at_register_uart_rx_intr(user_uart_rx_intr);

    at_enter_special_state();
    return;

exit_err:
    at_response_error();
    return;
}

// AT+TCPCONNECT? or AT+TCPSEND? or AT+TCPCLOSE?
// Used to confirm the TCP commands are implemented
//===============================================
void ICACHE_FLASH_ATTR at_queryCmdTCP(uint8_t id)
{
    at_port_print_irom_str("+TCPCOMMANDS:1\r\n");

    at_response_ok();
    return;
}

/*
  bit0: if set to 1, certificate and private key will be enabled,
        so SSL server can verify ESP8266; if 0, then will not.
  bit1: if set to 1, CA will be enabled, so ESP8266 can verify SSL server,
        if 0, then will not.
*/
//AT+TCPSSLCONFIG=<cfg>
//=====================================================================
void ICACHE_FLASH_ATTR at_setupCmdTCPSSLconfig(uint8_t id, char *pPara)
{
    int cfg = 0, err = 0, flag = 0;

    pPara++; // skip '='

    //get the 1st parameter (config)
    flag = at_get_next_int_dec(&pPara, &cfg, &err);
    if (err != 0) goto exit_err;
    // check if the last parameter
    if (*pPara != '\r') goto exit_err;
    if ((cfg < 0) || (cfg > 3)) goto exit_err;

    tcp_sslconfig = cfg;

    at_response_ok();
    return;

exit_err:
    at_response_error();
    return;
}

//AT+SSLCCONF?
//========================================================
void ICACHE_FLASH_ATTR at_queryCmdTCPSSLconfig(uint8_t id)
{
    char buf[32] = {'\0'};

    os_sprintf(buf, "+SSLCCONF:%d\r\n", tcp_sslconfig);
    at_port_print(buf);

    at_response_ok();
    return;
}

//AT+SSLLOADCERT=<cert_no>[,<length>]
//====================================================================
void ICACHE_FLASH_ATTR at_setupCmdTCPLoadCert(uint8_t id, char *pPara)
{
    int cert_n = 0, len = -1, err = 0, flag = 0;

    pPara++; // skip '='

    //get the 1st parameter (certificate number)
    flag = at_get_next_int_dec(&pPara, &cert_n, &err);
    if (err != 0) goto exit_err;
    if ((cert_n < 0) || (cert_n >= TCP_MAX_CERTS)) goto exit_err;

    if (*pPara == ',') {
        pPara++; // skip ','
        //get the 2nd parameter (length)
        flag = at_get_next_int_dec(&pPara, &len, &err);
        if (err != 0) goto exit_err;
        if ((len < 0) || (len > (SECTOR_SIZE-TCP_CERT_HEAD_SIZE-TCP_CERT_LEN_SIZE))) goto exit_err;
    }
    // check if the last parameter
    if (*pPara != '\r') goto exit_err;

    // Delete buffers if exists from previous command
    if (tcp_input_buf != NULL) os_free(tcp_input_buf);
    tcp_input_buf = NULL;
    if (espconn_in_ram_sector.buffer) os_free(espconn_in_ram_sector.buffer);
    espconn_in_ram_sector.sector = 0;
    espconn_in_ram_sector.size = 0;
    espconn_in_ram_sector.buffer = NULL;
    if (len == 0) {
        at_response_ok();
        return;
    }
    if (len < 0) len = 0;

    // Create input buffer
    tcp_input_buf = (uint8_t *)os_zalloc(SECTOR_SIZE);
    if (!tcp_input_buf) goto exit_err;

    tcp_input_buf_len = SECTOR_SIZE-1;
    tcp_input_finish_type = TCPINPUT_FINISH_TYPE_LOAD;
    tcp_input_buf_idx = TCP_CERT_HEAD_SIZE+TCP_CERT_LEN_SIZE;
    tcp_input_len = len;

    os_timer_setfn(&tcp_timer, (os_timer_func_t *)tcp_input_timeout, 0);
    os_timer_arm(&tcp_timer, TCP_INPUT_TIMEOUT_MS, 0);
    at_port_print_irom_str("\r\n>");

    at_register_uart_rx_intr(user_uart_rx_intr);

    at_enter_special_state();
    return;

exit_err:
    at_response_error();
    return;
}

//AT+TCPLOADCERT?
//=======================================================
void ICACHE_FLASH_ATTR at_queryCmdTCPLoadCert(uint8_t id)
{
    if ((espconn_in_ram_sector.sector > 0) && (espconn_in_ram_sector.buffer)) {
        char buf[32] = {'\0'};
        uint16_t *clen = (uint16_t *)(espconn_in_ram_sector.buffer + TCP_CERT_HEAD_SIZE);

        os_sprintf(buf, "+SSLLOADCERT:0,%04X,%d\r\n", espconn_in_ram_sector.sector, *clen);
        at_port_print(buf);
        at_port_print(espconn_in_ram_sector.buffer+TCP_CERT_HEAD_SIZE+TCP_CERT_LEN_SIZE);
    }
    else {
        at_port_print_irom_str("+SSLLOADCERT:0\r\n");
    }
    at_response_ok();
}
