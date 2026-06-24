#ifndef ENCODING_H_
#define ENCODING_H_

#include <stdint.h>
#include <cstdlib>
#include <string>

inline std::string urlDecode(const std::string &input)
{
    std::string decoded;
    char temp[] = "0x00";
    size_t len = input.length();
    size_t i = 0;
    while (i < len)
    {
        char c = input[i++];
        if (c == '%' && i + 1 < len)
        {
            temp[2] = input[i++];
            temp[3] = input[i++];
            decoded += (char)strtol(temp, NULL, 16);
        }
        else if (c == '+')
        {
            decoded += ' ';
        }
        else
        {
            decoded += c;
        }
    }
    return decoded;
}

inline void hex2bin(uint8_t *out, const char *in)
{
    while (*in)
    {
        while (*in == ' ')
            in++;
        if (!*in)
            break;
        uint8_t c = *in >= 'a' ? *in - 'a' + 10 : *in >= 'A' ? *in - 'A' + 10
                                                              : *in - '0';
        in++;
        c <<= 4;
        if (!*in)
            break;
        c |= *in >= 'a' ? *in - 'a' + 10 : *in >= 'A' ? *in - 'A' + 10
                                                       : *in - '0';
        in++;
        *out++ = c;
    }
}

#endif
