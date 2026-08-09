#ifndef CHECKSUM_H_
#define CHECKSUM_H_

#include <stdint.h>

// ZNP (TI Zigbee) frame checksum: sum of command + payload bytes, truncated to 8 bits.
inline uint8_t zbGenChksum(uint8_t cmd, const uint8_t *data, uint32_t len) noexcept
{
    uint8_t checksum = cmd;
    for (uint32_t i = 0; i < len; i++)
    {
        checksum += data[i];
    }
    return checksum;
}

#endif
