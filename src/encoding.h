#ifndef ENCODING_H_
#define ENCODING_H_

#include <stdint.h>
#include <cstdlib>
#include <string>

inline uint8_t hexNibble(char c) {
    if (c >= 'a') return c - 'a' + 10;
    if (c >= 'A') return c - 'A' + 10;
    return c - '0';
}

inline std::string urlDecode(const std::string &input) {
    std::string decoded;
    char hexBuf[] = "0x00";
    size_t len = input.length();
    size_t i = 0;
    while (i < len) {
        char c = input[i++];
        if (c == '%' && i + 1 < len) {
            hexBuf[2] = input[i++];
            hexBuf[3] = input[i++];
            decoded += (char)strtol(hexBuf, NULL, 16);
        } else if (c == '+') {
            decoded += ' ';
        } else {
            decoded += c;
        }
    }
    return decoded;
}

inline void hex2bin(uint8_t *out, const char *in) {
    while (*in) {
        while (*in == ' ')
            in++;
        if (!*in)
            break;
        uint8_t c = hexNibble(*in++);
        c <<= 4;
        if (!*in)
            break;
        c |= hexNibble(*in++);
        *out++ = c;
    }
}

#endif
