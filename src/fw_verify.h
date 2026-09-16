#ifndef FW_VERIFY_H_
#define FW_VERIFY_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

inline bool fwSigUrl(const char *firmwareUrl, char *out, size_t outSize) noexcept
{
    if (firmwareUrl == nullptr || out == nullptr || outSize == 0)
    {
        return false;
    }
    const int length = snprintf(out, outSize, "%s.sig", firmwareUrl);
    return length >= 0 && static_cast<size_t>(length) < outSize;
}

#endif
