#include <unity.h>
#include <string>
#include "../lib/IntelHex/src/IntelHexRecord.h"

static std::string record(uint8_t length, uint16_t address, uint8_t type) {
    char header[10];
    snprintf(header, sizeof(header), ":%02X%04X%02X", length, address, type);
    std::string line(header);
    uint8_t sum = length + (address >> 8) + address + type;
    line.append(size_t(length) * 2, '0');
    char checksum[3];
    snprintf(checksum, sizeof(checksum), "%02X", uint8_t(-sum));
    return line + checksum;
}

static void test_hex_record_rejects_corruption_and_preserves_long_data(void) {
    IntelHexRecord decoded;
    std::string line = record(255, 0x7F00, 0);
    TEST_ASSERT_TRUE(decodeIntelHexRecord(line.c_str(), line.size(), decoded));
    TEST_ASSERT_EQUAL_UINT8(255, decoded.length);
    TEST_ASSERT_EQUAL_UINT8(0, decoded.data[254]);
    const char *bad[] = {":00000001FG", ":0100000000", ":00000001FF00", ":00000002FE", ":00000006FA", "garbage"};
    for (const char *value : bad) TEST_ASSERT_FALSE(decodeIntelHexRecord(value, strlen(value), decoded));
    line = record(2, 0xFFFF, 0);
    TEST_ASSERT_FALSE(decodeIntelHexRecord(line.c_str(), line.size(), decoded));
    line = record(2, 0, 4);
    TEST_ASSERT_TRUE(decodeIntelHexRecord(line.c_str(), line.size(), decoded));
    TEST_ASSERT_TRUE(decodeIntelHexRecord(":00000001FF", 11, decoded));
}

void run_intel_hex_record_tests(void) {
    RUN_TEST(test_hex_record_rejects_corruption_and_preserves_long_data);
}
