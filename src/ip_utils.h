#ifndef IP_UTILS_H_
#define IP_UTILS_H_

#include <stdint.h>

// Parse a dotted-decimal IPv4 string into four octets.
// Returns true on success. On failure the output is left unchanged.
inline bool parseIpv4(const char *str, uint8_t octets[4]) noexcept
{
    if (str == nullptr)
    {
        return false;
    }

    uint8_t out[4] = {0, 0, 0, 0};
    uint8_t index = 0;
    uint16_t value = 0;
    uint8_t digits = 0;
    uint8_t dots = 0;

    for (const char *p = str;; p++)
    {
        const char c = *p;
        if (c >= '0' && c <= '9')
        {
            value = value * 10 + (c - '0');
            digits++;
            if (value > 255 || digits > 3)
            {
                return false;
            }
        }
        else if (c == '.')
        {
            if (digits == 0 || index >= 3)
            {
                return false;
            }
            out[index++] = value;
            value = 0;
            digits = 0;
            dots++;
        }
        else if (c == '\0')
        {
            if (digits == 0 || dots != 3)
            {
                return false;
            }
            out[index] = value;
            break;
        }
        else
        {
            return false;
        }
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        octets[i] = out[i];
    }
    return true;
}

#endif
