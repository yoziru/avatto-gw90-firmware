#ifndef MQTT_CMD_H_
#define MQTT_CMD_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ArduinoJson.h>

// Actions the gateway takes on an MQTT command message (see mqttCallback()).
// Pure logic (no Arduino types) so the native test suite can cover it.
enum class MqttCmd : uint8_t
{
    None,
    RstEsp,
    RstZig,
    EnblBsl
};

// Map a received command payload (raw bytes, network-supplied length) to an
// action. Oversized payloads are capped, never trusted for sizing - matches
// the old MQTT_MAX_PACKET_SIZE (256) bound. Garbage/non-JSON/unknown cmds
// all map to MqttCmd::None.
inline MqttCmd parseMqttCommand(const uint8_t *payload, size_t length)
{
    static const size_t kMaxPayload = 256;
    const size_t capped = (length < kMaxPayload) ? length : kMaxPayload;
    char buf[kMaxPayload + 1];
    if (payload != nullptr && capped > 0)
    {
        memcpy(buf, payload, capped);
    }
    buf[capped] = '\0';

    JsonDocument jsonBuffer;
    if (deserializeJson(jsonBuffer, buf))
    {
        return MqttCmd::None;
    }

    const char *command = jsonBuffer["cmd"];
    if (command == nullptr)
    {
        return MqttCmd::None;
    }
    if (strcmp(command, "rst_esp") == 0)
    {
        return MqttCmd::RstEsp;
    }
    if (strcmp(command, "rst_zig") == 0)
    {
        return MqttCmd::RstZig;
    }
    if (strcmp(command, "enbl_bsl") == 0)
    {
        return MqttCmd::EnblBsl;
    }
    return MqttCmd::None;
}

#endif
