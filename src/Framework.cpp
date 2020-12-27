#include "Framework.h"
#include "Module.h"
#include "Rtc.h"
#include "Http.h"
#include "Util.h"

uint16_t Framework::rebootCount = 0;
#ifndef DISABLE_MQTT
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
        module->mqttCallback(topic, (char *)payload, cmnd);
    }

    Led::led(200);
}

void Framework::connectedCallback()
{
    Mqtt::subscribe(Mqtt::getCmndTopic(F("#")));
    Led::blinkLED(40, 8);
    if (module)
    {
        module->mqttConnected();
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
    globalConfig.debug.type = 1;

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
    if (rebootCount == 1)
    {
        Config::readConfig();
        module->resetConfig();
    }
    else if (rebootCount == 2)
    {
        Config::readConfig();
        module->resetConfig();
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
        Mqtt::setClient(WifiMgr::wifiClient);
        Mqtt::mqttSetConnectedCallback(connectedCallback);
        Mqtt::mqttSetLoopCallback(callback);
#endif
        module->init();
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

void Framework::sleepDelay(uint32_t mseconds)
{
    if (mseconds)
    {
        for (uint32_t wait = 0; wait < mseconds; wait++)
        {
            delay(1);
            if (Serial.available())
            {
                break;
            }
#ifdef ESP32
            if (Serial1.available())
            {
                break;
            }
#endif
        }
    }
    else
    {
        delay(0);
    }
}

void Framework::loop()
{
    uint32_t my_sleep = millis();
    if (rebootCount >= 3)
    {
        WifiMgr::loop();
        Http::loop();
        return;
    }

    callModule(FUNC_LOOP);
    if (module)
    {
        module->loop();
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
        if (module)
        {
            module->perSecondDo();
        }
    }

    uint32_t my_activity = millis() - my_sleep;
    if (my_activity < 50)
    {
        sleepDelay(50 - my_activity);
    }
    else
    {
        delay(1);
    }
}
