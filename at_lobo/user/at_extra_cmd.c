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

#define TCPCONN_MAX_CONN            5       // maximal number of TCP connections
#define TCPCONN_MAX_SERV            3       // maximal number of TCP servers
#define TCPSERV_CONN_TIMEOUT        120
#define TCPCONN_PARRENT_MASK        0xA0
#define TCP_MAX_CERTS               1

#define TCPINPUT_TERMINATE_CHAR     '^'
#define TCP_CERT_HEAD_SIZE          32
#define TCP_CERT_LEN_SIZE           2
#define TCP_DATA_TIMEOUT_MS         50000   // ms

typedef struct {
    uint8_t         parrent;
    uint8_t         ssl;
    uint8_t         connected;
    uint16_t        keepalive;
    uint16_t        port;
    uint16_t        local_port;
    uint8           remote_ip[4];
    struct espconn  *conn;
    ip_addr_t       ip;
} tcpconn_t;

typedef struct {
    uint8_t         ssl;
    uint8_t         connected;
    uint8_t         maxconn;
    uint16_t        port;
    uint16_t        timeout;
    struct espconn  *conn;
} tcpserver_t;

static uint8_t tcp_sslconfig = 0;
static tcpconn_t *tcpconns[TCPCONN_MAX_CONN] = { NULL };
static tcpserver_t *tcpservers[TCPCONN_MAX_SERV] = { NULL };


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


// ===========================================================================================
// ===== New TCP connection support ==========================================================
// ===========================================================================================


//--------------------------------------------------------------------
static uint8_t ICACHE_FLASH_ATTR _get_tcpn(uint8_t *ip, uint16_t port)
{
    uint8_t tcp_n;
    for (tcp_n=0; tcp_n<TCPCONN_MAX_CONN; tcp_n++) {
        if (tcpconns[tcp_n] != NULL) {
            if ((os_memcmp(tcpconns[tcp_n]->remote_ip, ip, 4) == 0) && (tcpconns[tcp_n]->port == port)) break;
        }
    }
    return tcp_n;
}

//-----------------------------------------------------------------------
void ICACHE_FLASH_ATTR uart0_tx_buffer(uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++) {
        uart_tx_one_char(UART0, buf[i]);
    }
}

// Data sent callback
//-----------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_sendcb(void *arg)
{
    at_response_ok();
    at_leave_special_state();
}

// Data sent callback
//------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_writecb(void *arg)
{
}

// called when connection receives data
// send alert message to master and wait for request
//--------------------------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_recvcb(void *arg, char *pusrdata, unsigned short length)
{
    struct espconn *conn = (struct espconn *)arg;
    char info[32] = {'\0'};
    uint8_t ch = 0;
    int tmo;
    uint8_t srv_n = 9;
    uint8_t tcp_n = _get_tcpn(conn->proto.tcp->remote_ip, conn->proto.tcp->remote_port);

    if (tcp_n >= TCPCONN_MAX_CONN) {
        os_sprintf(info, "\r\nTCPClientERROR:recv,%d:", length);
        at_port_print(info);
        return;
    }
    if ((tcpconns[tcp_n]->parrent & TCPCONN_PARRENT_MASK) == TCPCONN_PARRENT_MASK) {
        srv_n = ((tcpconns[tcp_n]->parrent & 0x07) < TCPCONN_MAX_SERV) ? (tcpconns[tcp_n]->parrent & 0x07) : 9;
    }

    at_enter_special_state();
resend:
    // send the alert message:  '+TCP,<link_id>,<server_id>,<length>
    os_sprintf(info, "\r\n+TCP,%d,%d,%d:", tcp_n, srv_n, length);
    at_port_print(info);

    // wait for request (max 5 seconds)
    tmo = TCP_DATA_TIMEOUT_MS;
    system_soft_wdt_stop();
    while (tmo > 0) {
        os_delay_us(100);
        if (uart_rx_one_char(&ch) == 0) {
            if ((ch == 'r') || (ch == 'a')) break;
        }
        tmo--;
    }
    system_soft_wdt_restart();
    if ((tmo == 0) || (ch == 'a')) goto abort;

    // send data to host
    uart0_tx_buffer(pusrdata, length);

    // wait for confirmation (1 second)
    tmo = 10000;
    system_soft_wdt_stop();
    while (tmo > 0) {
        os_delay_us(100);
        if (uart_rx_one_char(&ch) == 0) {
            if ((ch == 'y') || (ch == 'a') || (ch == 'c')) break;
        }
        tmo--;
    }
    system_soft_wdt_restart();

    if ((tmo == 0) || (ch == 'a')) goto abort;
    if (ch == 'c') {
        at_port_print_irom_str("\r\n+TCPresend\r\n");
        os_delay_us(1000);
        goto resend;
    }
    at_leave_special_state();
    return;

abort:
    // timeout receiving confirmation or abort requested
    if ((tcpconns[tcp_n]) && (tcpconns[tcp_n]->connected)) {
        at_port_print_irom_str("\r\n+TCPabort\r\n");
        if (tcpconns[tcp_n]->ssl > 0) espconn_secure_disconnect(conn);
        else espconn_disconnect(conn);
    }
    at_leave_special_state();
}

// Free memory used by tcpconn structure
//----------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_cleanup(uint8_t tcp_n)
{
    if ((tcp_n < TCPCONN_MAX_CONN) && (tcpconns[tcp_n])) {
        if (tcpconns[tcp_n]->conn->proto.tcp) os_free(tcpconns[tcp_n]->conn->proto.tcp);
        if (tcpconns[tcp_n]->conn) os_free(tcpconns[tcp_n]->conn);
        os_free(tcpconns[tcp_n]);
        tcpconns[tcp_n] = NULL;
    }
}

// Disconnect callback
//-------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_disconcb(void *arg)
{
    char info[16] = {'\0'};
    struct espconn *conn = (struct espconn *)arg;
    uint8_t tcp_n = _get_tcpn(conn->proto.tcp->remote_ip, conn->proto.tcp->remote_port);

    uint8_t srv_n = TCPCONN_MAX_SERV;
    if ((conn->parrent & TCPCONN_PARRENT_MASK) == TCPCONN_PARRENT_MASK) {
        srv_n = conn->parrent & 0x07;
    }
    if ((srv_n < TCPCONN_MAX_SERV) && (tcpservers[srv_n]->connected > 0)) tcpservers[srv_n]->connected--;

    if (tcp_n < TCPCONN_MAX_CONN) {
        tcpconn_cleanup(tcp_n);
        os_sprintf(info, "%d,CLOSED\r\n", tcp_n);
        at_port_print(info);
    }
    else {
        at_port_print_irom_str("\r\nTCPClientERROR:disconn\r\n");
        if (conn->state == ESPCONN_CLOSE) {
            at_port_print_irom_str("9,CLOSED\r\n");
        }
    }
}

// callback for lost connection
//----------------------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_recon_cb(void *arg, sint8 errType)
{
    char info[16] = {'\0'};
    struct espconn *conn = (struct espconn *)arg;
    uint8_t tcp_n = _get_tcpn(conn->proto.tcp->remote_ip, conn->proto.tcp->remote_port);

    at_leave_special_state();
    if (tcp_n < TCPCONN_MAX_CONN) {
        if (tcpconns[tcp_n]->connected == 0) {
            at_response_error();
        }
        else {
            os_sprintf(info, "%d,CLOSED\r\n", tcp_n);
            at_port_print(info);
        }
        tcpconn_cleanup(tcp_n);
    }
    else {
        at_port_print_irom_str("\r\nTCPClientERROR:reconn\r\n");
        if (conn->state == ESPCONN_CLOSE) {
            at_port_print_irom_str("9,CLOSED\r\n");
        }
    }
}

//-------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR _tcpconn_register_cb_cb(struct espconn *conn)
{
    // set connection options
    /*
    ESPCONN_REUSEADDR = 0x01,   free memory after TCP disconnection. Need not wait for 2 minutes
    ESPCONN_NODELAY = 0x02,     disable nagle algorithm during TCP data transmission, thus quickening the data transmission
    ESPCONN_COPY = 0x04,        enable espconn_regist_write_finish. Enter write finish callback once the data has been sent using espconn_send
    ESPCONN_KEEPALIVE = 0x08,   enable TCP keep alive
    */
    espconn_set_opt(conn, 0x0F); // ESPCONN_REUSEADDR | ESPCONN_NODELAY | ESPCONN_KEEPALIVE | ESPCONN_COPY

    // register connection callbacks
    espconn_regist_disconcb(conn, tcpconn_disconcb);
    espconn_regist_recvcb(conn, tcpconn_recvcb);
    espconn_regist_write_finish(conn, tcpconn_writecb);
    espconn_regist_sentcb(conn, tcpconn_sendcb);
}

// TCP Client successfully connected to the remote server
// Used by AT+TCPSTART
//---------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_connect_cb(void *arg)
{
    struct espconn *conn = (struct espconn *)arg;
    char info[16] = {'\0'};
    uint8_t tcp_n = _get_tcpn(conn->proto.tcp->remote_ip, conn->proto.tcp->remote_port);

    if (tcp_n < TCPCONN_MAX_CONN) {
        _tcpconn_register_cb_cb(conn);

        if (tcpconns[tcp_n]->keepalive) {
            espconn_set_keepalive(tcpconns[tcp_n]->conn, ESPCONN_KEEPIDLE, &tcpconns[tcp_n]->keepalive);
        }

        tcpconns[tcp_n]->connected = 1;
        os_sprintf(info, "%d,CONNECT\r\n", tcp_n);
        at_port_print(info);

        at_leave_special_state();
        at_response_ok();
    }
    else {
        at_leave_special_state();
        at_port_print_irom_str("\r\nTCPClientERROR:connect\r\n");
        at_response_error();
    }
}

// call back for DNS lookup
//----------------------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpconn_resolved(const char *name, ip_addr_t *ip, void *arg)
{
    struct espconn *conn = (struct espconn *)arg;
    uint8_t tcp_n;
    for (tcp_n=0; tcp_n<TCPCONN_MAX_CONN; tcp_n++) {
        if (tcpconns[tcp_n] != NULL) {
            if (tcpconns[tcp_n]->parrent == conn->parrent) break;
        }
    }

    if ((tcp_n >= TCPCONN_MAX_CONN) || (ip == 0)) {
        // Domain name not resolved or tcpconn not resolved, exit with error
        tcpconn_cleanup(tcp_n);
        at_leave_special_state();
        if (ip != 0) at_port_print_irom_str("\r\n+TCPSTART:linkError\r\n");
        else at_port_print_irom_str("\r\n+TCPSTART:DNSError\r\n");
        at_response_error();
        return;
    }

    // set up the connection
    conn->type = ESPCONN_TCP;
    conn->state = ESPCONN_NONE;
    if (tcpconns[tcp_n]->local_port == 0) conn->proto.tcp->local_port = espconn_port();
    else conn->proto.tcp->local_port = tcpconns[tcp_n]->local_port;
    conn->proto.tcp->remote_port = tcpconns[tcp_n]->port;
    *(ip_addr_t*)conn->proto.tcp->remote_ip = *ip;
    os_memcpy(tcpconns[tcp_n]->remote_ip, conn->proto.tcp->remote_ip, 4);

    // register connect & reconnect connection callbacks
    espconn_regist_connectcb(conn, tcpconn_connect_cb);
    espconn_regist_reconcb(conn, tcpconn_recon_cb);

    tcpconns[tcp_n]->connected = 0;

    //espconn_tcp_set_max_syn(100); // ToDo: ??

    if (tcpconns[tcp_n]->ssl > 0) espconn_secure_connect(conn);
    else espconn_connect(conn);
}

//AT+TCPSTART=<link ID>,<type>,<remote IP>,<remoteport>[,<TCP keep alive>],[<local_port]
//=======================================================================
void ICACHE_FLASH_ATTR at_setupCmdTCPConnConnect(uint8_t id, char *pPara)
{
    int port = 0, localport = 0;
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
    if (tcpconns[conn_no] != NULL) {
        at_port_print_irom_str("\r\n+TCPSTART:linkUsed\r\n");
        goto exit_err;
    }

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

    // check if more parameters available
    if (*pPara == ',') {
        pPara++; // skip ','
        //get the optional 5th parameter (keepalive)
        flag = at_get_next_int_dec(&pPara, &keepalive, &err);
        if (err != 0) keepalive = 0;
        else if ((keepalive < 0) || (keepalive >= 7200)) goto exit_err;
    }
    // check if more parameters available
    if (*pPara == ',') {
        pPara++; // skip ','
        //get the optional 6th parameter (local port)
        flag = at_get_next_int_dec(&pPara, &localport, &err);
        if (err != 0) goto exit_err;
        if ((localport < 0) || (localport >= 65536) || (localport == port)) goto exit_err;
    }
    // check if the last parameter
    if (*pPara != '\r') goto exit_err;

    // create tcpconn structure
    tcpconn_t *tcpconn = (tcpconn_t *)os_zalloc(sizeof(tcpconn_t));
    if (!tcpconn) goto exit_err_alloc;

    // create connection
    tcpconn->conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
    if (!tcpconn->conn) {
        os_free(tcpconn);
        goto exit_err_alloc;
    }
    tcpconn->conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    if (!tcpconn->conn->proto.tcp) {
        os_free(tcpconn->conn);
        os_free(tcpconn);
        goto exit_err_alloc;
    }

    tcpconn->port = port;
    tcpconn->local_port = localport;
    tcpconn->connected = 0;
    tcpconn->keepalive = keepalive;
    tcpconn->ssl = ssl;
    tcpconn->parrent = TCPCONN_PARRENT_MASK | (TCPCONN_MAX_SERV + conn_no);
    tcpconn->conn->parrent = tcpconn->parrent;
    tcpconns[conn_no] = tcpconn;

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
        at_port_print_irom_str("\r\n+TCPSTART:resolveError\r\n");
        goto exit_err;
    }

    return;

exit_err_alloc:
    at_port_print_irom_str("\r\n+TCPSTART:zallocError\r\n");

exit_err:
    at_response_error();
    return;
}

// new client connected to the TCP Server
//------------------------------------------------------------------
static void ICACHE_FLASH_ATTR tcpserver_client_connect_cb(void *arg)
{
    struct espconn *conn = (struct espconn *)arg;
    uint8_t tcp_n, res;
    char info[48] = {'\0'};
    char ip_addr[16] = {'\0'};
    uint8_t srv_n = TCPCONN_MAX_SERV;

    if ((conn->parrent & TCPCONN_PARRENT_MASK) != TCPCONN_PARRENT_MASK) goto exit;
    srv_n = conn->parrent & 0x07;
    if (srv_n >= TCPCONN_MAX_SERV) goto exit;

    // Find free TCP connection to use for the connected client
    for (tcp_n=0; tcp_n<TCPCONN_MAX_CONN; tcp_n++) {
        if (tcpconns[tcp_n] == NULL) break;
    }
    if (tcp_n >= TCPCONN_MAX_CONN) goto exit; // not found

    if (tcpservers[srv_n]->connected >= tcpservers[srv_n]->maxconn) goto exit; // max number of connections reached

    // register new TCP connection to server
    // create tcpconn structure
    tcpconn_t *tcpconn = (tcpconn_t *)os_zalloc(sizeof(tcpconn_t));
    if (!tcpconn) goto exit;

    tcpconn->connected = 1;
    os_memcpy(tcpconn->remote_ip, conn->proto.tcp->remote_ip, 4);
    os_sprintf(ip_addr, IPSTR, conn->proto.tcp->remote_ip[0], conn->proto.tcp->remote_ip[1], conn->proto.tcp->remote_ip[2], conn->proto.tcp->remote_ip[3]);
    tcpconn->ip.addr = ipaddr_addr(ip_addr);
    tcpconn->port = (uint16_t)conn->proto.tcp->remote_port;
    tcpconn->ssl = tcpservers[srv_n]->ssl;
    tcpconn->parrent = conn->parrent;
    tcpconn->conn = conn;

    res = espconn_regist_time(tcpconn->conn, (uint32_t)tcpservers[srv_n]->timeout, 1);
    if (res != ESPCONN_OK) {
        at_port_print_irom_str("\r\nTCPClientRegTimeERROR\r\n");
    }

    tcpconns[tcp_n] = tcpconn;

    // register connection callbacks
    _tcpconn_register_cb_cb(conn);

    tcpservers[srv_n]->connected++;

    os_sprintf(info, "%d,%d,TCPconnect:%s,%d\r\n", srv_n, tcp_n, ip_addr, tcpconn->port);
    at_port_print(info);

    return;

exit:
    if (srv_n < TCPCONN_MAX_SERV) {
        if (tcpservers[srv_n]->ssl > 0) espconn_secure_disconnect(conn);
        else espconn_disconnect(conn);
    }
    else espconn_disconnect(conn);
    os_sprintf(info, "\r\nTCPServerERROR:conn,%d\r\n", conn->link_cnt);
    at_port_print(info);
}


//AT+TCPSERVER=<serv_id>,<enable>,[<port>],[<ssl>],[<maxconn>],[conn_timeout]
//==================================================================
void ICACHE_FLASH_ATTR at_setupCmdTCPServer(uint8_t id, char *pPara)
{
    int enable = -1;
    int port = 333;
    int ssl = 0;
    int srv_n = 0;
    int maxconn = TCPCONN_MAX_CONN;
    int tmo = TCPSERV_CONN_TIMEOUT;
    int err = 0, flag = 0;
    uint8_t tcp_n;
    uint8_t cli_srv_n;
    err_t result;
    uint8_t res;

    pPara++; // skip '='

    //get the 1st parameter (serv_id)
    flag = at_get_next_int_dec(&pPara, &srv_n, &err);
    if (err != 0) goto exit_err;
    if ((srv_n < 0) || (srv_n >= TCPCONN_MAX_SERV)) goto exit_err;

    if (*pPara != ',') goto exit_err;
    pPara++; // skip ','

    //get the 2nd parameter (enable)
    flag = at_get_next_int_dec(&pPara, &enable, &err);
    if (err != 0) goto exit_err;
    if ((enable < 0) || (enable > 1)) goto exit_err;

    // check if the last parameter
    if (*pPara == ',') {
        pPara++; // skip ','
        //get the optional 3rd parameter (port)
        flag = at_get_next_int_dec(&pPara, &port, &err);
        if (err != 0) port = 333;
        if ((port < 0) || (port > 65535)) port = 333;
    }
    // check if the last parameter
    if (*pPara == ',') {
        pPara++; // skip ','
        //get the optional 4th parameter (ssl)
        flag = at_get_next_int_dec(&pPara, &ssl, &err);
        if (err != 0) ssl = 0;
        if ((ssl < 0) || (ssl > 1)) ssl = 0;
    }
    // check if the last parameter
    if (*pPara == ',') {
        pPara++; // skip ','
        //get the optional 5th parameter (keepalive)
        flag = at_get_next_int_dec(&pPara, &maxconn, &err);
        if (err != 0) maxconn = TCPCONN_MAX_CONN;
        if ((maxconn < 1) || (maxconn > TCPCONN_MAX_CONN)) goto exit_err;
    }
    // check if the last parameter
    if (*pPara == ',') {
        pPara++; // skip ','
        //get the optional 6th parameter (timeout)
        flag = at_get_next_int_dec(&pPara, &tmo, &err);
        if (err != 0) tmo = TCPSERV_CONN_TIMEOUT;
        if ((tmo < 0) || (maxconn > 7200)) goto exit_err;
    }
    if (*pPara != '\r') goto exit_err;

    if (enable == 1) {
        // === Create the new TCP Server ===
        if (tcpservers[srv_n] != NULL) goto exit_err;

        // create tcpconn structure
        tcpserver_t *tcpsrv = (tcpserver_t *)os_zalloc(sizeof(tcpserver_t));
        if (!tcpsrv) goto exit_err;

        // create connection
        tcpsrv->conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
        if (!tcpsrv->conn) {
            os_free(tcpsrv);
            goto exit_err;
        }
        tcpsrv->conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
        if (!tcpsrv->conn->proto.tcp) {
            os_free(tcpsrv->conn);
            os_free(tcpsrv);
            goto exit_err;
        }

        // Configure tcpserver structure
        tcpservers[srv_n] = tcpsrv;
        tcpservers[srv_n]->port = port;
        tcpservers[srv_n]->connected = 0;
        tcpservers[srv_n]->ssl = ssl;
        tcpservers[srv_n]->timeout = tmo;
        tcpservers[srv_n]->maxconn = maxconn;
        tcpservers[srv_n]->conn->parrent = TCPCONN_PARRENT_MASK | srv_n;
        tcpservers[srv_n]->conn->type = ESPCONN_TCP;
        tcpservers[srv_n]->conn->state = ESPCONN_NONE;
        tcpservers[srv_n]->conn->proto.tcp->local_port = port;
        // register client connection callback
        espconn_regist_connectcb(tcpservers[srv_n]->conn, tcpserver_client_connect_cb);

        espconn_tcp_set_max_con_allow(tcpservers[srv_n]->conn, maxconn);
        // Accept connections
        if (ssl) espconn_secure_accept(tcpservers[srv_n]->conn);
        else {
            res = espconn_accept(tcpservers[srv_n]->conn);
            if (res != ESPCONN_OK) {
                at_port_print_irom_str("\r\nTCPServerAcceptERROR\r\n");
                goto exit_err;
            }
        }
    }
    else if (enable == 0) {
        // === Delete the TCP Server ===
        if (tcpservers[srv_n]) {
            // Close all clients
            for (tcp_n=0; tcp_n<TCPCONN_MAX_CONN; tcp_n++) {
                if (tcpconns[tcp_n] != NULL) {
                    if ((tcpconns[tcp_n]->parrent & TCPCONN_PARRENT_MASK) == TCPCONN_PARRENT_MASK) {
                        cli_srv_n = tcpconns[tcp_n]->parrent & 0x07;
                        if (cli_srv_n == srv_n) {
                            if (tcpconns[tcp_n]->connected) {
                                if (tcpconns[tcp_n]->ssl > 0) espconn_secure_disconnect(tcpconns[tcp_n]->conn);
                                else espconn_disconnect(tcpconns[tcp_n]->conn);
                            }
                        }
                    }
                }
            }

            espconn_delete(tcpservers[srv_n]->conn);
            os_free(tcpservers[srv_n]->conn);
            os_free(tcpservers[srv_n]);
            tcpservers[srv_n] = NULL;
        }
        else goto exit_err;
    }
    else goto exit_err;

    at_response_ok();
    return;

exit_err:
    at_response_error();
    return;
}

//AT+TCPSERVER?
//-----------------------------------------------------
void ICACHE_FLASH_ATTR at_queryCmdTCPServer(uint8_t id)
{
    char info[128] = {'\0'};
    char ip_addr[16] = {'\0'};
    uint8_t srv_n, cli_srv_n;
    uint8_t tcp_n, n = 0;

    for (srv_n=0; srv_n<TCPCONN_MAX_SERV; srv_n++) {
        if (tcpservers[srv_n]) n++;
    }
    os_sprintf(info, "+TCPSERVER:%d\r\n", n);
    at_port_print(info);
    if (n > 0) {
        for (srv_n=0; srv_n<TCPCONN_MAX_SERV; srv_n++) {
            if (tcpservers[srv_n]) {
                n++;
                os_sprintf(info, "+TCPSERVER:%d,%d,%d,%d,%d,%d\r\n", srv_n, tcpservers[srv_n]->connected,
                        tcpservers[srv_n]->maxconn, tcpservers[srv_n]->port, tcpservers[srv_n]->ssl, tcpservers[srv_n]->timeout);
                at_port_print(info);
                for (tcp_n=0; tcp_n<TCPCONN_MAX_CONN; tcp_n++) {
                    if (tcpconns[tcp_n] != NULL) {
                        if ((tcpconns[tcp_n]->parrent & TCPCONN_PARRENT_MASK) == TCPCONN_PARRENT_MASK) {
                            cli_srv_n = tcpconns[tcp_n]->parrent & 0x07;
                            if (cli_srv_n == srv_n) {
                                os_sprintf(ip_addr, IPSTR, tcpconns[tcp_n]->conn->proto.tcp->remote_ip[0], tcpconns[tcp_n]->conn->proto.tcp->remote_ip[1],
                                        tcpconns[tcp_n]->conn->proto.tcp->remote_ip[2], tcpconns[tcp_n]->conn->proto.tcp->remote_ip[3]);
                                os_sprintf(info, "+TCPCLIENT:%d,\"%s\",%d\r\n", ip_addr, tcpconns[tcp_n]->conn->proto.tcp->remote_port);
                                at_port_print(info);
                            }
                        }
                    }
                }
            }
        }
    }
    at_response_ok();
}

//AT+TCPSTATUS or AT+TCPSTATUS?
//=====================================================
void ICACHE_FLASH_ATTR at_queryCmdTCPStatus(uint8_t id)
{
    char info[80] = {'\0'};
    char ip_addr[16] = {'\0'};
    uint8_t is_server, n, status, found = 0;
    uint8_t wifi_mode = wifi_get_opmode() & 1;

    for (n=0; n<TCPCONN_MAX_CONN; n++) {
        if (tcpconns[n] != NULL) {
            if (tcpconns[n]->connected) {
                found++;
                break;
            }
        }
    }
    if (wifi_mode == 0) status = 5;
    else {
        status = 2;
        if (found) status = 3;
    }

    os_sprintf(info, "STATUS:%d\r\n", status);
    at_port_print(info);
    if (status == 3) {
        for (n=0; n<TCPCONN_MAX_CONN; n++) {
            if (tcpconns[n] != NULL) {
                is_server = 0;
                if ((tcpconns[n]->parrent & TCPCONN_PARRENT_MASK) == TCPCONN_PARRENT_MASK) {
                    is_server = ((tcpconns[n]->parrent & 0x07) < TCPCONN_MAX_SERV) ? 1 : 0;
                }
                os_sprintf(ip_addr, IPSTR, tcpconns[n]->conn->proto.tcp->remote_ip[0], tcpconns[n]->conn->proto.tcp->remote_ip[1],
                        tcpconns[n]->conn->proto.tcp->remote_ip[2], tcpconns[n]->conn->proto.tcp->remote_ip[3]);
                os_sprintf(info, "+TCPSTATUS:%d,\"TCP\",\"%s\",%d,%d,%d\r\n",
                        n, ip_addr, tcpconns[n]->port, tcpconns[n]->conn->proto.tcp->remote_port, is_server);
                at_port_print(info);
            }
        }
    }

    at_response_ok();
}

//AT+TCPSTATUS=<link_id>
//==================================================================
void ICACHE_FLASH_ATTR at_setupCmdTCPStatus(uint8_t id, char *pPara)
{
    int tcp_n = 0, err = 0, flag = 0;
    char info[40] = {'\0'};
    char ip_addr[16] = {'\0'};
    uint8_t is_server = 0;

    pPara++; // skip '='

    //get the 1st parameter (conn number)
    flag = at_get_next_int_dec(&pPara, &tcp_n, &err);
    if (err != 0) goto exit_err;

    if ((tcp_n < 0) || (tcp_n >= TCPCONN_MAX_CONN)) goto exit_err;
    if (tcpconns[tcp_n] == NULL) goto exit_err;

    // check if the last parameter
    if (*pPara != '\r') goto exit_err;

    is_server = 0;
    if ((tcpconns[tcp_n]->parrent & TCPCONN_PARRENT_MASK) == TCPCONN_PARRENT_MASK) {
        is_server = ((tcpconns[tcp_n]->parrent & 0x07) < TCPCONN_MAX_SERV) ? 1 : 0;
    }
    os_sprintf(ip_addr, IPSTR, tcpconns[tcp_n]->conn->proto.tcp->remote_ip[0], tcpconns[tcp_n]->conn->proto.tcp->remote_ip[1],
            tcpconns[tcp_n]->conn->proto.tcp->remote_ip[2], tcpconns[tcp_n]->conn->proto.tcp->remote_ip[3]);
    os_sprintf(info, "+TCPSTATUS:\"%s\",%d,%d", ip_addr, tcpconns[tcp_n]->conn->proto.tcp->remote_port, is_server);
    at_port_print(info);

    at_response_ok();

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

//-----------------------------------------------------------------------------------------
static uint16_t ICACHE_FLASH_ATTR _request_get_input(uint8_t *buf, uint16_t len, char *msg)
{
    uint16_t idx = 0;
    uint32_t tmo = 500000; // one second timeout
    uint8_t term = 0, ch;
    char info[32] = {0};

    // ==== Send the data request prompt to the client ====
    at_port_print_irom_str("\r\n>");

    // ==== Accept input data into buffer ====
    system_soft_wdt_stop();
    while (tmo) {
        os_delay_us(2);
        tmo--;
        while (uart_rx_one_char(&ch) == 0) {
            if ((len == 0) && (ch == TCPINPUT_TERMINATE_CHAR)) term = 1;
            else {
                // place the received character into the buffer
                buf[idx++] = ch;
                if (idx == len) term = 1;
            }
            if (term) break;
        }
        if (term) break;
    }
    system_soft_wdt_restart();

    os_sprintf(info, "\r\n%s:%d\r\n", msg, idx);
    at_port_print(info);

    if ((tmo == 0) || (idx == 0) || ((len > 0) && (idx != len))) {
        os_free(buf);
        at_port_print_irom_str("\r\nFAIL\r\n");
        idx = 0;
    }
    return idx;
}

//AT+TCPSEND=<link ID>[,<length>]
// <length> > 0       -> load send data of specified length
// <length> not given -> load send data with terminating character at the end
//================================================================
void ICACHE_FLASH_ATTR at_setupCmdTCPSend(uint8_t id, char *pPara)
{
    int tcp_n = 0, len = -1, err = 0, flag = 0;
    char buf[16] = {0};
    uint8_t res;
    char ch;
    uint8_t *tcp_input_buf;
    uint16_t send_len;

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
        if ((len < 1) || (len > 2048)) goto exit_err;
    }
    // check if the last parameter
    if (*pPara != '\r') goto exit_err;

    if (len < 0) len = 0; // load up to terminating character

    // Create input buffer
    tcp_input_buf = (uint8_t *)os_zalloc(len+16);
    if (!tcp_input_buf) goto exit_err;

    at_enter_special_state();
    // Send the ready prompt to the client
    // and accept input data into buffer
    send_len = _request_get_input(tcp_input_buf, len, "+TCPSEND");
    if (send_len == 0) {
        at_leave_special_state();
        return;
    }

    // All expected characters are received, send the buffer
    if (tcpconns[tcp_n]->ssl > 0) res = espconn_secure_send(tcpconns[tcp_n]->conn, tcp_input_buf, send_len);
    else res = espconn_send(tcpconns[tcp_n]->conn, tcp_input_buf, send_len);

    os_free(tcp_input_buf);
    if (res != 0) {
        at_port_print_irom_str("\r\nFAIL\r\n");
        if (tcpconns[tcp_n]->ssl > 0) espconn_secure_disconnect(tcpconns[tcp_n]->conn);
        else espconn_disconnect(tcpconns[tcp_n]->conn);
        at_leave_special_state();
    }

    return;

exit_err:
    at_response_error();
}

//AT+SSLLOADCERT=<cert_no>[,<length>]
// <length> = 0       -> delete the certificate
// <length> > 100     -> load certificate of specified length
// <length> not given -> load certificate with terminating character at the end
//====================================================================
void ICACHE_FLASH_ATTR at_setupCmdTCPLoadCert(uint8_t id, char *pPara)
{
    int cert_n = 0, len = -1, err = 0, flag = 0;
    char buf[24] = {0};
    uint8_t res, term;
    char ch;
    uint8_t *end_ptr;
    uint8_t *tcp_input_buf = NULL;
    uint16_t send_len;

    pPara++; // skip '='

    //get the 1st parameter (certificate number, nut used at the moment)
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

    // Delete current certificate buffer if exists
    if (espconn_in_ram_sector.buffer) os_free(espconn_in_ram_sector.buffer);
    espconn_in_ram_sector.sector = 0;
    espconn_in_ram_sector.size = 0;
    espconn_in_ram_sector.buffer = NULL;
    if (len == 0) {
        at_response_ok();
        return;
    }
    if (len < 0) len = 0; // load up to terminating character

    // Create the certificate buffer
    tcp_input_buf = (uint8_t *)os_zalloc(SECTOR_SIZE);
    if (!tcp_input_buf) goto exit_err;

    // Send the ready prompt to the client
    // and accept input data into buffer at position after the certificate header (34 bytes)
    send_len = _request_get_input(tcp_input_buf+TCP_CERT_HEAD_SIZE+TCP_CERT_LEN_SIZE, len, "+SSLLOADCERT");
    if (send_len == 0) return;

    // Create header and length in RAM sector
    os_sprintf(tcp_input_buf, "TLS.ca_x509.cer");
    uint16_t *clen = (uint16_t *)(tcp_input_buf + TCP_CERT_HEAD_SIZE);
    *clen = send_len;

    espconn_in_ram_sector.buffer = tcp_input_buf;
    espconn_in_ram_sector.sector = SYSTEM_PARTITION_SSL_CLIENT_CA_ADDR / SECTOR_SIZE;
    espconn_in_ram_sector.size = send_len+TCP_CERT_HEAD_SIZE+TCP_CERT_LEN_SIZE+1;

    at_response_ok();

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

// AT+TCPSTART? or AT+TCPSEND? or AT+TCPCLOSE?
// Used to confirm the TCP commands are implemented
//===============================================
void ICACHE_FLASH_ATTR at_queryCmdTCP(uint8_t id)
{
    at_port_print_irom_str("+TCPCOMMANDS:1\r\n");

    at_response_ok();
    return;
}
