#ifndef FW_VERIFY_H_
#define FW_VERIFY_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

inline bool fwSigUrl(const char *firmwareUrl, char *out, size_t outSize) noexcept
{
    if (firmwareUrl == nullptr || out == nullptr || outSize == 0)
    {
        return false;
    }
    const char *query = strchr(firmwareUrl, '?');
    const int length = query == nullptr
                           ? snprintf(out, outSize, "%s.sig", firmwareUrl)
                           : snprintf(out, outSize, "%.*s.sig%s", static_cast<int>(query - firmwareUrl), firmwareUrl, query);
    return length >= 0 && static_cast<size_t>(length) < outSize;
}

#endif
