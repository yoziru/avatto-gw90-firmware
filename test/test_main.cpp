#include <unity.h>

extern void run_encoding_tests(void);
extern void run_checksum_tests(void);
extern void run_zones_tests(void);
extern void run_zb_ver_tests(void);
extern void run_zb_info_tests(void);
extern void run_wifi_save_logic_tests(void);
extern void run_config_utils_tests(void);
extern void run_ha_discovery_tests(void);
extern void run_mqtt_cmd_tests(void);
extern void run_profiling_tests(void);
extern void run_web_csrf_tests(void);
extern void run_boot_guard_logic_tests(void);
extern void run_fw_verify_tests(void);
extern void run_device_id_tests(void);
extern void run_intel_hex_record_tests(void);

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    run_encoding_tests();
    run_checksum_tests();
    run_zones_tests();
    run_zb_ver_tests();
    run_zb_info_tests();
    run_wifi_save_logic_tests();
    run_config_utils_tests();
    run_ha_discovery_tests();
    run_mqtt_cmd_tests();
    run_profiling_tests();
    run_web_csrf_tests();
    run_boot_guard_logic_tests();
    run_fw_verify_tests();
    run_device_id_tests();
    run_intel_hex_record_tests();
    return UNITY_END();
}
