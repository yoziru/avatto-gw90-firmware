#ifndef DEVICE_ID_H_
#define DEVICE_ID_H_

#include <stdint.h>
#include <stdio.h>

inline void formatDeviceID(char *out, size_t capacity, const char *model, uint64_t mac) {
  uint8_t a = 0;
  uint8_t b = 0;
  for (unsigned i = 0; i < 4; ++i) {
    a ^= mac >> (8 * i);
    b ^= mac >> (8 * (i + 4));
  }
  snprintf(out, capacity, "%s-%02X%02X", model, a, b);
}

#endif
