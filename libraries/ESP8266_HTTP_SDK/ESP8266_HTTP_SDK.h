/*
  ESP8266_HTTP_SDK.h - Simple library for using the expressif SDK to register 
  callback functions to http requests, and return data.
  Created by Dominic Peters, July 6th, 2017.
  Released into the public domain.
*/
#include <stdbool.h>
#ifndef ESP8266_HTTP_SDK
#define ESP8266_HTTP_SDK

#define URLSize 10

extern int _response;
extern int _throttlePos;
extern int _steeringPos;
extern int _steeringTrim;
extern char* _telemData;
extern bool _manualCtrl;

typedef enum ProtocolType {
    GET = 0,
    POST,
    GET_FAVICON
} ProtocolType;

typedef struct URL_Param {
    enum ProtocolType Type;
    char pParam[URLSize][URLSize];
    char pParVal[URLSize][URLSize];
    int nPar;
} URL_Param;

#ifdef __cplusplus
extern "C" {
#endif

void SdkWebServer_Init(int port);
void SdkWebServer_updateData(char* data);
int SdkWebServer_getResponse();
int SdkWebServer_getThrottlePos();
int SdkWebServer_getSteeringPos();
int SdkWebServer_getSteeringTrim();
bool SdkWebServer_ManualCtrl();

#ifdef __cplusplus
}
#endif

#endif