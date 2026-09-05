#include <unity.h>
#include <string.h>
#include "mqtt_cmd.h"

static MqttCmd parse(const char *s) {
    return parseMqttCommand(reinterpret_cast<const uint8_t *>(s), strlen(s));
}

static void test_rst_esp(void) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MqttCmd::RstEsp, (uint8_t)parse("{\"cmd\":\"rst_esp\"}"));
}

static void test_rst_zig(void) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MqttCmd::RstZig, (uint8_t)parse("{\"cmd\":\"rst_zig\"}"));
}

static void test_enbl_bsl(void) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MqttCmd::EnblBsl, (uint8_t)parse("{\"cmd\":\"enbl_bsl\"}"));
}

static void test_unknown_cmd_is_none(void) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MqttCmd::None, (uint8_t)parse("{\"cmd\":\"self_destruct\"}"));
}

static void test_missing_cmd_is_none(void) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MqttCmd::None, (uint8_t)parse("{\"foo\":1}"));
}

static void test_garbage_is_none(void) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MqttCmd::None, (uint8_t)parse("not json at all"));
}

static void test_empty_is_none(void) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MqttCmd::None, (uint8_t)parse(""));
}

static void test_null_is_none(void) {
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MqttCmd::None, (uint8_t)parseMqttCommand(nullptr, 0));
}

static void test_oversized_is_capped_not_crash(void) {
    // 300 bytes: no crash, and the cmd past the 256 cap is not seen.
    char big[300];
    memset(big, ' ', sizeof(big));
    static const char cmd[] = "{\"cmd\":\"rst_esp\"}";
    memcpy(big, cmd, sizeof(cmd) - 1);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MqttCmd::RstEsp,
                            (uint8_t)parseMqttCommand(reinterpret_cast<const uint8_t *>(big), sizeof(big)));
    memset(big, 'x', sizeof(big));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)MqttCmd::None,
                            (uint8_t)parseMqttCommand(reinterpret_cast<const uint8_t *>(big), sizeof(big)));
}

void run_mqtt_cmd_tests(void) {
    RUN_TEST(test_rst_esp);
    RUN_TEST(test_rst_zig);
    RUN_TEST(test_enbl_bsl);
    RUN_TEST(test_unknown_cmd_is_none);
    RUN_TEST(test_missing_cmd_is_none);
    RUN_TEST(test_garbage_is_none);
    RUN_TEST(test_empty_is_none);
    RUN_TEST(test_null_is_none);
    RUN_TEST(test_oversized_is_capped_not_crash);
}
