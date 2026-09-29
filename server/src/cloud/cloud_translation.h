#pragma once

#include "english/english_ime.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// SugiIME drops the Chinese cloud-translation / gloss feature. This header is a
// no-op stub kept so the shared credential-test plumbing and the candidate
// worker still link. Signatures mirror the original header; every call is inert.
namespace CloudTranslation
{
using ApplyCallback = std::function<void(std::vector<EnglishIme::TranslationResult> results, uint64_t generation)>;

inline std::string TrimSecret(std::string value)
{
    return value;
}

inline bool IsUsableSecret(const std::string &value)
{
    return !value.empty();
}

inline void Start(const std::string &, ApplyCallback)
{
}

inline void Stop()
{
}

inline void Clear()
{
}

inline std::string LookupCache(const std::string &, EnglishIme::TranslationDirection)
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

inline void RequestMisses(std::vector<EnglishIme::TranslationQuery>, uint64_t)
{
}
} // namespace CloudTranslation
