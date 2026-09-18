#include <unity.h>
#include <ArduinoJson.h>
#include <string.h>
#include "config_utils.h"

static void test_wg_private_key_new_key_wins(void) {
    JsonDocument doc;
    doc["localPrivKey"] = "newkey";
    doc["localIP"] = "oldkey";
    TEST_ASSERT_EQUAL_STRING("newkey", wgPrivateKey(doc));
}

static void test_wg_private_key_legacy_fallback(void) {
    JsonDocument doc;
    doc["localIP"] = "oldkey";
    TEST_ASSERT_EQUAL_STRING("oldkey", wgPrivateKey(doc));
}

static void test_wg_private_key_missing(void) {
    JsonDocument doc;
    TEST_ASSERT_NULL(wgPrivateKey(doc));
}

static void test_wg_private_key_non_string_ignored(void) {
    JsonDocument doc;
    doc["localPrivKey"] = 123; // not a string, must not be returned
    TEST_ASSERT_NULL(wgPrivateKey(doc));
}

void run_config_utils_tests(void) {
    RUN_TEST(test_wg_private_key_new_key_wins);
    RUN_TEST(test_wg_private_key_legacy_fallback);
    RUN_TEST(test_wg_private_key_missing);
    RUN_TEST(test_wg_private_key_non_string_ignored);
}
