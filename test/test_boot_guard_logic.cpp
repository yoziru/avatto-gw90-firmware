#include <unity.h>
#include "boot_guard_logic.h"

// --- The guard is inert unless an OTA armed it ---

static void test_disarmed_never_acts(void)
{
    TEST_ASSERT_EQUAL(BootGuardDecision::Wait, bootGuardDecision(false, 0, false, 0, 0));
    TEST_ASSERT_EQUAL(BootGuardDecision::Wait, bootGuardDecision(false, 99, true, 600000, 600000));
    TEST_ASSERT_EQUAL(BootGuardDecision::Wait, bootGuardDecision(false, 4, false, 0, 600000));
}

// --- Health must hold for the settling period before confirming ---

static void test_healthy_before_settle_waits(void)
{
    TEST_ASSERT_EQUAL(BootGuardDecision::Wait, bootGuardDecision(true, 1, true, 0, 0));
    TEST_ASSERT_EQUAL(BootGuardDecision::Wait, bootGuardDecision(true, 1, true, BOOT_GUARD_SETTLE_MS - 1, 1000));
}

static void test_health_confirms_after_settle(void)
{
    TEST_ASSERT_EQUAL(BootGuardDecision::Confirm, bootGuardDecision(true, 1, true, BOOT_GUARD_SETTLE_MS, 1000));
    TEST_ASSERT_EQUAL(BootGuardDecision::Confirm, bootGuardDecision(true, BOOT_GUARD_MAX_BOOT_ATTEMPTS, true, BOOT_GUARD_SETTLE_MS, 0));
}

// A device that did become healthy is never rolled back, even on the last
// allowed attempt or after a long unhealthy start.
static void test_confirm_wins_over_rollback(void)
{
    TEST_ASSERT_EQUAL(BootGuardDecision::Confirm,
                      bootGuardDecision(true, BOOT_GUARD_MAX_BOOT_ATTEMPTS + 1, true, BOOT_GUARD_SETTLE_MS, 0));
    TEST_ASSERT_EQUAL(BootGuardDecision::Confirm,
                      bootGuardDecision(true, 1, true, BOOT_GUARD_SETTLE_MS, BOOT_GUARD_ATTEMPT_TIMEOUT_MS * 2));
}

// --- Crash loops spend attempts until the budget is gone ---

static void test_attempt_budget_exhausted_rolls_back(void)
{
    TEST_ASSERT_EQUAL(BootGuardDecision::Wait,
                      bootGuardDecision(true, BOOT_GUARD_MAX_BOOT_ATTEMPTS, false, 0, 0));
    TEST_ASSERT_EQUAL(BootGuardDecision::Rollback,
                      bootGuardDecision(true, BOOT_GUARD_MAX_BOOT_ATTEMPTS + 1, false, 0, 0));
}

// --- A boot that stays up but never reaches the checkpoint times out ---

static void test_attempt_timeout_rolls_back(void)
{
    TEST_ASSERT_EQUAL(BootGuardDecision::Wait,
                      bootGuardDecision(true, 1, false, 0, BOOT_GUARD_ATTEMPT_TIMEOUT_MS - 1));
    TEST_ASSERT_EQUAL(BootGuardDecision::Rollback,
                      bootGuardDecision(true, 1, false, 0, BOOT_GUARD_ATTEMPT_TIMEOUT_MS));
}

// Flapping health (never settling) still rolls back at the deadline.
static void test_flapping_health_times_out(void)
{
    TEST_ASSERT_EQUAL(BootGuardDecision::Wait, bootGuardDecision(true, 1, true, 30000, BOOT_GUARD_ATTEMPT_TIMEOUT_MS - 1));
    TEST_ASSERT_EQUAL(BootGuardDecision::Rollback, bootGuardDecision(true, 1, true, 30000, BOOT_GUARD_ATTEMPT_TIMEOUT_MS));
}

void run_boot_guard_logic_tests(void)
{
    RUN_TEST(test_disarmed_never_acts);
    RUN_TEST(test_healthy_before_settle_waits);
    RUN_TEST(test_health_confirms_after_settle);
    RUN_TEST(test_confirm_wins_over_rollback);
    RUN_TEST(test_attempt_budget_exhausted_rolls_back);
    RUN_TEST(test_attempt_timeout_rolls_back);
    RUN_TEST(test_flapping_health_times_out);
}
