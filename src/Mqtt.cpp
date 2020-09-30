#ifndef DISABLE_MQTT

#include "Mqtt.h"
#include "Module.h"

uint8_t Mqtt::operationFlag = 0;
PubSubClient Mqtt::mqttClient;
uint32_t Mqtt::lastReconnectAttempt = 0;   // 最后尝试重连时间
uint32_t Mqtt::kMqttReconnectTime = 30000; // 重新连接尝试之间的延迟（ms）
std::function<void()> Mqtt::connectedcallback = NULL;
#ifdef USE_ASYNC_MQTT_CLIENT
std::function<void(char *, uint8_t *, unsigned int)> Mqtt::callback = NULL;
char mqttWill[80] = {0};
#endif
bool Mqtt::mqttConnect()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Debug::AddInfo(PSTR("wifi disconnected"));
        return false;
    }
    if (globalConfig.mqtt.port == 0)
    {
        Debug::AddInfo(PSTR("no set mqtt info"));
        return false;
    }
    if (mqttClient.connected())
    {
        return true;
    }

    Debug::AddInfo(PSTR("client mqtt not connected, trying to connect to %s:%d Broker"), globalConfig.mqtt.server, globalConfig.mqtt.port);
#ifdef USE_ASYNC_MQTT_CLIENT
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
        Debug::AddInfo(PSTR("successful client mqtt connection"));
        availability();
        if (globalConfig.mqtt.interval > 0)
        {
            doReportHeartbeat();
        }
        if (connectedcallback != NULL)
        {
            connectedcallback();
        }
    }
    else
    {
        Debug::AddInfo(PSTR("Connecting to %s:%d Broker . . failed, rc=%d"), globalConfig.mqtt.server, globalConfig.mqtt.port, mqttClient.state());
    }
#endif
    return mqttClient.connected();
}

void Mqtt::doReportHeartbeat()
{
    char message[250];
    sprintf(message, PSTR("{\"UID\":\"%s\",\"SSID\":\"%s\",\"RSSI\":\"%s\",\"Version\":\"%s\",\"ip\":\"%s\",\"mac\":\"%s\",\"freeMem\":%d,\"uptime\":%d}"),
            UID, WiFi.SSID().c_str(), String(WiFi.RSSI()).c_str(), (module ? module->getModuleVersion().c_str() : PSTR("0")), WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(), ESP.getFreeHeap(), millis() / 1000);
    //Debug::AddInfo(PSTR("%s"), message);
    publish(getTeleTopic(F("HEARTBEAT")), message);
}

void Mqtt::availability()
{
    publish(getTeleTopic(F("availability")), "online", true);
}

void Mqtt::perSecondDo()
{
    bitSet(operationFlag, 0);
}

void Mqtt::loop()
{
    if (WiFi.status() != WL_CONNECTED || globalConfig.mqtt.port == 0)
    {
        return;
    }
    uint32_t now = millis();
    if (!mqttClient.connected())
    {
        if (now - lastReconnectAttempt > kMqttReconnectTime || lastReconnectAttempt == 0)
        {
            lastReconnectAttempt = now;
            if (mqttConnect())
            {
                lastReconnectAttempt = 0;
            }
        }
    }
    else
    {
#ifndef USE_ASYNC_MQTT_CLIENT
        mqttClient.loop();
#endif
        if (bitRead(operationFlag, 0))
        {
            bitClear(operationFlag, 0);
            if (globalConfig.mqtt.interval > 0 && (perSecond % globalConfig.mqtt.interval) == 0)
            {
                doReportHeartbeat();
            }
            if (perSecond % 3609 == 0)
            {
                availability();
            }
        }
    }
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
        Debug::AddInfo(PSTR("successful client mqtt connection"));
        availability();
        if (globalConfig.mqtt.interval > 0)
        {
            doReportHeartbeat();
        }
        Mqtt::connectedcallback();
    });
#endif
}

PubSubClient &Mqtt::setClient(Client &client)
{
#ifdef USE_ASYNC_MQTT_CLIENT
    return mqttClient;
#else
    return mqttClient.setClient(client);
#endif
}

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