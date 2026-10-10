#include <unity.h>
#include "encoding.h"

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
