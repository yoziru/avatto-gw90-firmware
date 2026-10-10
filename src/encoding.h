#ifndef ENCODING_H_
#define ENCODING_H_

#include <stdint.h>

#include <string>
#include <string_view>

constexpr uint8_t hexNibble(char c) noexcept {
    if (c >= 'a') return static_cast<uint8_t>(c - 'a' + 10);
    if (c >= 'A') return static_cast<uint8_t>(c - 'A' + 10);
    return static_cast<uint8_t>(c - '0');
}

inline std::string urlDecode(const std::string_view input) {
    std::string decoded;
    decoded.reserve(input.length());
    const size_t len = input.length();
    size_t i = 0;
    while (i < len) {
        const char c = input[i++];
        if (c == '%' && i + 1 < len) {
            decoded += static_cast<char>((hexNibble(input[i]) << 4) | hexNibble(input[i + 1]));
            i += 2;
        } else if (c == '+') {
            decoded += ' ';
        } else {
            decoded += c;
        }
    }
    return decoded;
}

#endif
