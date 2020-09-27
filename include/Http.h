// Http.h

#ifndef _HTTP_h
#define _HTTP_h

#include "Arduino.h"
#include "Common.h"

class Http
{
private:
    static bool isBegin;
    static void handleRoot(WEB_SERVER_REQUEST);
#ifndef DISABLE_MQTT
    static void handleMqtt(WEB_SERVER_REQUEST);
#ifndef DISABLE_MQTT_DISCOVERY
    static void handleDiscovery(WEB_SERVER_REQUEST);
#endif
#endif
    static void handledhcp(WEB_SERVER_REQUEST);
    static void handleScanWifi(WEB_SERVER_REQUEST);
    static void handleWifi(WEB_SERVER_REQUEST);
    static void handleOperate(WEB_SERVER_REQUEST);
    static void handleNotFound(WEB_SERVER_REQUEST);
    static void handleModuleSetting(WEB_SERVER_REQUEST);
    static void handleOTA(WEB_SERVER_REQUEST);
    static void handleGetStatus(WEB_SERVER_REQUEST);
    static void handleUpdate(WEB_SERVER_REQUEST);
#ifdef USE_ESP_ASYNC_WEBSERVER
    static void handleUpdateUpload(WEB_SERVER_REQUEST, String filename, size_t index, uint8_t *data, size_t len, bool final);
#else
    static void handleUpdateUpload(WEB_SERVER_REQUEST);
#endif
    static bool checkAuth(WEB_SERVER_REQUEST);

public:
    static AsyncWebServer *theServer;
    static void begin();
    static void stop();
    static void loop();
    static bool captivePortal(WEB_SERVER_REQUEST);

    static void OTA(String url);
};

#endif
