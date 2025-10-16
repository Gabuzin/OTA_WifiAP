#ifndef WEB_H
#define WEB_H


#include "OTA.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Update.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>







class Web
{
private:
    
    void pageOTA();
    void pageInfo();
    void pageUpdate();
    bool authEnabled = false;
    String FW_VERSION;

public:
    Web( String fwVersion);
    bool autentication(AsyncWebServerRequest* request);
    void WEB_OTA(bool autenticacao);
};

extern Web web;



#endif