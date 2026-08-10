#include <unity.h>
#include <ArduinoJson.h>
#include <string.h>
#include "ha_discovery.h"

static void test_entity_types_are_valid(void) {
    for (const auto &e : haDiscoveryEntities) {
        TEST_ASSERT_TRUE_MESSAGE(strcmp(e.type, "button") == 0 ||
                                     strcmp(e.type, "binary_sensor") == 0 ||
                                     strcmp(e.type, "sensor") == 0,
                                 e.type);
    }
}

// Every button payload_press must be parseable JSON carrying a non-empty "cmd".
// This locks the fix that changed `{cmd:"rst_esp"}` (unparseable) into
// `{"cmd":"rst_esp"}` - otherwise the Restart/BSL MQTT buttons would never fire.
static void test_button_payloads_are_valid_json(void) {
    for (const auto &e : haDiscoveryEntities) {
        if (e.command == nullptr) {
            continue;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, e.command);
        TEST_ASSERT_FALSE_MESSAGE(err, e.command);
        TEST_ASSERT_TRUE(doc["cmd"].is<const char *>());
        const char *cmd = doc["cmd"].as<const char *>();
        TEST_ASSERT_NOT_NULL(cmd);
        TEST_ASSERT_TRUE(strlen(cmd) > 0);
    }
}

static void test_ids_are_unique(void) {
    for (size_t i = 0; i < haDiscoveryEntityCount(); i++) {
        for (size_t j = i + 1; j < haDiscoveryEntityCount(); j++) {
            TEST_ASSERT_TRUE(strcmp(haDiscoveryEntities[i].id, haDiscoveryEntities[j].id) != 0);
        }
    }
}

static void test_io_state_and_template_are_exclusive(void) {
    for (const auto &e : haDiscoveryEntities) {
        // The state source is either .../io/<id> (buttons + socket) or a value
        // template against .../state (sensors) - never both, never neither.
        TEST_ASSERT_TRUE(e.ioState != (e.valueTemplate != nullptr));
    }
}

static void test_full_device_descriptor_used_once(void) {
    size_t fullDeviceCount = 0;
    for (const auto &e : haDiscoveryEntities) {
        if (e.fullDevice) {
            fullDeviceCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT(1, fullDeviceCount);
}

static void test_discovery_topic_format(void) {
    for (const auto &e : haDiscoveryEntities) {
        std::string topic = haDiscoveryTopic(e, "AVATTO-GW90-TI-1234");
        std::string expected = std::string("homeassistant/") + e.type + "/AVATTO-GW90-TI-1234/" + e.id + "/config";
        TEST_ASSERT_EQUAL_STRING(expected.c_str(), topic.c_str());
    }
}

static void test_button_message_shape(void) {
    JsonDocument fullDev;
    fullDev["ids"] = "devid";
    fullDev["name"] = "GW90";
    fullDev["mf"] = "Zig Star";
    JsonDocument idDev;
    idDev["ids"] = "devid";

    const HaDiscoveryEntity *btn = nullptr;
    for (const auto &e : haDiscoveryEntities) {
        if (strcmp(e.id, "rst_esp") == 0) {
            btn = &e;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(btn);

    JsonDocument msg = buildHaDiscoveryMessage(*btn, "AVATTO-GW90-TI-1234", "avatto/zigbee",
                                               fullDev.as<JsonObjectConst>(), idDev.as<JsonObjectConst>());

    // The full device descriptor (name/mf/sw) is attached to this one entity.
    TEST_ASSERT_EQUAL_STRING("GW90", msg["dev"]["name"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("Zig Star", msg["dev"]["mf"].as<const char *>());
    // Buttons report state on .../io/<id>, take commands on .../cmd, and carry
    // a press payload the gateway can parse.
    TEST_ASSERT_EQUAL_STRING("avatto/zigbee/io/rst_esp", msg["stat_t"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("avatto/zigbee/cmd", msg["cmd_t"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("{\"cmd\":\"rst_esp\"}", msg["payload_press"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("avatto/zigbee/avty", msg["avty_t"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("AVATTO-GW90-TI-1234/rst_esp", msg["uniq_id"].as<const char *>());
}

static void test_sensor_message_shape(void) {
    JsonDocument fullDev;
    fullDev["ids"] = "devid";
    JsonDocument idDev;
    idDev["ids"] = "devid";

    const HaDiscoveryEntity *sensor = nullptr;
    for (const auto &e : haDiscoveryEntities) {
        if (strcmp(e.id, "temperature") == 0) {
            sensor = &e;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(sensor);

    JsonDocument msg = buildHaDiscoveryMessage(*sensor, "AVATTO-GW90-TI-1234", "avatto/zigbee",
                                               fullDev.as<JsonObjectConst>(), idDev.as<JsonObjectConst>());

    // Sensors report the JSON state doc on .../state via a value template.
    TEST_ASSERT_EQUAL_STRING("avatto/zigbee/state", msg["stat_t"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("avatto/zigbee/state", msg["json_attr_t"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("{{ value_json.temperature }}", msg["val_tpl"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("temperature", msg["dev_cla"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("measurement", msg["stat_cla"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("°C", msg["unit_of_meas"].as<const char *>());
    // Every non-first entity carries only the id-only device reference.
    TEST_ASSERT_FALSE(msg["dev"].containsKey("name"));
}

// The state topic must follow the entity's stat source: .../io/<id> for
// ioState entities (buttons + socket), .../state for everything else.
static void test_stat_topic_matches_entity_source(void) {
    JsonDocument dev;
    dev["ids"] = "devid";
    for (const auto &e : haDiscoveryEntities) {
        JsonDocument msg = buildHaDiscoveryMessage(e, "GW", "avatto/zigbee",
                                                   dev.as<JsonObjectConst>(), dev.as<JsonObjectConst>());
        std::string expected = e.ioState ? std::string("avatto/zigbee/io/") + e.id
                                         : std::string("avatto/zigbee/state");
        TEST_ASSERT_EQUAL_STRING(expected.c_str(), msg["stat_t"].as<const char *>());
    }
}

void run_ha_discovery_tests(void) {
    RUN_TEST(test_entity_types_are_valid);
    RUN_TEST(test_button_payloads_are_valid_json);
    RUN_TEST(test_ids_are_unique);
    RUN_TEST(test_io_state_and_template_are_exclusive);
    RUN_TEST(test_full_device_descriptor_used_once);
    RUN_TEST(test_discovery_topic_format);
    RUN_TEST(test_button_message_shape);
    RUN_TEST(test_sensor_message_shape);
    RUN_TEST(test_stat_topic_matches_entity_source);
}
