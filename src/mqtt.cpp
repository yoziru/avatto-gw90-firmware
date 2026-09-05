#include <Arduino.h>
#include <ArduinoJson.h>
#include <ETH.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <PubSubClient.h>

#include "config.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "mqtt.h"
#include "mqtt_cmd.h"
#include "ha_discovery.h"

extern zbVerStruct zbVer;

WiFiClient clientMqtt;

PubSubClient clientPubSub(clientMqtt);

void mqttConnectSetup()
{
    clientPubSub.setServer(MqttSettings.serverIP, MqttSettings.port);
    clientPubSub.setCallback(mqttCallback);
}

void mqttReconnect()
{
    DEBUG_PRINT(F("Attempting MQTT connection..."));

    byte willQoS = 0;
    String willTopic = String(MqttSettings.topic) + "/avty";
    const char *willMessage = "offline";
    boolean willRetain = false;
    char deviceIdArr[20];
    getDeviceID(deviceIdArr);

    if (clientPubSub.connect(String(deviceIdArr).c_str(), MqttSettings.user, MqttSettings.pass, willTopic.c_str(), willQoS, willRetain, willMessage))
    {
        MqttSettings.reconnectTime = 0;
        mqttOnConnect();
    }
    else
    {
        DEBUG_PRINT(F("failed, rc="));
        DEBUG_PRINT(clientPubSub.state());
        DEBUG_PRINT(F(" try again in "));
        DEBUG_PRINT(MqttSettings.interval);
        DEBUG_PRINTLN(F(" seconds"));

        MqttSettings.reconnectTime = millis() + MqttSettings.interval * 1000;
    }
}

void mqttOnConnect()
{
    DEBUG_PRINTLN(F("connected"));
    mqttSubscribe("cmd");
    DEBUG_PRINTLN(F("mqtt Subscribed"));
    if (MqttSettings.discovery)
    {
        mqttPublishDiscovery();
        DEBUG_PRINTLN(F("mqtt Published Discovery"));
    }

    mqttPublishIo("rst_esp", "OFF");
    mqttPublishIo("rst_zig", "OFF");
    mqttPublishIo("enbl_bsl", "OFF");
    mqttPublishIo("socket", "OFF");
    DEBUG_PRINTLN(F("mqtt Published IOs"));
    mqttPublishAvail();
    DEBUG_PRINTLN(F("mqtt Published Avty"));
    if (MqttSettings.interval > 0)
    {
        mqttPublishState();
        DEBUG_PRINTLN(F("mqtt Published State"));
    }
}

void mqttPublishMsg(const std::string &topic, const std::string &msg, bool retain)
{
    clientPubSub.beginPublish(topic.c_str(), msg.length(), retain);
    clientPubSub.print(msg.c_str());
    clientPubSub.endPublish();
}

void mqttPublishAvail()
{
    String topic(MqttSettings.topic);
    topic = topic + "/avty";
    String mqttBuffer = "online";
    clientPubSub.publish(topic.c_str(), mqttBuffer.c_str(), true);
}

void mqttPublishState()
{
    String topic(MqttSettings.topic);
    topic = topic + "/state";
    JsonDocument root;
    String readableTime;
    getReadableTime(readableTime, 0);
    root["uptime"] = readableTime;

    float CPUtemp = getCPUtemp();
    root["temperature"] = CPUtemp;
    
    root["connections"] = ConfigSettings.connectedClients;
    
    if (ConfigSettings.connectedEther)
    {
        if (ConfigSettings.dhcp)
        {
            root["ip"] = ETH.localIP().toString();
        }
        else
        {
            root["ip"] = ConfigSettings.ipAddress;
        }
    }
    else
    {
        if (ConfigSettings.dhcpWiFi)
        {
            root["ip"] = WiFi.localIP().toString();
        }
        else
        {
            root["ip"] = ConfigSettings.ipAddressWiFi;
        }
    }

    switch (ConfigSettings.coordinator_mode) {
        case CoordinatorMode::PASSTHROUGH:
            root["mode"] = "Zigbee-to-Serial";
            break;
        case CoordinatorMode::NETWORK:
            root["mode"] = "Zigbee-to-Network";
            break;
        default:
            break;
    }
    root["zbfw"] = zbVer.zbRev;
    root["hostname"] = ConfigSettings.hostname;
    String mqttBuffer;
    serializeJson(root, mqttBuffer);
    DEBUG_PRINTLN(mqttBuffer);
    clientPubSub.publish(topic.c_str(), mqttBuffer.c_str(), true);
    MqttSettings.heartbeatTime = millis() + (MqttSettings.interval * 1000);
}

void mqttPublishIo(String const &io, String const &state)
{
    if (clientPubSub.connected())
    {
        String topic(MqttSettings.topic);
        topic = topic + "/io/" + io;
        clientPubSub.publish(topic.c_str(), state.c_str(), true);
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    (void)topic;
    const MqttCmd cmd = parseMqttCommand(payload, length);
    if (cmd == MqttCmd::None)
    {
        return;
    }

    switch (cmd)
    {
    case MqttCmd::RstEsp:
        printLogMsg("ESP restart MQTT");
        ESP.restart();
        break;
    case MqttCmd::RstZig:
        printLogMsg("Zigbee restart MQTT");
        zigbeeRestart();
        break;
    case MqttCmd::EnblBsl:
        printLogMsg("Zigbee BSL enable MQTT");
        zigbeeEnableBSL();
        break;
    case MqttCmd::None:
        break;
    }
    return;
}

void mqttSubscribe(String topic)
{
    String mtopic(MqttSettings.topic);
    mtopic = mtopic + "/" + topic;
    clientPubSub.subscribe(mtopic.c_str());
}

void mqttLoop()
{
    if (!clientPubSub.connected())
    {
        if (MqttSettings.reconnectTime == 0)
        {
            DEBUG_PRINTLN(F("mqttReconnect in 5 seconds"));
            MqttSettings.reconnectTime = millis() + 5000;
        }
        else
        {
            if (MqttSettings.reconnectTime <= millis())
            {
                mqttReconnect();
            }
        }
    }
    else
    {
        clientPubSub.loop();
        if (MqttSettings.interval > 0)
        {
            if (MqttSettings.heartbeatTime <= millis())
            {
                mqttPublishState();
            }
        }
    }
}

void mqttPublishDiscovery()
{
    String deviceName(MqttSettings.topic);

    char deviceIdArr[20];
    getDeviceID(deviceIdArr);

    // Minimal device reference (ids only) used by every entity.
    JsonDocument via;
    via["ids"] = String(deviceIdArr);

    // Full device descriptor (ids + name + manufacturer + sw) attached once.
    JsonDocument dev;
    dev["ids"] = String(deviceIdArr);
    dev["name"] = ConfigSettings.hostname;
    dev["mf"] = "Zig Star";
    char verArr[25];
    const char *env = STRINGIFY(BUILD_ENV_NAME);
    sprintf(verArr, "%s (%s)", VERSION, env);
    dev["sw"] = String(verArr);

    const String mqttTopic = String(MqttSettings.topic);

    for (const auto &e : haDiscoveryEntities)
    {
        JsonDocument buffJson = buildHaDiscoveryMessage(e, deviceName.c_str(), mqttTopic.c_str(),
                                                        dev.as<JsonObjectConst>(), via.as<JsonObjectConst>());
        std::string topic = haDiscoveryTopic(e, deviceName.c_str());
        std::string mqttBuffer;
        serializeJson(buffJson, mqttBuffer);
        mqttPublishMsg(topic, mqttBuffer, true);
    }
}
