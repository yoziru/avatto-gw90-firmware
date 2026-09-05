#ifndef HA_DISCOVERY_H_
#define HA_DISCOVERY_H_

#include <stddef.h>

#include <string>

#include <ArduinoJson.h>

// Home Assistant MQTT discovery entities published by mqttPublishDiscovery().
// Kept as plain data (no Arduino types) so the native test suite can validate
// the table - e.g. that every button payload_press is parseable JSON.
struct HaDiscoveryEntity
{
    const char *type;          // "button", "binary_sensor" or "sensor"
    const char *id;            // unique id, e.g. "rst_esp"
    const char *name;
    const char *icon;
    bool ioState;              // state on .../io/<id> (buttons + socket), else .../state
    const char *command;       // button: JSON payload_press
    const char *valueTemplate; // sensor: val_tpl
    const char *deviceClass;   // dev_cla
    const char *statClass;     // stat_cla
    const char *unit;          // unit_of_meas
    bool fullDevice;           // use the full device object (once), else the id-only one
};

inline const HaDiscoveryEntity haDiscoveryEntities[] = {
    {"button", "rst_esp", "Restart ESP", "mdi:restore-alert", true, "{\"cmd\":\"rst_esp\"}", nullptr, nullptr, nullptr, nullptr, true},
    {"button", "rst_zig", "Restart Zigbee", "mdi:restart", true, "{\"cmd\":\"rst_zig\"}", nullptr, nullptr, nullptr, nullptr, false},
    {"button", "enbl_bsl", "Enable BSL", "mdi:flash", true, "{\"cmd\":\"enbl_bsl\"}", nullptr, nullptr, nullptr, nullptr, false},
    {"binary_sensor", "socket", "Socket", nullptr, true, nullptr, nullptr, "connectivity", nullptr, nullptr, false},
    {"sensor", "uptime", "Uptime", "mdi:clock", false, nullptr, "{{ value_json.uptime }}", nullptr, nullptr, nullptr, false},
    {"sensor", "ip", "IP", "mdi:check-network", false, nullptr, "{{ value_json.ip }}", nullptr, nullptr, nullptr, false},
    {"sensor", "temperature", "ESP temperature", "mdi:coolant-temperature", false, nullptr, "{{ value_json.temperature }}", "temperature", "measurement", "°C", false},
    {"sensor", "hostname", "Hostname", "mdi:account-network", false, nullptr, "{{ value_json.hostname }}", nullptr, nullptr, nullptr, false},
    {"sensor", "connections", "Socket connections", "mdi:check-network-outline", false, nullptr, "{{ value_json.connections }}", nullptr, nullptr, nullptr, false},
    {"sensor", "mode", "Mode", "mdi:access-point-network", false, nullptr, "{{ value_json.mode }}", nullptr, nullptr, nullptr, false},
    {"sensor", "zbfw", "Zigbee fw rev", "mdi:access-point-network", false, nullptr, "{{ value_json.zbfw }}", nullptr, nullptr, nullptr, false},
};

// Home Assistant discovery topic for an entity, e.g.
// "homeassistant/sensor/AVATTO-GW90-TI-XXXX/uptime/config".
inline std::string haDiscoveryTopic(const HaDiscoveryEntity &e, const char *deviceName) noexcept
{
    return std::string("homeassistant/") + e.type + "/" + deviceName + "/" + e.id + "/config";
}

// Assemble the MQTT discovery config message for one entity (what Home
// Assistant actually consumes). `deviceName` is the MQTT topic base used for
// unique ids; `mqttTopic` is the base topic the gateway publishes state to.
// `fullDev` carries the device name/manufacturer/sw (attached once); `idOnlyDev`
// only carries the device ids (attached to every other entity).
inline JsonDocument buildHaDiscoveryMessage(const HaDiscoveryEntity &e,
                                            const char *deviceName,
                                            const char *mqttTopic,
                                            const JsonObjectConst &fullDev,
                                            const JsonObjectConst &idOnlyDev)
{
    JsonDocument msg;
    const std::string base(deviceName);
    const std::string topic(mqttTopic);
    msg["name"] = e.name;
    msg["uniq_id"] = base + "/" + e.id;
    msg["stat_t"] = e.ioState ? (topic + "/io/" + e.id) : (topic + "/state");
    msg["avty_t"] = topic + "/avty";
    if (e.icon != nullptr)
    {
        msg["icon"] = e.icon;
    }
    if (e.command != nullptr)
    {
        msg["cmd_t"] = topic + "/cmd";
        msg["payload_press"] = e.command;
    }
    if (e.valueTemplate != nullptr)
    {
        msg["val_tpl"] = e.valueTemplate;
        msg["json_attr_t"] = topic + "/state";
    }
    if (e.deviceClass != nullptr)
    {
        msg["dev_cla"] = e.deviceClass;
    }
    if (e.statClass != nullptr)
    {
        msg["stat_cla"] = e.statClass;
    }
    if (e.unit != nullptr)
    {
        msg["unit_of_meas"] = e.unit;
    }
    msg["dev"] = e.fullDevice ? fullDev : idOnlyDev;
    return msg;
}

#endif
