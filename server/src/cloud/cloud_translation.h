#pragma once

#include <string>
#include <vector>
#include <cstdint>

// SugiIME drops the Chinese cloud-translation / gloss feature. This header is a
// no-op stub kept so the shared credential-test plumbing still links.
namespace CloudTranslation
{
inline std::string TrimSecret(std::string value)
{
    return value;
}

inline bool IsUsableSecret(const std::string &value)
{
    return !value.empty();
}

template <typename Callback>
void Start(const std::string &, Callback)
{
}

inline void Stop()
{
}

inline void Clear()
{
}

inline std::string LookupCache(const std::string &, int)
{
    return {};
}

inline bool IsCloudTranslatableEnglish(const std::string &)
{
    return false;
}

inline bool IsCloudTranslatableChinese(const std::string &)
{
    return false;
}

inline void RequestMisses(std::vector<std::string>, uint64_t)
{
}
} // namespace CloudTranslation
