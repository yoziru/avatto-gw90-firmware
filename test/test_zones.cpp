#include <unity.h>
#include <string.h>
#include "zones.h"

static void test_timezone_known_zone(void) {
    TEST_ASSERT_EQUAL_STRING("GMT0", getGmtOffsetForZone("Africa/Abidjan"));
}

static void test_timezone_gmt_offset_format(void) {
    TEST_ASSERT_EQUAL_STRING("CET-1", getGmtOffsetForZone("Africa/Algiers"));
}

static void test_timezone_dst_posix_rule(void) {
    // POSIX TZ string with DST transition rule
    TEST_ASSERT_EQUAL_STRING("EET-2EEST,M4.5.5/0,M10.5.4/24", getGmtOffsetForZone("Africa/Cairo"));
}

static void test_timezone_unknown_returns_null(void) {
    TEST_ASSERT_NULL(getGmtOffsetForZone("Mars/Olympus"));
}

static void test_timezone_empty_returns_null(void) {
    TEST_ASSERT_NULL(getGmtOffsetForZone(""));
}

static void test_timezone_lookup_is_case_sensitive(void) {
    TEST_ASSERT_NULL(getGmtOffsetForZone("africa/abidjan"));
}

static void test_timezone_tz_database_has_unique_zones(void) {
    for (int i = 0; i < timeZoneCount; i++) {
        for (int j = i + 1; j < timeZoneCount; j++) {
            if (strcmp(timeZones[i].zone, timeZones[j].zone) == 0) {
                TEST_FAIL_MESSAGE("Duplicate timezone entry");
            }
        }
    }
}

static void test_timezone_all_entries_resolvable(void) {
    for (int i = 0; i < timeZoneCount; i++) {
        TEST_ASSERT_EQUAL_STRING(timeZones[i].gmtOffset, getGmtOffsetForZone(timeZones[i].zone));
    }
}

void run_zones_tests(void) {
    RUN_TEST(test_timezone_known_zone);
    RUN_TEST(test_timezone_gmt_offset_format);
    RUN_TEST(test_timezone_dst_posix_rule);
    RUN_TEST(test_timezone_unknown_returns_null);
    RUN_TEST(test_timezone_empty_returns_null);
    RUN_TEST(test_timezone_lookup_is_case_sensitive);
    RUN_TEST(test_timezone_tz_database_has_unique_zones);
    RUN_TEST(test_timezone_all_entries_resolvable);
}
