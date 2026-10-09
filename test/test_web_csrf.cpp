#include <unity.h>
#include "web_csrf.h"

static void test_same_origin_allow_origin(void) {
    TEST_ASSERT_TRUE(sameOrigin("http://192.168.1.50", "192.168.1.50"));
}

static void test_same_origin_allow_origin_with_port(void) {
    TEST_ASSERT_TRUE(sameOrigin("http://192.168.1.50:8080", "192.168.1.50:8080"));
}

static void test_same_origin_allow_referer_with_path(void) {
    TEST_ASSERT_TRUE(sameOrigin("http://192.168.1.50/sys-tools?x=1", "192.168.1.50"));
}

static void test_same_origin_allow_hostname_case(void) {
    TEST_ASSERT_TRUE(sameOrigin("http://Avatto-GW90.local", "avatto-gw90.local"));
}

static void test_same_origin_allow_missing_source(void) {
    TEST_ASSERT_TRUE(sameOrigin("", "192.168.1.50"));
}

static void test_same_origin_allow_port_on_source_only(void) {
    TEST_ASSERT_TRUE(sameOrigin("http://192.168.1.50:3000", "192.168.1.50"));
}

static void test_same_origin_allow_port_on_host_only(void) {
    TEST_ASSERT_TRUE(sameOrigin("http://192.168.1.50", "192.168.1.50:8080"));
}

static void test_same_origin_allow_different_scheme(void) {
    TEST_ASSERT_TRUE(sameOrigin("https://192.168.1.50", "192.168.1.50"));
}

static void test_same_origin_reject_other_host(void) {
    TEST_ASSERT_FALSE(sameOrigin("http://evil.example", "192.168.1.50"));
}

static void test_same_origin_reject_other_referer(void) {
    TEST_ASSERT_FALSE(sameOrigin("https://evil.example/gateway", "192.168.1.50"));
}

static void test_same_origin_reject_null(void) {
    TEST_ASSERT_FALSE(sameOrigin("null", "192.168.1.50"));
}

static void test_same_origin_reject_missing_host(void) {
    TEST_ASSERT_FALSE(sameOrigin("http://192.168.1.50", ""));
}

static void test_same_origin_reject_relative_source(void) {
    TEST_ASSERT_FALSE(sameOrigin("192.168.1.50", "192.168.1.50"));
}

static void test_same_origin_ipv6(void) {
    TEST_ASSERT_TRUE(sameOrigin("http://[fe80::1]:80", "[fe80::1]:80"));
    TEST_ASSERT_FALSE(sameOrigin("http://[fe80::1]", "[fe80::2]"));
}

static void test_mutations_require_non_simple_header(void) {
    TEST_ASSERT_FALSE(webRequestAllowed("", "gateway.local", true, ""));
    TEST_ASSERT_FALSE(webRequestAllowed("", "gateway.local", true, "0"));
    TEST_ASSERT_FALSE(webRequestAllowed("http://gateway.local", "gateway.local", true, ""));
    TEST_ASSERT_TRUE(webRequestAllowed("", "gateway.local", true, "1"));
    TEST_ASSERT_FALSE(webRequestAllowed("http://evil.example", "gateway.local", true, "1"));
    TEST_ASSERT_TRUE(webRequestAllowed("", "gateway.local", false, ""));
}

void run_web_csrf_tests(void) {
    RUN_TEST(test_mutations_require_non_simple_header);
    RUN_TEST(test_same_origin_allow_origin);
    RUN_TEST(test_same_origin_allow_origin_with_port);
    RUN_TEST(test_same_origin_allow_referer_with_path);
    RUN_TEST(test_same_origin_allow_hostname_case);
    RUN_TEST(test_same_origin_allow_missing_source);
    RUN_TEST(test_same_origin_allow_port_on_source_only);
    RUN_TEST(test_same_origin_allow_port_on_host_only);
    RUN_TEST(test_same_origin_allow_different_scheme);
    RUN_TEST(test_same_origin_reject_other_host);
    RUN_TEST(test_same_origin_reject_other_referer);
    RUN_TEST(test_same_origin_reject_null);
    RUN_TEST(test_same_origin_reject_missing_host);
    RUN_TEST(test_same_origin_reject_relative_source);
    RUN_TEST(test_same_origin_ipv6);
}
