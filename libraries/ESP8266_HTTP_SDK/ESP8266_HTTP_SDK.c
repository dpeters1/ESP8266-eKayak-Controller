/*
  ESP8266_HTTP_SDK.c - Simple library for using the expressif SDK to register 
  callback functions to http requests, and return data.
  Created by Dominic Peters, July 6th, 2017.
  Released into the public domain.
*/

#include "ESP8266_HTTP_SDK.h"
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem_manager.h"
#include "mem.h"
#include "string.h"
#include "user_interface.h"
#include "cont.h"
#include "espconn.h"
#include "eagle_soc.h"
#include <pgmspace.h>
void * pvPortZalloc(int size,char *, int);

LOCAL char *precvbuffer;
static uint32 dat_sumlength = 0;
int _response = 0;
char* _telemData = NULL;
int _throttlePos = 0;
int _steeringPos = 0;
int _steeringTrim = 0;
bool _manualCtrl = true;

/********************************************************
 * SDK API Web Server parse received parameters
 * Function: void SdkWebServer_ parse_url_params(
 *           char *precv, URL_Param *purl_param) 
 * 
 * Parameter    Description
 * ---------    ---------------------------------------
 * *precv       string received
 * *purl_param  parsed parameter structure
 * return       no return value
 ********************************************************/
void SdkWebServer_parse_url_params(char *precv, URL_Param *purl_param)
  {
    char *str = NULL;
    uint8 length = 0;
    char *pbuffer = NULL;
    char *pbufer = NULL;
    int ipar=0;

    if (purl_param == NULL || precv == NULL) {
        return;
    }

    pbuffer = (char *)strstr(precv, "Host:");

    if (pbuffer != NULL) {
        length = pbuffer - precv;
        pbufer = (char *)os_zalloc(length + 1);
        pbuffer = pbufer;
        memcpy(pbuffer, precv, length);
        os_memset(purl_param->pParam, 0, URLSize);
        os_memset(purl_param->pParVal, 0, URLSize);

        if (strncmp(pbuffer, "GET ", 4) == 0) {
            purl_param->Type = GET;
            pbuffer += 4;
        } else if (strncmp(pbuffer, "POST ", 5) == 0) {
            purl_param->Type = POST;
            pbuffer += 5;
        }

        pbuffer ++;
        str = (char *)strstr(pbuffer, "?");

        if (str != NULL) {
            str ++;
            do {
              pbuffer = (char *)os_strstr(str, "=");
              length = pbuffer - str;
              os_memcpy(purl_param->pParam[ipar], str, length);
              str = (char *)os_strstr(++pbuffer, "&");
              if(str != NULL) {
                length = str - pbuffer;
                os_memcpy(purl_param->pParVal[ipar++], pbuffer, length);
                str++;
              }
              else {
                str = (char *)os_strstr(pbuffer, " HTTP");
                if(str != NULL) {
                    length = str - pbuffer;
                    os_memcpy(purl_param->pParVal[ipar++], pbuffer, length);
                    str = NULL;
                }
                else {
                    //os_memcpy(purl_param->pParVal[ipar++], pbuffer, 16);
                }
              }
            }
            while (str!=NULL);
        }

        purl_param->nPar = ipar;
        os_free(pbufer);
    } else {
        return;
    }
}


/********************************************************
 * SDK API Web Server save data from connected client
 * Function: bool SdkWebServer_savedata(char *precv, 
 *                                      uint16 length)
 * 
 * Parameter    Description
 * ---------    ---------------------------------------
 * *precv       string received
 * length       string length
 * return       true if successful save
 ********************************************************/
bool SdkWebServer_savedata(char *precv, uint16 length)
{
    bool flag = false;
    char length_buf[10] = {0};
    char *ptemp = NULL;
    char *pdata = NULL;
    uint16 headlength = 0;
    static uint32 totallength = 0;

    ptemp = (char *)os_strstr(precv, "\r\n\r\n");

    if (ptemp != NULL) {
        length -= ptemp - precv;
        length -= 4;
        totallength += length;
        headlength = ptemp - precv + 4;
        pdata = (char *)os_strstr(precv, "Content-Length: ");

        if (pdata != NULL) {
            pdata += 16;
            precvbuffer = (char *)os_strstr(pdata, "\r\n");

            if (precvbuffer != NULL) {
                os_memcpy(length_buf, pdata, precvbuffer - pdata);
                dat_sumlength = atoi(length_buf);
            }
        } else {
            if (totallength != 0x00){
                totallength = 0;
                dat_sumlength = 0;
                return false;
            }
        }
        if ((dat_sumlength + headlength) >= 1024) {
            precvbuffer = (char *)os_zalloc(headlength + 1);
            os_memcpy(precvbuffer, precv, headlength + 1);
        } else {
            precvbuffer = (char *)os_zalloc(dat_sumlength + headlength + 1);
            os_memcpy(precvbuffer, precv, os_strlen(precv));
        }
    } else {
        if (precvbuffer != NULL) {
            totallength += length;
            os_memcpy(precvbuffer + os_strlen(precvbuffer), precv, length);
        } else {
            totallength = 0;
            dat_sumlength = 0;
            return false;
        }
    }

    if (totallength == dat_sumlength) {
        totallength = 0;
        dat_sumlength = 0;
        return true;
    } else {
        return false;
    }
}

/********************************************************
 * SDK API Web Server send data to connected client
 * Function: SdkWebServer_senddata(void *arg, 
 *                                 bool responseOK, 
 *                                 char *psend)
 * 
 * Parameter    Description
 * ---------    ---------------------------------------
 * *arg         pointer to espconn structure
 * responseOK   reply status
 * *psend       string to send
 * return       no return value
 ********************************************************/
void SdkWebServer_senddata(void *arg, bool responseOK, char *psend)
{
    uint16 length = 0;
    char *pbuf = NULL;
    char httphead[256];
    struct espconn *ptrespconn = arg;
    os_memset(httphead, 0, 256);

    if (responseOK) {
        os_sprintf(httphead,
                   "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\nAccess-Control-Allow-Origin: *\r\n",
                   psend ? os_strlen(psend) : 0);

        if (psend) {
            os_sprintf(httphead + os_strlen(httphead),
                       "Content-type: application/json\r\nExpires: Fri, 10 Apr 2015 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
            length = os_strlen(httphead) + os_strlen(psend);
            pbuf = (char *)os_zalloc(length + 1);
            os_memcpy(pbuf, httphead, os_strlen(httphead));
            os_memcpy(pbuf + os_strlen(httphead), psend, os_strlen(psend));
        } else {
            os_sprintf(httphead + os_strlen(httphead), "\n");
            length = os_strlen(httphead);
        }
    } else {
        os_sprintf(httphead, "HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
        length = os_strlen(httphead);
    }

    if (psend) {
        espconn_sent(ptrespconn, (uint8 *)pbuf, length);
    } else {
        espconn_sent(ptrespconn, (uint8 *)httphead, length);
    }

    if (pbuf) {
        os_free(pbuf);
        pbuf = NULL;
    }
}

/********************************************************
 * SDK API Web Server Receive Data Callback
 * Function: SdkWebServer_recv(void *arg, char *pusrdata, 
 *                          unsigned short length)
 * 
 * Parameter    Description
 * ---------    ---------------------------------------
 * *arg         pointer to espconn structure
 * *pusrdata    data received after IP:port
 * length       data length (bytes)
 * return       no return value
 ********************************************************/
void SdkWebServer_recv(void *arg, char *pusrdata, unsigned short length)
{

    _response = 2;
    URL_Param *pURL_Param = NULL;
    char *pParseBuffer = NULL;
    bool parse_flag = false;
    struct espconn *ptrespconn = arg;
    //String payld = "";
    espconn_set_opt(ptrespconn, ESPCONN_REUSEADDR);

    parse_flag = SdkWebServer_savedata(pusrdata, length);
    pURL_Param = (URL_Param *)os_zalloc(sizeof(URL_Param));

    char * pRx = NULL;
    pRx = (char *)os_zalloc(512);
    os_memcpy(pRx, precvbuffer, length);
     
    SdkWebServer_parse_url_params(precvbuffer, pURL_Param);

    _throttlePos = atoi(pURL_Param->pParVal[1]);
    _steeringPos = atoi(pURL_Param->pParVal[2]);
    _steeringTrim = atoi(pURL_Param->pParVal[3]);

    if(os_strcmp(pURL_Param->pParVal[0], "true")==0) {
        SdkWebServer_senddata(ptrespconn, true, _telemData);
    }
    else { SdkWebServer_senddata(ptrespconn, true, NULL); }

    if(os_strcmp(pURL_Param->pParVal[4], "false")==0) {
        _manualCtrl = false;
    }
    else { _manualCtrl = true; }
    

    if (precvbuffer != NULL){
      os_free(precvbuffer);
      precvbuffer = NULL;
    }
    os_free(pURL_Param);
    pURL_Param = NULL;
    os_free(pRx);
    pRx=NULL;
}

/********************************************************
 * SDK API Web Server TCP Connection Closed Callback
 * Function: SdkWebServer_discon(void *arg)
 * 
 * Parameter    Description
 * ---------    ---------------------------------------
 * *arg         pointer to espconn structure
 * return       no return value
 ********************************************************/
void SdkWebServer_discon(void *arg)
{
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d disconnect\n", pesp_conn->proto.tcp->remote_ip[0],
            pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
            pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);
}

/********************************************************
 * SDK API Web Server TCP Disconnect on error Callback
 * Function: SdkWebServer_recon(void *arg, sint8 err)
 * 
 * Parameter    Description
 * ---------    ---------------------------------------
 * *arg         pointer to espconn structure
 * err          error code
 * return       no return value
 ********************************************************/
void SdkWebServer_recon(void *arg, sint8 err)
{
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d err %d reconnect\n", pesp_conn->proto.tcp->remote_ip[0],
        pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
        pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port, err);
}

/********************************************************
 * SDK API Web Server TCP Client Connection Callback
 * Function: SdkWebServer_listen(void *arg)
 * 
 * Parameter    Description
 * ---------    ---------------------------------------
 * *arg         pointer to espconn structure
 * return       no return value
 ********************************************************/
void SdkWebServer_listen(void *arg)
{
    struct espconn *pesp_conn = arg;

    espconn_regist_recvcb(pesp_conn, SdkWebServer_recv);
    espconn_regist_reconcb(pesp_conn, SdkWebServer_recon);
    espconn_regist_disconcb(pesp_conn, SdkWebServer_discon);
}

/********************************************************
 * SDK API Web Server Initialization
 * Function: SdkWebServer_Init(int port)
 * 
 * Parameter    Description
 * ---------    ---------------------------------------
 * port         http server listen port
 * return       no return value
 ********************************************************/
void SdkWebServer_Init(int port) {
    _response = 1;
    LOCAL struct espconn esp_conn;
    LOCAL esp_tcp esptcp;
    //Fill the connection structure, including "listen" port
    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    esp_conn.proto.tcp->local_port = port;
    esp_conn.recv_callback = NULL;
    esp_conn.sent_callback = NULL;
    esp_conn.reverse = NULL;
    //Register the connection timeout(0=no timeout)
    espconn_regist_time(&esp_conn,0,0);
    //Register connection callback
    espconn_regist_connectcb(&esp_conn, SdkWebServer_listen);
    //Start Listening for connections
    espconn_accept(&esp_conn); 
}

int SdkWebServer_getResponse() {
    return _response;
}

int SdkWebServer_getThrottlePos() {
    return _throttlePos;
}

int SdkWebServer_getSteeringPos() {
    return _steeringPos;
}

int SdkWebServer_getSteeringTrim() {
    return _steeringTrim;
}

bool SdkWebServer_ManualCtrl() {
    return _manualCtrl;
}

void SdkWebServer_updateData(char* telemData) {
    _telemData = telemData;
}