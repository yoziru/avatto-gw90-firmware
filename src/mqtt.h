#ifndef MQTT_H_
#define MQTT_H_

#include <string>

void mqttConnectSetup();
void mqttReconnect();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void mqttLoop();
void mqttPublishState();
void mqttOnConnect();
void mqttPublishAvail();
void mqttPublishDiscovery();
void mqttPublishMsg(const std::string &topic, const std::string &msg, bool retain);
void mqttPublishIo(String const &io, String const &state);
void mqttSubscribe(String topic);

#endif
