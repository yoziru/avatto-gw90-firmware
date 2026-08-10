#include <unity.h>
#include <ArduinoJson.h>
#include <string.h>
#include "config_utils.h"

static void test_clamp_accepts_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, clampTempOffset(0));
}

static void test_clamp_accepts_bounds(void) {
    TEST_ASSERT_EQUAL_INT(-10, clampTempOffset(-10));
    TEST_ASSERT_EQUAL_INT(10, clampTempOffset(10));
}

static void test_clamp_accepts_inside_range(void) {
    TEST_ASSERT_EQUAL_INT(5, clampTempOffset(5));
    TEST_ASSERT_EQUAL_INT(-3, clampTempOffset(-3));
}

static void test_clamp_rejects_above_range(void) {
    TEST_ASSERT_EQUAL_INT(0, clampTempOffset(11));
    TEST_ASSERT_EQUAL_INT(0, clampTempOffset(23));
}

static void test_clamp_rejects_below_range(void) {
    TEST_ASSERT_EQUAL_INT(0, clampTempOffset(-11));
    TEST_ASSERT_EQUAL_INT(0, clampTempOffset(-40));
}

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
    RUN_TEST(test_clamp_accepts_zero);
    RUN_TEST(test_clamp_accepts_bounds);
    RUN_TEST(test_clamp_accepts_inside_range);
    RUN_TEST(test_clamp_rejects_above_range);
    RUN_TEST(test_clamp_rejects_below_range);
    RUN_TEST(test_wg_private_key_new_key_wins);
    RUN_TEST(test_wg_private_key_legacy_fallback);
    RUN_TEST(test_wg_private_key_missing);
    RUN_TEST(test_wg_private_key_non_string_ignored);
}
