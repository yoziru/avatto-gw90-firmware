#ifndef INTEL_HEX_RECORD_H_
#define INTEL_HEX_RECORD_H_

#include <stddef.h>
#include <stdint.h>

struct IntelHexRecord {
    uint8_t length = 0;
    uint16_t address = 0;
    uint8_t type = 0;
    uint8_t data[255] = {};
};

inline int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline bool decodeIntelHexRecord(const char *line, size_t size, IntelHexRecord &record) {
    if (!line || size < 11 || size > 521 || line[0] != ':' || size % 2 != 1) return false;
    uint8_t bytes[260];
    const size_t count = (size - 1) / 2;
    uint8_t sum = 0;
    for (size_t i = 0; i < count; ++i) {
        const int high = hexNibble(line[1 + 2 * i]);
        const int low = hexNibble(line[2 + 2 * i]);
        if (high < 0 || low < 0) return false;
        bytes[i] = (high << 4) | low;
        sum += bytes[i];
    }
    if (sum != 0 || count != static_cast<size_t>(bytes[0]) + 5) return false;
    record.length = bytes[0];
    record.address = (bytes[1] << 8) | bytes[2];
    record.type = bytes[3];
    switch (record.type) {
        case 0: if (record.length == 0 || uint32_t(record.address) + record.length > 0x10000) return false; break;
        case 1: if (record.length != 0 || record.address != 0) return false; break;
        case 2: case 4: if (record.length != 2 || record.address != 0) return false; break;
        case 3: case 5: if (record.length != 4 || record.address != 0) return false; break;
        default: return false;
    }
    for (size_t i = 0; i < record.length; ++i) record.data[i] = bytes[4 + i];
    return true;
}

#endif
