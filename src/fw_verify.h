#ifndef FW_VERIFY_H_
#define FW_VERIFY_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

inline bool fwSigUrl(const char *firmwareUrl, char *out, size_t outSize) noexcept
{
    if (firmwareUrl == nullptr || out == nullptr || outSize == 0)
    {
        return false;
    }
    const size_t urlLen = strlen(firmwareUrl);
    if (urlLen + 5 > outSize)
    {
        return false;
    }
    memcpy(out, firmwareUrl, urlLen);
    memcpy(out + urlLen, ".sig", 5);
    return true;
}

inline bool fwNeedsSignature(bool force) noexcept
{
    return !force;
}

#endif
