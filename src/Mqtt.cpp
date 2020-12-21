#ifndef DISABLE_MQTT

#include "Mqtt.h"
#include "Module.h"
#include "Rtc.h"

uint8_t Mqtt::operationFlag = 0;
PubSubClient Mqtt::mqttClient;
uint32_t Mqtt::lastReconnectAttempt = 0; // 最后尝试重连时间
uint32_t Mqtt::kMqttReconnectTime = 60;  // 重新连接尝试之间的延迟（秒）
std::function<void()> Mqtt::connectedcallback = NULL;

bool Mqtt::mqttConnect()
{
    if (WiFi.status() != WL_CONNECTED)
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
    mqttClient.setServer(globalConfig.mqtt.server, globalConfig.mqtt.port);
    if (mqttClient.connect(UID, globalConfig.mqtt.user, globalConfig.mqtt.pass, getTeleTopic(F("availability")).c_str(), 0, true, "offline"))
    {
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
    if (WiFi.status() != WL_CONNECTED || globalConfig.mqtt.port == 0)
    {
        return;
    }

    if (!mqttClient.connected())
    {
        if (lastReconnectAttempt == 0 || millis() - lastReconnectAttempt > kMqttReconnectTime * 1000)
        {
            lastReconnectAttempt = millis();
            if (mqttConnect())
            {
                lastReconnectAttempt = 0;
            }
        }
    }
    else
    {
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
    if (WiFi.status() != WL_CONNECTED || globalConfig.mqtt.port == 0)
    {
        return;
    }
    mqttClient.loop();
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
    mqttClient.setCallback(callback);
}

void Mqtt::mqttSetConnectedCallback(MQTT_CONNECTED_CALLBACK_SIGNATURE)
{
    Mqtt::connectedcallback = connectedcallback;
}

PubSubClient &Mqtt::setClient(Client &client)
{
    return mqttClient.setClient(client);
}

bool Mqtt::publish(const char *topic, const char *payload, bool retained)
{
    return mqttClient.publish(topic, payload, retained);
}

bool Mqtt::publish(const char *topic, const uint8_t *payload, unsigned int plength, bool retained)
{
    return mqttClient.publish(topic, payload, plength, retained);
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