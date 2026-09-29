#include "tests/includes/test_framework.h"
#include "engine/local_modes/kaomoji_query.h"
#include "utils/common_utils.h"

#include <filesystem>
#include <string>
#include <vector>

namespace
{
bool KaomojiDatabaseAvailable()
{
    return std::filesystem::exists(CommonUtils::get_ime_data_path() + "\\others.db");
}

bool Contains(const std::vector<WordItem> &items, const std::string &word)
{
    for (const auto &item : items)
        if (item.word == word)
            return true;
    return false;
}
} // namespace

// Direct code lookup only: pinyin/shuangpin expansion was removed with the Chinese engine.
TEST_CASE(kaomoji_query_prefix_matches_full_pinyin)
{
    if (!KaomojiDatabaseAvailable())
        return;
    const auto results = metasequoia::local_modes::query_kaomoji("haixiu", SchemeType::JapaneseRomaji, 10).candidates;
    REQUIRE(Contains(results, "(*/ω＼*)"));
}

TEST_CASE(kaomoji_query_prefix_matches_english_word)
{
    if (!KaomojiDatabaseAvailable())
        return;
    const auto results = metasequoia::local_modes::query_kaomoji("kiss", SchemeType::JapaneseRomaji, 10).candidates;
    REQUIRE(!results.empty());
    REQUIRE(results[0].source == CandidateSource::Kaomoji);
}

TEST_CASE(kaomoji_query_single_char_pinyin_prefix)
{
    if (!KaomojiDatabaseAvailable())
        return;
    // "k" after the trigger returns kaomoji stored under the "k" code prefix.
    const auto results = metasequoia::local_modes::query_kaomoji("k", SchemeType::JapaneseRomaji, 10).candidates;
    REQUIRE(!results.empty());
}
