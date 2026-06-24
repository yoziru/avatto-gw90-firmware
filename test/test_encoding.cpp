#include <unity.h>
#include <string.h>
#include <stdint.h>
#include "encoding.h"

static void test_hex2bin_empty(void) {
    uint8_t out[4];
    memset(out, 0xAA, sizeof(out));
    hex2bin(out, "");
    TEST_ASSERT_EQUAL_HEX8(0xAA, out[0]);
}

static void test_hex2bin_uppercase(void) {
    uint8_t out[4] = {0};
    hex2bin(out, "AABBCCDD");
    TEST_ASSERT_EQUAL_HEX8(0xAA, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0xDD, out[3]);
}

static void test_hex2bin_lowercase(void) {
    uint8_t out[4] = {0};
    hex2bin(out, "aabbccdd");
    TEST_ASSERT_EQUAL_HEX8(0xAA, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0xDD, out[3]);
}

static void test_hex2bin_mixed_case(void) {
    uint8_t out[4] = {0};
    hex2bin(out, "AaBbCcDd");
    TEST_ASSERT_EQUAL_HEX8(0xAA, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0xDD, out[3]);
}

static void test_hex2bin_with_spaces(void) {
    uint8_t out[4] = {0};
    hex2bin(out, "AA BB CC DD");
    TEST_ASSERT_EQUAL_HEX8(0xAA, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0xDD, out[3]);
}

static void test_hex2bin_leading_trailing_spaces(void) {
    uint8_t out[2] = {0xFF, 0xFF};
    hex2bin(out, "  AA  ");
    TEST_ASSERT_EQUAL_HEX8(0xAA, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, out[1]);
}

static void test_hex2bin_multiple_spaces(void) {
    uint8_t out[4] = {0};
    hex2bin(out, "AA   BB");
    TEST_ASSERT_EQUAL_HEX8(0xAA, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, out[1]);
}

static void test_hex2bin_odd_nibble_ignored(void) {
    uint8_t out[4];
    memset(out, 0xBB, sizeof(out));
    hex2bin(out, "A");
    TEST_ASSERT_EQUAL_HEX8(0xBB, out[0]);
}

static void test_hex2bin_znp_frame(void) {
    uint8_t out[8] = {0};
    hex2bin(out, "FE 02 27 0A 01 01 2F");
    TEST_ASSERT_EQUAL_HEX8(0xFE, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x27, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0x0A, out[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01, out[4]);
    TEST_ASSERT_EQUAL_HEX8(0x01, out[5]);
    TEST_ASSERT_EQUAL_HEX8(0x2F, out[6]);
}

static void test_hex2bin_zeros(void) {
    uint8_t out[2] = {0};
    hex2bin(out, "0000");
    TEST_ASSERT_EQUAL_HEX8(0x00, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, out[1]);
}

static void test_url_decode_empty(void) {
    TEST_ASSERT_EQUAL_STRING("", urlDecode("").c_str());
}

static void test_url_decode_no_encoding(void) {
    TEST_ASSERT_EQUAL_STRING("hello", urlDecode("hello").c_str());
}

static void test_url_decode_plus_as_space(void) {
    TEST_ASSERT_EQUAL_STRING("hello world", urlDecode("hello+world").c_str());
}

static void test_url_decode_percent_20(void) {
    TEST_ASSERT_EQUAL_STRING("hello world", urlDecode("hello%20world").c_str());
}

static void test_url_decode_multiple_percent(void) {
    TEST_ASSERT_EQUAL_STRING("AAAA", urlDecode("%41%41%41%41").c_str());
}

static void test_url_decode_special_chars(void) {
    TEST_ASSERT_EQUAL_STRING("!#$&'()", urlDecode("%21%23%24%26%27%28%29").c_str());
}

static void test_url_decode_mixed(void) {
    TEST_ASSERT_EQUAL_STRING("a b c", urlDecode("a+b%20c").c_str());
}

static void test_url_decode_truncated_percent(void) {
    TEST_ASSERT_EQUAL_STRING("hello%", urlDecode("hello%").c_str());
    TEST_ASSERT_EQUAL_STRING("hello%4", urlDecode("hello%4").c_str());
}

static void test_url_decode_roundtrip(void) {
    std::string original = "a b c!#$&'()";
    std::string encoded = "a+b+c%21%23%24%26%27%28%29";
    TEST_ASSERT_EQUAL_STRING(original.c_str(), urlDecode(encoded).c_str());
}

void run_encoding_tests(void) {
    RUN_TEST(test_hex2bin_empty);
    RUN_TEST(test_hex2bin_uppercase);
    RUN_TEST(test_hex2bin_lowercase);
    RUN_TEST(test_hex2bin_mixed_case);
    RUN_TEST(test_hex2bin_with_spaces);
    RUN_TEST(test_hex2bin_leading_trailing_spaces);
    RUN_TEST(test_hex2bin_multiple_spaces);
    RUN_TEST(test_hex2bin_odd_nibble_ignored);
    RUN_TEST(test_hex2bin_znp_frame);
    RUN_TEST(test_hex2bin_zeros);
    RUN_TEST(test_url_decode_empty);
    RUN_TEST(test_url_decode_no_encoding);
    RUN_TEST(test_url_decode_plus_as_space);
    RUN_TEST(test_url_decode_percent_20);
    RUN_TEST(test_url_decode_multiple_percent);
    RUN_TEST(test_url_decode_special_chars);
    RUN_TEST(test_url_decode_mixed);
    RUN_TEST(test_url_decode_truncated_percent);
    RUN_TEST(test_url_decode_roundtrip);
}
