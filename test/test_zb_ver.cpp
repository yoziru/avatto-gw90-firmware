#include <unity.h>
#include <stdint.h>
#include "zb_ver.h"

static void test_decodeZbVer_fields(void) {
    const uint8_t buf[11] = {0x01, 0x02, 0x07, 0x05, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ZbVerInfo v = decodeZbVer(buf);
    TEST_ASSERT_EQUAL_UINT8(0x01, v.transportrev);
    TEST_ASSERT_EQUAL_UINT8(0x02, v.product);
    TEST_ASSERT_EQUAL_UINT8(0x07, v.majorrel);
    TEST_ASSERT_EQUAL_UINT8(0x05, v.minorrel);
    TEST_ASSERT_EQUAL_UINT8(0x04, v.maintrel);
}

static void test_decodeZbVer_rev_little_endian(void) {
    // zbRev is 4 bytes little-endian: 0x11223344 -> bytes 44 33 22 11
    const uint8_t buf[11] = {0, 0, 0, 0, 0, 0x44, 0x33, 0x22, 0x11, 0, 0};
    ZbVerInfo v = decodeZbVer(buf);
    TEST_ASSERT_EQUAL_UINT32(0x11223344, v.zbRev);
}

static void test_decodeZbVer_rev_zero(void) {
    const uint8_t buf[11] = {0};
    ZbVerInfo v = decodeZbVer(buf);
    TEST_ASSERT_EQUAL_UINT32(0, v.zbRev);
    TEST_ASSERT_EQUAL_UINT8(0, v.transportrev);
}

static void test_decodeZbVer_rev_ignores_trailing_bytes(void) {
    // bytes 9-10 are not part of zbRev
    const uint8_t buf[11] = {0, 0, 0, 0, 0, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF};
    ZbVerInfo v = decodeZbVer(buf);
    TEST_ASSERT_EQUAL_UINT32(1, v.zbRev);
}

void run_zb_ver_tests(void) {
    RUN_TEST(test_decodeZbVer_fields);
    RUN_TEST(test_decodeZbVer_rev_little_endian);
    RUN_TEST(test_decodeZbVer_rev_zero);
    RUN_TEST(test_decodeZbVer_rev_ignores_trailing_bytes);
}
