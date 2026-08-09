#include <unity.h>
#include <stdint.h>
#include <string.h>
#include "ip_utils.h"

static void test_parseIpv4_valid(void) {
    uint8_t octets[4];
    TEST_ASSERT_TRUE(parseIpv4("192.168.1.100", octets));
    TEST_ASSERT_EQUAL_UINT8(192, octets[0]);
    TEST_ASSERT_EQUAL_UINT8(168, octets[1]);
    TEST_ASSERT_EQUAL_UINT8(1, octets[2]);
    TEST_ASSERT_EQUAL_UINT8(100, octets[3]);
}

static void test_parseIpv4_zero_and_leading_zeros(void) {
    uint8_t octets[4];
    TEST_ASSERT_TRUE(parseIpv4("0.0.0.0", octets));
    TEST_ASSERT_EQUAL_UINT8(0, octets[0]);
    TEST_ASSERT_TRUE(parseIpv4("10.20.30.40", octets));
    TEST_ASSERT_EQUAL_UINT8(10, octets[0]);
    TEST_ASSERT_EQUAL_UINT8(40, octets[3]);
}

static void test_parseIpv4_max_octet(void) {
    uint8_t octets[4];
    TEST_ASSERT_TRUE(parseIpv4("255.255.255.255", octets));
    TEST_ASSERT_EQUAL_UINT8(255, octets[3]);
}

static void test_parseIpv4_invalid_rejected(void) {
    uint8_t octets[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    TEST_ASSERT_FALSE(parseIpv4("", octets));
    TEST_ASSERT_FALSE(parseIpv4("256.1.1.1", octets));
    TEST_ASSERT_FALSE(parseIpv4("1.2.3", octets));
    TEST_ASSERT_FALSE(parseIpv4("1.2.3.4.5", octets));
    TEST_ASSERT_FALSE(parseIpv4("a.b.c.d", octets));
    TEST_ASSERT_FALSE(parseIpv4("1..2.3", octets));
    TEST_ASSERT_FALSE(parseIpv4(nullptr, octets));
}

void run_ip_utils_tests(void) {
    RUN_TEST(test_parseIpv4_valid);
    RUN_TEST(test_parseIpv4_zero_and_leading_zeros);
    RUN_TEST(test_parseIpv4_max_octet);
    RUN_TEST(test_parseIpv4_invalid_rejected);
}
