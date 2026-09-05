#include <unity.h>

extern void run_encoding_tests(void);
extern void run_config_schema_tests(void);
extern void run_log_tests(void);
extern void run_api_dispatch_tests(void);
extern void run_checksum_tests(void);
extern void run_zones_tests(void);
extern void run_zb_ver_tests(void);
extern void run_wifi_save_logic_tests(void);
extern void run_config_utils_tests(void);
extern void run_ha_discovery_tests(void);
extern void run_mqtt_cmd_tests(void);
extern void run_profiling_tests(void);

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    run_encoding_tests();
    run_config_schema_tests();
    run_log_tests();
    run_api_dispatch_tests();
    run_checksum_tests();
    run_zones_tests();
    run_zb_ver_tests();
    run_wifi_save_logic_tests();
    run_config_utils_tests();
    run_ha_discovery_tests();
    run_mqtt_cmd_tests();
    run_profiling_tests();
    return UNITY_END();
}
