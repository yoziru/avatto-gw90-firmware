#ifndef WEB_CSRF_H_
#define WEB_CSRF_H_

#include <cstddef>
#include <string_view>

inline std::string_view authorityHost(std::string_view authority) noexcept
{
    const std::size_t at = authority.find('@');
    if (at != std::string_view::npos)
        authority = authority.substr(at + 1);
    if (!authority.empty() && authority.front() == '[')
    {
        const std::size_t close = authority.find(']');
        return close == std::string_view::npos ? std::string_view() : authority.substr(0, close + 1);
    }
    const std::size_t colon = authority.find(':');
    return colon == std::string_view::npos ? authority : authority.substr(0, colon);
}

inline std::string_view urlHost(std::string_view url) noexcept
{
    const std::size_t scheme = url.find("://");
    if (scheme == std::string_view::npos)
        return {};
    std::string_view rest = url.substr(scheme + 3);
    const std::size_t end = rest.find_first_of("/?#");
    if (end != std::string_view::npos)
        rest = rest.substr(0, end);
    return authorityHost(rest);
}

inline bool sameOrigin(std::string_view source, std::string_view host) noexcept
{
    if (source.empty())
        return true;
    const std::string_view sourceHost = urlHost(source);
    const std::string_view requestHost = authorityHost(host);
    if (sourceHost.empty() || requestHost.empty() || sourceHost.size() != requestHost.size())
        return false;
    for (std::size_t i = 0; i < sourceHost.size(); i++)
    {
        char a = sourceHost[i];
        char b = requestHost[i];
        if (a >= 'A' && a <= 'Z')
            a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z')
            b += 'a' - 'A';
        if (a != b)
            return false;
    }
    return true;
}

#endif
