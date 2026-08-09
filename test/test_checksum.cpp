#include <unity.h>
#include <stdint.h>
#include "checksum.h"

static void test_checksum_empty_payload(void) {
    TEST_ASSERT_EQUAL_UINT8(0xFE, zbGenChksum(0xFE, NULL, 0));
}

static void test_checksum_single_byte(void) {
    const uint8_t data[] = {0x02};
    TEST_ASSERT_EQUAL_UINT8(0x00, zbGenChksum(0xFE, data, 1)); // 0xFE + 0x02 = 0x100 → 0x00
}

static void test_checksum_no_overflow_wrap(void) {
    const uint8_t data[] = {0x01, 0x02, 0x03};
    TEST_ASSERT_EQUAL_UINT8(0xFE + 0x01 + 0x02 + 0x03, zbGenChksum(0xFE, data, 3));
}

static void test_checksum_wraps_at_256(void) {
    const uint8_t data[] = {0xFF, 0x01};
    TEST_ASSERT_EQUAL_UINT8(0x00, zbGenChksum(0x00, data, 2)); // 0xFF + 0x01 = 0x100 → 0x00
}

static void test_checksum_znp_cmd_payload(void) {
    // cmd=0x41, payload 0x02 0x01 0x02 -> 0x41+0x02+0x01+0x02 = 0x46
    const uint8_t data[] = {0x02, 0x01, 0x02};
    TEST_ASSERT_EQUAL_UINT8(0x46, zbGenChksum(0x41, data, 3));
}

static void test_checksum_zero_cmd_zero_len(void) {
    TEST_ASSERT_EQUAL_UINT8(0x00, zbGenChksum(0x00, NULL, 0));
}

static void test_checksum_respects_length(void) {
    const uint8_t data[] = {0x10, 0x20};
    TEST_ASSERT_EQUAL_UINT8(0x30, zbGenChksum(0x00, data, 2));
    TEST_ASSERT_EQUAL_UINT8(0x10, zbGenChksum(0x00, data, 1));
}

void run_checksum_tests(void) {
    RUN_TEST(test_checksum_empty_payload);
    RUN_TEST(test_checksum_single_byte);
    RUN_TEST(test_checksum_no_overflow_wrap);
    RUN_TEST(test_checksum_wraps_at_256);
    RUN_TEST(test_checksum_znp_cmd_payload);
    RUN_TEST(test_checksum_zero_cmd_zero_len);
    RUN_TEST(test_checksum_respects_length);
}
