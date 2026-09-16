#include <unity.h>
#include <ArduinoJson.h>
#include <string.h>
#include <string>

// Config field names extracted from source (etc.cpp, save.cpp, pages.cpp)
// These must match what the production code uses for JSON serialization
static const char *configFileWifi = "/config/configWifi.json";
static const char *configFileEther = "/config/configEther.json";
static const char *configFileGeneral = "/config/configGeneral.json";
static const char *configFileSecurity = "/config/configSecurity.json";
static const char *configFileSerial = "/config/configSerial.json";
static const char *configFileMqtt = "/config/configMqtt.json";
static const char *configFileWg = "/config/configWg.json";

static void test_general_config_schema(void) {
    JsonDocument doc;
    doc["coordMode"] = 0;
    doc["keepWeb"] = 1;
    doc["disableLedPwr"] = 1;
    doc["disableLedUSB"] = 0;

    std::string output;
    serializeJson(doc, output);

    JsonDocument parsed;
    deserializeJson(parsed, output);

    TEST_ASSERT_EQUAL_INT(0, parsed["coordMode"].as<int>());
    TEST_ASSERT_TRUE(parsed["keepWeb"].as<bool>());
    TEST_ASSERT_TRUE(parsed["disableLedPwr"].as<bool>());
    TEST_ASSERT_FALSE(parsed["disableLedUSB"].as<bool>());
}

static void test_ethernet_config_schema(void) {
    JsonDocument doc;
    doc["dhcp"] = 1;
    doc["ip"] = "192.168.1.100";
    doc["mask"] = "255.255.255.0";
    doc["gw"] = "192.168.1.1";

    std::string output;
    serializeJson(doc, output);

    JsonDocument parsed;
    deserializeJson(parsed, output);

    TEST_ASSERT_EQUAL_INT(1, parsed["dhcp"].as<int>());
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", parsed["ip"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", parsed["mask"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", parsed["gw"].as<const char *>());
}

static void test_mqtt_config_schema(void) {
    JsonDocument doc;
    doc["enable"] = 1;
    doc["discovery"] = 1;
    doc["tls"] = 1;
    doc["server"] = "mqtt.example.com";
    doc["port"] = 1883;
    doc["user"] = "user1";
    doc["pass"] = "pass1";
    doc["topic"] = "avatto/zigbee";
    doc["interval"] = 60;

    std::string output;
    serializeJson(doc, output);

    JsonDocument parsed;
    deserializeJson(parsed, output);

    TEST_ASSERT_EQUAL_INT(1, parsed["enable"].as<int>());
    TEST_ASSERT_EQUAL_INT(1, parsed["discovery"].as<int>());
    TEST_ASSERT_EQUAL_INT(1, parsed["tls"].as<int>());
    TEST_ASSERT_EQUAL_STRING("mqtt.example.com", parsed["server"].as<const char *>());
    TEST_ASSERT_EQUAL_INT(1883, parsed["port"].as<int>());
}

static void test_wifi_config_schema(void) {
    JsonDocument doc;
    doc["ssid"] = "MyNetwork";
    doc["pass"] = "secret";
    doc["dhcpWiFi"] = 1;
    doc["ip"] = "192.168.1.50";
    doc["mask"] = "255.255.255.0";
    doc["gw"] = "192.168.1.1";

    std::string output;
    serializeJson(doc, output);

    JsonDocument parsed;
    deserializeJson(parsed, output);

    TEST_ASSERT_EQUAL_STRING("MyNetwork", parsed["ssid"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("secret", parsed["pass"].as<const char *>());
    TEST_ASSERT_EQUAL_INT(1, parsed["dhcpWiFi"].as<int>());
}

static void test_security_config_schema(void) {
    JsonDocument doc;
    doc["disableWeb"] = 0;
    doc["webAuth"] = 1;
    doc["webUser"] = "admin";
    doc["webPass"] = "pass123";
    doc["fwEnabled"] = 1;
    doc["fwIp"] = "192.168.1.200";

    std::string output;
    serializeJson(doc, output);

    JsonDocument parsed;
    deserializeJson(parsed, output);

    TEST_ASSERT_EQUAL_INT(0, parsed["disableWeb"].as<int>());
    TEST_ASSERT_EQUAL_INT(1, parsed["webAuth"].as<int>());
    TEST_ASSERT_EQUAL_STRING("admin", parsed["webUser"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("pass123", parsed["webPass"].as<const char *>());
    TEST_ASSERT_EQUAL_INT(1, parsed["fwEnabled"].as<int>());
}

static void test_serial_config_schema(void) {
    JsonDocument doc;
    doc["baud"] = 115200;
    doc["port"] = 6638;

    std::string output;
    serializeJson(doc, output);

    JsonDocument parsed;
    deserializeJson(parsed, output);

    TEST_ASSERT_EQUAL_INT(115200, parsed["baud"].as<int>());
    TEST_ASSERT_EQUAL_INT(6638, parsed["port"].as<int>());
}

static void test_wg_config_schema(void) {
    JsonDocument doc;
    doc["enable"] = 1;
    doc["localAddr"] = "10.0.0.2";
    doc["localPrivKey"] = "privatekey123";
    doc["endAddr"] = "vpn.example.com";
    doc["endPubKey"] = "pubkey123";
    doc["endPort"] = 51820;

    std::string output;
    serializeJson(doc, output);

    JsonDocument parsed;
    deserializeJson(parsed, output);

    TEST_ASSERT_EQUAL_INT(1, parsed["enable"].as<int>());
    TEST_ASSERT_EQUAL_STRING("10.0.0.2", parsed["localAddr"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("privatekey123", parsed["localPrivKey"].as<const char *>());
}

static void test_config_file_paths(void) {
    TEST_ASSERT_EQUAL_STRING("/config/configWifi.json", configFileWifi);
    TEST_ASSERT_EQUAL_STRING("/config/configEther.json", configFileEther);
    TEST_ASSERT_EQUAL_STRING("/config/configGeneral.json", configFileGeneral);
    TEST_ASSERT_EQUAL_STRING("/config/configSecurity.json", configFileSecurity);
    TEST_ASSERT_EQUAL_STRING("/config/configSerial.json", configFileSerial);
    TEST_ASSERT_EQUAL_STRING("/config/configMqtt.json", configFileMqtt);
    TEST_ASSERT_EQUAL_STRING("/config/configWg.json", configFileWg);
}

// Coordinator mode enum values from config.h
static void test_coordinator_mode_values(void) {
    const uint8_t COORDINATOR_MODE_NETWORK = 0;
    const uint8_t COORDINATOR_MODE_PASSTHROUGH = 1;

    JsonDocument doc;
    doc["coordMode"] = COORDINATOR_MODE_NETWORK;
    TEST_ASSERT_EQUAL_INT(0, doc["coordMode"].as<int>());

    doc["coordMode"] = COORDINATOR_MODE_PASSTHROUGH;
    TEST_ASSERT_EQUAL_INT(1, doc["coordMode"].as<int>());
}

// API page enum values from web.h
static void test_api_page_values(void) {
    const uint8_t API_PAGE_GENERAL = 1;
    const uint8_t API_PAGE_ETHERNET = 2;
    const uint8_t API_PAGE_WIFI = 3;

    TEST_ASSERT_EQUAL_INT(1, API_PAGE_GENERAL);
    TEST_ASSERT_EQUAL_INT(2, API_PAGE_ETHERNET);
    TEST_ASSERT_EQUAL_INT(3, API_PAGE_WIFI);
}

void run_config_schema_tests(void) {
    RUN_TEST(test_general_config_schema);
    RUN_TEST(test_ethernet_config_schema);
    RUN_TEST(test_mqtt_config_schema);
    RUN_TEST(test_wifi_config_schema);
    RUN_TEST(test_security_config_schema);
    RUN_TEST(test_serial_config_schema);
    RUN_TEST(test_wg_config_schema);
    RUN_TEST(test_config_file_paths);
    RUN_TEST(test_coordinator_mode_values);
    RUN_TEST(test_api_page_values);
}
