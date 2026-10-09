#include <unity.h>
#include "device_id.h"

static void test_device_id_is_bounded_and_stable(void) {
    char full[20];
    formatDeviceID(full, sizeof(full), "AVATTO-GW90-TI", UINT64_C(0x0123456789ABCDEF));
    TEST_ASSERT_EQUAL_STRING("AVATTO-GW90-TI-0000", full);

    struct { char id[18]; char guard; } small = {{0}, '!'};
    formatDeviceID(small.id, sizeof(small.id), "AVATTO-GW90-TI", UINT64_C(0x12));
    TEST_ASSERT_EQUAL_STRING("AVATTO-GW90-TI-12", small.id);
    TEST_ASSERT_EQUAL_CHAR('!', small.guard);
    formatDeviceID(&small.guard, 0, "AVATTO-GW90-TI", 0);
    TEST_ASSERT_EQUAL_CHAR('!', small.guard);
}

void run_device_id_tests(void) {
    RUN_TEST(test_device_id_is_bounded_and_stable);
}
