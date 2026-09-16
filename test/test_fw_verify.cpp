#include <unity.h>

#include "fw_verify.h"

static void test_sig_url_appends_suffix(void) {
    char out[64] = {0};
    TEST_ASSERT_TRUE(fwSigUrl("http://host/AVATTO-GW90-Ti.bin", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("http://host/AVATTO-GW90-Ti.bin.sig", out);
}

static void test_sig_url_rejects_small_buffer(void) {
    char out[8] = {0};
    TEST_ASSERT_FALSE(fwSigUrl("http://host/fw.bin", out, sizeof(out)));
}

static void test_sig_url_handles_bad_input(void) {
    char out[64] = {0};
    TEST_ASSERT_FALSE(fwSigUrl(nullptr, out, sizeof(out)));
    TEST_ASSERT_FALSE(fwSigUrl("http://host/fw.bin", nullptr, sizeof(out)));
    TEST_ASSERT_FALSE(fwSigUrl("http://host/fw.bin", out, 0));
}

void run_fw_verify_tests(void) {
    RUN_TEST(test_sig_url_appends_suffix);
    RUN_TEST(test_sig_url_rejects_small_buffer);
    RUN_TEST(test_sig_url_handles_bad_input);
}
