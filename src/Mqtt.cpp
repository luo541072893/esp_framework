#ifndef DISABLE_MQTT

#include "Mqtt.h"
#include "Module.h"
#include "Rtc.h"

uint8_t Mqtt::operationFlag = 0;
PubSubClient Mqtt::mqttClient;
uint32_t Mqtt::disconnectCounter = 0;    // 重连次数
uint32_t Mqtt::lastReconnectAttempt = 0; // 最后尝试重连时间
uint32_t Mqtt::kMqttReconnectTime = 30;  // 重新连接尝试之间的延迟（秒）
std::function<void()> Mqtt::connectedcallback = NULL;
#ifdef USE_ASYNC_MQTT_CLIENT
std::function<void(char *, uint8_t *, unsigned int)> Mqtt::callback = NULL;
char mqttWill[80] = {0};
#endif

bool Mqtt::mqttConnect()
{
    if (!WiFi.isConnected())
    {
        Log::Info(PSTR("wifi disconnected"));
        return false;
    }
    if (globalConfig.mqtt.port == 0)
    {
        Log::Info(PSTR("no set mqtt info"));
        return false;
    }
    if (mqttClient.connected())
    {
        return true;
    }

    Log::Info(PSTR("mqtt connect to %s:%d Broker"), globalConfig.mqtt.server, globalConfig.mqtt.port);
#ifdef USE_ASYNC_MQTT_CLIENT
    mqttClient.disconnect(true);
    mqttClient.setClientId(UID);
    mqttClient.setServer(globalConfig.mqtt.server, globalConfig.mqtt.port);
    mqttClient.setCredentials(globalConfig.mqtt.user, globalConfig.mqtt.pass);
    strcpy(mqttWill, getTeleTopic(F("availability")).c_str());
    mqttClient.setWill(mqttWill, 0, true, "offline", 7);
    mqttClient.connect();
#else
    mqttClient.setServer(globalConfig.mqtt.server, globalConfig.mqtt.port);
    if (mqttClient.connect(UID, globalConfig.mqtt.user, globalConfig.mqtt.pass, getTeleTopic(F("availability")).c_str(), 0, true, "offline"))
    {
        bitSet(Config::statusFlag, 1);
        Log::Info(PSTR("mqtt connection successful"));
        availability();
        if (globalConfig.mqtt.interval > 0)
        {
            doReportInfo();
        }
        if (connectedcallback != NULL)
        {
            connectedcallback();
        }
    }
    else
    {
        Log::Info(PSTR("mqtt connection failed, rc=%d"), mqttClient.state());
    }
#endif
    return mqttClient.connected();
}

void Mqtt::doReportInfo()
{
    char message[250];
    sprintf(message, PSTR("{\"uid\":\"%s\",\"ssid\":\"%s\",\"rssi\":\"%s\",\"version\":\"%s\",\"ip\":\"%s\",\"mac\":\"%s\",\"freemem\":%d,\"uptime\":%d,\"buildtime\":\"%s\"}"),
            UID, WiFi.SSID().c_str(), String(WiFi.RSSI()).c_str(), (module ? module->getModuleVersion().c_str() : PSTR("0")), WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(), ESP.getFreeHeap(), millis() / 1000, Rtc::GetBuildDateAndTime().c_str());
    //Log::Info(PSTR("%s"), message);
    publish(getTeleTopic(F("info")), message);
}

void Mqtt::availability()
{
    publish(getTeleTopic(F("availability")), "online", true);
}

void Mqtt::perSecondDo()
{
    if (!WiFi.isConnected() || globalConfig.mqtt.port == 0)
    {
        return;
    }

    if (!mqttClient.connected())
    {
        bitClear(Config::statusFlag, 1);
        if (lastReconnectAttempt == 0 || millis() - lastReconnectAttempt > kMqttReconnectTime * 1000)
        {
            disconnectCounter++;
            lastReconnectAttempt = millis();
            if (mqttConnect())
            {
                lastReconnectAttempt = 0;
            }
        }
    }
    else
    {
        bitSet(Config::statusFlag, 1);
        if (globalConfig.mqtt.interval > 0 && (perSecond % globalConfig.mqtt.interval) == 0)
        {
            doReportInfo();
        }
        if (perSecond % 3609 == 0)
        {
            availability();
        }
    }
}

void Mqtt::loop()
{
    if (!bitRead(Config::statusFlag, 0) || !bitRead(Config::statusFlag, 1))
    {
        return;
    }
#ifndef USE_ASYNC_MQTT_CLIENT
    if (mqttClient.loop())
    {
        bitSet(Config::statusFlag, 1);
    }
    else
    {
        bitClear(Config::statusFlag, 1);
    }
#endif
}

bool Mqtt::callModule(uint8_t function)
{
    switch (function)
    {
    case FUNC_EVERY_SECOND:
        perSecondDo();
        break;
    case FUNC_LOOP:
        loop();
        break;
    }
    return false;
}

String Mqtt::getCmndTopic(String topic)
{
    return getTopic(0, topic);
}

String Mqtt::getStatTopic(String topic)
{
    return getTopic(1, topic);
}

String Mqtt::getTeleTopic(String topic)
{
    return getTopic(2, topic);
}

void Mqtt::mqttSetLoopCallback(MQTT_CALLBACK_SIGNATURE)
{
#ifdef USE_ASYNC_MQTT_CLIENT
    Mqtt::callback = callback;
    mqttClient.onMessage([&](char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
        Mqtt::callback(topic, (byte *)payload, len);
    });
#else
    mqttClient.setCallback(callback);
#endif
}

void Mqtt::mqttSetConnectedCallback(MQTT_CONNECTED_CALLBACK_SIGNATURE)
{
    Mqtt::connectedcallback = connectedcallback;
#ifdef USE_ASYNC_MQTT_CLIENT
    mqttClient.onConnect([&](bool sessionPresent) {
        bitSet(Config::statusFlag, 1);
        Log::Info(PSTR("successful client mqtt connection"));
        availability();
        if (globalConfig.mqtt.interval > 0)
        {
            doReportInfo();
        }
        Mqtt::connectedcallback();
    });
#endif
}

#ifndef USE_ASYNC_MQTT_CLIENT
PubSubClient &Mqtt::setClient(Client &client)
{
    return mqttClient.setClient(client);
}
#endif

bool Mqtt::publish(const char *topic, const char *payload, bool retained)
{
#ifdef USE_ASYNC_MQTT_CLIENT
    return mqttClient.publish(topic, 0, retained, payload);
#else
    return mqttClient.publish(topic, payload, retained);
#endif
}

bool Mqtt::publish(const char *topic, const uint8_t *payload, unsigned int plength, bool retained)
{
#ifdef USE_ASYNC_MQTT_CLIENT
    return mqttClient.publish(topic, 0, retained, (const char *)payload, plength);
#else
    return mqttClient.publish(topic, payload, plength, retained);
#endif
}

bool Mqtt::subscribe(const char *topic, uint8_t qos)
{
    return mqttClient.subscribe(topic, qos);
}
bool Mqtt::unsubscribe(const char *topic)
{
    return mqttClient.unsubscribe(topic);
}

String Mqtt::getTopic(uint8_t prefix, String subtopic)
{
    // 0: Cmnd  1:Stat 2:Tele
    String fulltopic = String(globalConfig.mqtt.topic);
    if ((0 == prefix) && (-1 == fulltopic.indexOf(F("%prefix%"))))
    {
        fulltopic += F("/%prefix%"); // Need prefix for commands to handle mqtt topic loops
    }
    fulltopic.replace(F("%prefix%"), (prefix == 0 ? F("cmnd") : ((prefix == 1 ? F("stat") : F("tele")))));
    fulltopic.replace(F("%hostname%"), UID);
    fulltopic.replace(F("%module%"), module ? module->getModuleName() : F("module"));
    fulltopic.replace(F("#"), F(""));
    fulltopic.replace(F("//"), F("/"));
    if (!fulltopic.endsWith(F("/")))
        fulltopic += F("/");
    return fulltopic + subtopic;
}
#endif