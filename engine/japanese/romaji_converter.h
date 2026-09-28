#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace japanese
{
struct RomajiConversion
{
    std::string hiragana;
    std::string pending;
    bool complete = false;
};

RomajiConversion ConvertRomaji(std::string_view input);
std::string HiraganaToKatakana(std::string_view hiragana);
std::string KatakanaToHiragana(std::string_view katakana);
std::string HiraganaToRomaji(std::string_view kana);
// Full-width katakana (JIS X 0208) to half-width katakana (JIS X 0201); voiced
// marks use the combining dakuten U+FF9E / handakuten U+FF9F. Non-kana
// characters pass through unchanged.
std::string KatakanaToHalfWidth(std::string_view katakana);
std::string HiraganaToHalfWidthKatakana(std::string_view hiragana);
// Half-width ASCII to full-width (JIS X 0201 latin -> full-width forms).
std::string AsciiToFullWidth(std::string_view ascii);
bool IsSingleKanaConversion(const RomajiConversion &conversion);

// Romaji prefixes such as "k" or "ky" map to every table kana whose spelling
// starts with that prefix. This is the Japanese counterpart of Google Pinyin's
// half spelling id (shengmu).
std::vector<std::string> KanaForRomajiPrefix(std::string_view pending);
} // namespace japanese
