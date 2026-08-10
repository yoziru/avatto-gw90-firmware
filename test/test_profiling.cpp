#include <unity.h>
#include "profiling.h"

static void test_min_max_tracking(void) {
    LoopStats s;
    loopStatsReset(s);
    loopStatsSample(s, 500);
    loopStatsSample(s, 2000);
    loopStatsSample(s, 800);
    TEST_ASSERT_EQUAL_UINT32(500, s.minUs);
    TEST_ASSERT_EQUAL_UINT32(2000, s.maxUs);
    TEST_ASSERT_EQUAL_UINT32(3, s.cycles);
}

static void test_ema_converges_to_steady_value(void) {
    LoopStats s;
    loopStatsReset(s);
    for (int i = 0; i < 512; i++) {
        loopStatsSample(s, 1000);
    }
    TEST_ASSERT_UINT32_WITHIN(50, 1000, s.emaUs);
}

static void test_single_spike_does_not_dominate_ema(void) {
    LoopStats s;
    loopStatsReset(s);
    for (int i = 0; i < 512; i++) {
        loopStatsSample(s, 1000);
    }
    // One 100ms spike (100000us) then steady again.
    loopStatsSample(s, 100000);
    for (int i = 0; i < 512; i++) {
        loopStatsSample(s, 1000);
    }
    TEST_ASSERT_UINT32_WITHIN(150, 1000, s.emaUs);
    TEST_ASSERT_EQUAL_UINT32(100000, s.maxUs); // but the max still records it
}

static void test_reset_clears_window(void) {
    LoopStats s;
    loopStatsReset(s);
    loopStatsSample(s, 700);
    loopStatsReset(s);
    TEST_ASSERT_EQUAL_UINT32(0, s.cycles);
    TEST_ASSERT_EQUAL_UINT32(0, s.maxUs);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, s.minUs);
}

void run_profiling_tests(void) {
    RUN_TEST(test_min_max_tracking);
    RUN_TEST(test_ema_converges_to_steady_value);
    RUN_TEST(test_single_spike_does_not_dominate_ema);
    RUN_TEST(test_reset_clears_window);
}
