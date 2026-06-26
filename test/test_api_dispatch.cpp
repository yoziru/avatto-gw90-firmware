#include <unity.h>

static const uint8_t API_GET_PAGE = 0;
static const uint8_t API_GET_PARAM = 1;
static const uint8_t API_STARTWIFISCAN = 2;
static const uint8_t API_WIFISCANSTATUS = 3;
static const uint8_t API_GET_FILELIST = 4;
static const uint8_t API_GET_FILE = 5;
static const uint8_t API_SEND_HEX = 6;
static const uint8_t API_WIFICONNECTSTAT = 7;
static const uint8_t API_CMD = 8;
static const uint8_t API_GET_LOG = 9;
static const uint8_t API_FLASH_ZB = 10;

static const uint8_t API_PAGE_ROOT = 0;
static const uint8_t API_PAGE_GENERAL = 1;
static const uint8_t API_PAGE_ETHERNET = 2;
static const uint8_t API_PAGE_WIFI = 3;
static const uint8_t API_PAGE_ZHA_Z2M = 4;
static const uint8_t API_PAGE_SECURITY = 5;
static const uint8_t API_PAGE_SYSTOOLS = 6;
static const uint8_t API_PAGE_ABOUT = 7;
static const uint8_t API_PAGE_MQTT = 8;
static const uint8_t API_PAGE_WG = 9;

static const uint8_t CMD_ZB_ROUTER_RECON = 0;
static const uint8_t CMD_ZB_RST = 1;
static const uint8_t CMD_ZB_BSL = 2;
static const uint8_t CMD_ESP_RES = 3;
static const uint8_t CMD_ADAP_LAN = 4;
static const uint8_t CMD_ADAP_USB = 5;
static const uint8_t CMD_LED_PWR_TOG = 6;
static const uint8_t CMD_LED_USB_TOG = 7;
static const uint8_t CMD_CLEAR_LOG = 8;
static const uint8_t CMD_ESP_UPD_URL = 9;
static const uint8_t CMD_ZB_CHK_REV = 10;
static const uint8_t CMD_ZB_CHK_CON = 11;
static const uint8_t CMD_ZB_LED_TOG = 12;

static void test_api_action_values(void) {
    TEST_ASSERT_EQUAL_UINT8(0, API_GET_PAGE);
    TEST_ASSERT_EQUAL_UINT8(1, API_GET_PARAM);
    TEST_ASSERT_EQUAL_UINT8(2, API_STARTWIFISCAN);
    TEST_ASSERT_EQUAL_UINT8(3, API_WIFISCANSTATUS);
    TEST_ASSERT_EQUAL_UINT8(4, API_GET_FILELIST);
    TEST_ASSERT_EQUAL_UINT8(5, API_GET_FILE);
    TEST_ASSERT_EQUAL_UINT8(6, API_SEND_HEX);
    TEST_ASSERT_EQUAL_UINT8(7, API_WIFICONNECTSTAT);
    TEST_ASSERT_EQUAL_UINT8(8, API_CMD);
    TEST_ASSERT_EQUAL_UINT8(9, API_GET_LOG);
    TEST_ASSERT_EQUAL_UINT8(10, API_FLASH_ZB);
}

static void test_api_page_values(void) {
    TEST_ASSERT_EQUAL_UINT8(0, API_PAGE_ROOT);
    TEST_ASSERT_EQUAL_UINT8(1, API_PAGE_GENERAL);
    TEST_ASSERT_EQUAL_UINT8(2, API_PAGE_ETHERNET);
    TEST_ASSERT_EQUAL_UINT8(3, API_PAGE_WIFI);
    TEST_ASSERT_EQUAL_UINT8(4, API_PAGE_ZHA_Z2M);
    TEST_ASSERT_EQUAL_UINT8(5, API_PAGE_SECURITY);
    TEST_ASSERT_EQUAL_UINT8(6, API_PAGE_SYSTOOLS);
    TEST_ASSERT_EQUAL_UINT8(7, API_PAGE_ABOUT);
    TEST_ASSERT_EQUAL_UINT8(8, API_PAGE_MQTT);
    TEST_ASSERT_EQUAL_UINT8(9, API_PAGE_WG);
}

static void test_cmd_values(void) {
    TEST_ASSERT_EQUAL_UINT8(0, CMD_ZB_ROUTER_RECON);
    TEST_ASSERT_EQUAL_UINT8(1, CMD_ZB_RST);
    TEST_ASSERT_EQUAL_UINT8(2, CMD_ZB_BSL);
    TEST_ASSERT_EQUAL_UINT8(3, CMD_ESP_RES);
    TEST_ASSERT_EQUAL_UINT8(4, CMD_ADAP_LAN);
    TEST_ASSERT_EQUAL_UINT8(5, CMD_ADAP_USB);
    TEST_ASSERT_EQUAL_UINT8(6, CMD_LED_PWR_TOG);
    TEST_ASSERT_EQUAL_UINT8(7, CMD_LED_USB_TOG);
    TEST_ASSERT_EQUAL_UINT8(8, CMD_CLEAR_LOG);
    TEST_ASSERT_EQUAL_UINT8(9, CMD_ESP_UPD_URL);
    TEST_ASSERT_EQUAL_UINT8(10, CMD_ZB_CHK_REV);
    TEST_ASSERT_EQUAL_UINT8(11, CMD_ZB_CHK_CON);
    TEST_ASSERT_EQUAL_UINT8(12, CMD_ZB_LED_TOG);
}

static void test_api_action_to_url_mapping(void) {
    TEST_ASSERT_EQUAL_UINT8(9, API_GET_LOG);
    TEST_ASSERT_EQUAL_UINT8(8, API_CMD);
    TEST_ASSERT_EQUAL_UINT8(10, API_FLASH_ZB);
}

static void test_api_cmd_to_action_mapping(void) {
    TEST_ASSERT_EQUAL_UINT8(8, API_CMD);
    TEST_ASSERT_EQUAL_UINT8(9, CMD_ESP_UPD_URL);
    TEST_ASSERT_EQUAL_UINT8(8, CMD_CLEAR_LOG);
    TEST_ASSERT_EQUAL_UINT8(2, CMD_ZB_BSL);
    TEST_ASSERT_EQUAL_UINT8(1, CMD_ZB_RST);
    TEST_ASSERT_EQUAL_UINT8(3, CMD_ESP_RES);
}

static void test_api_get_param_names(void) {
    const char *param_refreshLogs = "refreshLogs";
    const char *param_coordMode = "coordMode";
    const char *param_zbRev = "zbRev";
    const char *param_espVer = "espVer";

    TEST_ASSERT_EQUAL_STRING("refreshLogs", param_refreshLogs);
    TEST_ASSERT_EQUAL_STRING("coordMode", param_coordMode);
    TEST_ASSERT_EQUAL_STRING("zbRev", param_zbRev);
    TEST_ASSERT_EQUAL_STRING("espVer", param_espVer);
}

static void test_coordinator_mode_mapping(void) {
    TEST_ASSERT_EQUAL_UINT8(0, (uint8_t)0);
    TEST_ASSERT_EQUAL_UINT8(1, (uint8_t)1);
    TEST_ASSERT_EQUAL_UINT8(2, (uint8_t)2);
}

void run_api_dispatch_tests(void) {
    RUN_TEST(test_api_action_values);
    RUN_TEST(test_api_page_values);
    RUN_TEST(test_cmd_values);
    RUN_TEST(test_api_action_to_url_mapping);
    RUN_TEST(test_api_cmd_to_action_mapping);
    RUN_TEST(test_api_get_param_names);
    RUN_TEST(test_coordinator_mode_mapping);
}
