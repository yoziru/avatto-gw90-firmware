#include <unity.h>

extern void run_encoding_tests(void);
extern void run_config_schema_tests(void);

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    run_encoding_tests();
    run_config_schema_tests();
    return UNITY_END();
}
