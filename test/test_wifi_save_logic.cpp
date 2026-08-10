#include <unity.h>
#include "wifi_save_logic.h"

// --- Desired behaviour: a NEW network must connect before it is saved ---

// Joining a network different from the saved one is a ConnectFirst: it is
// staged in RAM and only committed after a successful connection. A
// wrong-password attempt therefore never persists.
static void test_new_network_is_connect_first(void)
{
    TEST_ASSERT_EQUAL(WifiSaveMode::ConnectFirst, wifiSaveMode("NewHome", "OldHome"));
    TEST_ASSERT_FALSE(wifiSavePersistsNow(WifiSaveMode::ConnectFirst));
}

// When nothing is saved yet, joining any network is a ConnectFirst too.
static void test_first_join_is_connect_first(void)
{
    TEST_ASSERT_EQUAL(WifiSaveMode::ConnectFirst, wifiSaveMode("Home", nullptr));
    TEST_ASSERT_EQUAL(WifiSaveMode::ConnectFirst, wifiSaveMode("Home", ""));
}

// --- Desired behaviour: tweaking the already-saved network persists ---

// Editing the saved network (IP settings, password, DHCP) persists now.
static void test_saved_network_persists_now(void)
{
    TEST_ASSERT_EQUAL(WifiSaveMode::SaveNow, wifiSaveMode("Home", "Home"));
    TEST_ASSERT_TRUE(wifiSavePersistsNow(WifiSaveMode::SaveNow));
}

// --- Desired behaviour: forget clears the saved network ---

// Empty SSID always forgets, regardless of what is saved.
static void test_empty_ssid_forgets(void)
{
    TEST_ASSERT_EQUAL(WifiSaveMode::Forget, wifiSaveMode("", "Home"));
    TEST_ASSERT_EQUAL(WifiSaveMode::Forget, wifiSaveMode(nullptr, "Home"));
    TEST_ASSERT_TRUE(wifiSavePersistsNow(WifiSaveMode::Forget));
}

// A too-short SSID (< 2 chars) is treated as forget (mirrors the handler).
static void test_short_ssid_forgets(void)
{
    TEST_ASSERT_EQUAL(WifiSaveMode::Forget, wifiSaveMode("A", "Home"));
    TEST_ASSERT_TRUE(wifiSavePersistsNow(WifiSaveMode::Forget));
}

// --- Desired behaviour: staged config only commits on a matching connect ---

// A staged save commits only when the STA is connected to the very SSID it
// staged. Wrong password (never connects) or a different network: no commit.
static void test_commit_requires_matching_connection(void)
{
    TEST_ASSERT_TRUE(wifiShouldCommit(true, true, "Home", "Home"));
    TEST_ASSERT_FALSE(wifiShouldCommit(true, false, "Home", "Home"));
    TEST_ASSERT_FALSE(wifiShouldCommit(true, true, "OtherNet", "Home"));
    TEST_ASSERT_FALSE(wifiShouldCommit(true, true, nullptr, "Home"));
    TEST_ASSERT_FALSE(wifiShouldCommit(false, true, "Home", "Home"));
}

// --- Consistency: persistence implies not a ConnectFirst ---

static void test_persistence_flag_consistency(void)
{
    TEST_ASSERT_FALSE(wifiSavePersistsNow(WifiSaveMode::ConnectFirst));
    TEST_ASSERT_TRUE(wifiSavePersistsNow(WifiSaveMode::SaveNow));
    TEST_ASSERT_TRUE(wifiSavePersistsNow(WifiSaveMode::Forget));
}

void run_wifi_save_logic_tests(void)
{
    RUN_TEST(test_new_network_is_connect_first);
    RUN_TEST(test_first_join_is_connect_first);
    RUN_TEST(test_saved_network_persists_now);
    RUN_TEST(test_empty_ssid_forgets);
    RUN_TEST(test_short_ssid_forgets);
    RUN_TEST(test_commit_requires_matching_connection);
    RUN_TEST(test_persistence_flag_consistency);
}
