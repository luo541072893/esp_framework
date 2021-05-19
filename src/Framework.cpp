#include "Framework.h"
#include "Module.h"
#include "Rtc.h"
#include "Http.h"
#include "Util.h"

uint16_t Framework::rebootCount = 0;
#ifndef DISABLE_MQTT
#ifndef USE_ASYNC_MQTT_CLIENT
WiFiClient wifiClient;
#endif
void Framework::callback(char *topic, byte *payload, unsigned int length)
{
    Log::Info(PSTR("Subscribe: %s payload: %.*s"), topic, length, payload);

    char *cmnd = strrchr(topic, '/');
    if (cmnd == nullptr)
    {
        return;
    }
    cmnd++;
    payload[length] = 0;

    if (strcmp(cmnd, "ota") == 0)
    {
        String str = String((char *)payload);
        Http::OTA(str.endsWith(F(".bin")) ? str : OTA_URL);
    }
    else if (strcmp(cmnd, "restart") == 0)
    {
        ESP_Restart();
    }
    else if (module)
    {
        Module *ptr = module;
        while (ptr != nullptr)
        {
            ptr->mqttCallback(topic, (char *)payload, cmnd);
            ptr = ptr->next;
        }
    }

    Led::led(200);
}

void Framework::connectedCallback()
{
    Mqtt::subscribe(Mqtt::getCmndTopic(F("#")));
    Led::blinkLED(40, 8);

    callModule(FUNC_MQTT_CONNECTED);

    Module *ptr = module;
    while (ptr != nullptr)
    {
        ptr->mqttConnected();
        ptr = ptr->next;
    }
}
#endif

void Framework::tickerPerSecondDo()
{
    perSecond++;
    if (perSecond == 30)
    {
        Rtc::rtcReboot.fast_reboot_count = 0;
        Rtc::rtcRebootSave();
    }
    if (rebootCount >= 3)
    {
        return;
    }
    Rtc::addSecond();
    bitSet(Config::operationFlag, 0);
}

void Framework::one(unsigned long baud)
{
    Rtc::rtcRebootLoad();
    Rtc::rtcReboot.fast_reboot_count++;
    Rtc::rtcRebootSave();
    rebootCount = Rtc::rtcReboot.fast_reboot_count > BOOT_LOOP_OFFSET ? Rtc::rtcReboot.fast_reboot_count - BOOT_LOOP_OFFSET : 0;

    Serial.begin(baud);
    globalConfig.debug.type = 5;

    addModule(WifiMgr::callModule);
    addModule(Http::callModule);

    if (rebootCount < 3)
    {
        addModule(Led::callModule);
        addModule(Rtc::callModule);
        addModule(Config::callModule);
#ifndef DISABLE_MQTT
        addModule(Mqtt::callModule);
#endif
    }
}

void Framework::setup()
{
    Log::Error(PSTR("---------------------  v%s  %s  %d-------------------"), module->getModuleVersion().c_str(), Rtc::GetBuildDateAndTime().c_str(), rebootCount);
#ifdef USE_UFILESYS
    FileSystem::init();
#endif
    if (rebootCount == 1 || rebootCount == 2)
    {
        Config::readConfig();
        Module *ptr = module;
        while (ptr != nullptr)
        {
            ptr->resetConfig();
            ptr = ptr->next;
        }
    }
    else if (rebootCount == 3)
    {
        Config::resetConfig();
    }
    else
    {
        Config::readConfig();
    }
    if (globalConfig.uid[0] != '\0')
    {
        strcpy(UID, globalConfig.uid);
    }
    else
    {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        sprintf(UID, "%s_%02x%02x%02x", module->getModuleName().c_str(), mac[3], mac[4], mac[5]);
    }
    Util::strlowr(UID);
    Log::Info(PSTR("UID: %s"), UID);

    WifiMgr::connectWifi();
    if (rebootCount >= 3)
    {
        module = NULL;
    }
    else
    {
#ifndef DISABLE_MQTT
#ifndef USE_ASYNC_MQTT_CLIENT
        wifiClient.setTimeout(200);
        Mqtt::setClient(wifiClient);
#endif
        Mqtt::mqttSetConnectedCallback(connectedCallback);
        Mqtt::mqttSetLoopCallback(callback);
#endif
        Module *ptr = module;
        while (ptr != nullptr)
        {
            ptr->init();
            ptr = ptr->next;
        }
        Rtc::init();
    }
    Http::init();
    if (module)
    {
        callModule(FUNC_INIT);
    }

    tickerPerSecond = new Ticker();
    tickerPerSecond->attach(1, tickerPerSecondDo);
}

void Framework::loop()
{
    if (rebootCount >= 3)
    {
        WifiMgr::loop();
        Http::loop();
        return;
    }

    callModule(FUNC_LOOP);
    Module *ptr = module;
    while (ptr != nullptr)
    {
        ptr->loop();
        ptr = ptr->next;
    }

    static uint32_t state_50msecond = 0; // State 50msecond timer
    if (Util::timeReached(state_50msecond))
    {
        Util::setNextTimeInterval(state_50msecond, 50);
        callModule(FUNC_EVERY_50_MSECOND);
    }

    if (bitRead(Config::operationFlag, 0))
    {
        bitClear(Config::operationFlag, 0);
        callModule(FUNC_EVERY_SECOND);
        Module *ptr = module;
        while (ptr != nullptr)
        {
            ptr->perSecondDo();
            ptr = ptr->next;
        }
    }
    delay(1);
}
