#include <unity.h>
#include <ArduinoJson.h>
#include <string.h>
#include <string>
#include "zb_info.h"
#include "zb_ver.h"

static ZbInfo sampleInfo() {
    ZbInfo info;
    info.zbRev = 20250321;
    info.deviceState = ZB_DEVICE_STATE_COORDINATOR;
    info.maintrel = 1;
    info.minorrel = 2;
    info.majorrel = 3;
    info.product = 4;
    info.transportrev = 5;
    snprintf(info.chipID, sizeof(info.chipID), "%s", "CC2652P7");
    return info;
}

static void test_zb_info_json_roundtrip(void) {
    const ZbInfo info = sampleInfo();
    JsonDocument doc;
    zbInfoToJson(info, doc);

    std::string json;
    serializeJson(doc, json);

    JsonDocument parsed;
    deserializeJson(parsed, json);
    const ZbInfo decoded = zbInfoFromJson(parsed);

    TEST_ASSERT_TRUE(zbInfoEquals(info, decoded));
    TEST_ASSERT_EQUAL_UINT32(20250321, decoded.zbRev);
    TEST_ASSERT_EQUAL_UINT8(ZB_DEVICE_STATE_COORDINATOR, decoded.deviceState);
    TEST_ASSERT_EQUAL_STRING("CC2652P7", decoded.chipID);
}

static void test_zb_info_from_empty_json_is_unknown(void) {
    JsonDocument doc;
    deserializeJson(doc, "{}");
    const ZbInfo info = zbInfoFromJson(doc);

    TEST_ASSERT_EQUAL_UINT32(0, info.zbRev);
    TEST_ASSERT_EQUAL_UINT8(0xFF, info.deviceState);
    TEST_ASSERT_EQUAL_STRING("", info.chipID);
    TEST_ASSERT_FALSE(zbInfoHasData(info));
}

static void test_zb_info_equals_detects_every_change(void) {
    const ZbInfo base = sampleInfo();

    ZbInfo changed = base;
    TEST_ASSERT_TRUE(zbInfoEquals(base, changed));

    changed.zbRev++;
    TEST_ASSERT_FALSE(zbInfoEquals(base, changed));
    changed = base;

    changed.deviceState = ZB_DEVICE_STATE_ROUTER;
    TEST_ASSERT_FALSE(zbInfoEquals(base, changed));
    changed = base;

    changed.maintrel++;
    TEST_ASSERT_FALSE(zbInfoEquals(base, changed));
    changed = base;

    changed.minorrel++;
    TEST_ASSERT_FALSE(zbInfoEquals(base, changed));
    changed = base;

    changed.majorrel++;
    TEST_ASSERT_FALSE(zbInfoEquals(base, changed));
    changed = base;

    changed.product++;
    TEST_ASSERT_FALSE(zbInfoEquals(base, changed));
    changed = base;

    changed.transportrev++;
    TEST_ASSERT_FALSE(zbInfoEquals(base, changed));
    changed = base;

    snprintf(changed.chipID, sizeof(changed.chipID), "%s", "CC2652P");
    TEST_ASSERT_FALSE(zbInfoEquals(base, changed));
}

static void test_zb_info_has_data(void) {
    ZbInfo info;
    TEST_ASSERT_FALSE(zbInfoHasData(info));

    info.zbRev = 1;
    TEST_ASSERT_TRUE(zbInfoHasData(info));
    info.zbRev = 0;

    info.deviceState = ZB_DEVICE_STATE_ROUTER;
    TEST_ASSERT_TRUE(zbInfoHasData(info));
    info.deviceState = 0xFF;

    snprintf(info.chipID, sizeof(info.chipID), "%s", "CC2652P");
    TEST_ASSERT_TRUE(zbInfoHasData(info));
}

static void test_zb_info_chip_id_truncates(void) {
    JsonDocument doc;
    doc["chipID"] = "CC2652P7-EXTREMELY-LONG";
    const ZbInfo info = zbInfoFromJson(doc);

    TEST_ASSERT_EQUAL_UINT(sizeof(info.chipID) - 1, strlen(info.chipID));
    TEST_ASSERT_EQUAL_STRING("CC2652P7-EXTREM", info.chipID);
}

static void test_zb_info_from_malformed_json_is_unknown(void) {
    JsonDocument doc;
    deserializeJson(doc, "{\"zbRev\":\"abc\",\"chipID\":42}");
    const ZbInfo info = zbInfoFromJson(doc);

    TEST_ASSERT_EQUAL_UINT32(0, info.zbRev);
    TEST_ASSERT_EQUAL_UINT8(0xFF, info.deviceState);
    TEST_ASSERT_EQUAL_STRING("", info.chipID);
    TEST_ASSERT_FALSE(zbInfoHasData(info));
}

static void test_zb_detect_after_attempt(void) {
    using S = ZbDetectState;
    TEST_ASSERT_EQUAL(static_cast<int>(S::KNOWN), static_cast<int>(zbDetectAfterAttempt(S::DETECTING, true)));
    TEST_ASSERT_EQUAL(static_cast<int>(S::UNKNOWN), static_cast<int>(zbDetectAfterAttempt(S::DETECTING, false)));
    TEST_ASSERT_EQUAL(static_cast<int>(S::KNOWN), static_cast<int>(zbDetectAfterAttempt(S::KNOWN, false)));
    TEST_ASSERT_EQUAL(static_cast<int>(S::KNOWN), static_cast<int>(zbDetectAfterAttempt(S::UNKNOWN, true)));
    TEST_ASSERT_EQUAL(static_cast<int>(S::UNKNOWN), static_cast<int>(zbDetectAfterAttempt(S::UNKNOWN, false)));
}

static void test_zb_detect_sentinel(void) {
    TEST_ASSERT_EQUAL_STRING("checking", zbDetectSentinel(ZbDetectState::DETECTING));
    TEST_ASSERT_EQUAL_STRING("unknown", zbDetectSentinel(ZbDetectState::UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("unknown", zbDetectSentinel(ZbDetectState::KNOWN));
}

void run_zb_info_tests(void) {
    RUN_TEST(test_zb_info_json_roundtrip);
    RUN_TEST(test_zb_info_from_empty_json_is_unknown);
    RUN_TEST(test_zb_info_from_malformed_json_is_unknown);
    RUN_TEST(test_zb_info_equals_detects_every_change);
    RUN_TEST(test_zb_info_has_data);
    RUN_TEST(test_zb_info_chip_id_truncates);
    RUN_TEST(test_zb_detect_after_attempt);
    RUN_TEST(test_zb_detect_sentinel);
}
