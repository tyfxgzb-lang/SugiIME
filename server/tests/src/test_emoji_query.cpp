#include "tests/includes/test_framework.h"
#include "engine/local_modes/emoji_query.h"
#include "utils/common_utils.h"

#include <filesystem>
#include <string>
#include <vector>

namespace
{
const std::string kSmiley = "\xF0\x9F\x98\x80"; // 😀

bool EmojiDatabaseAvailable()
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

// The query matches the stored code directly; the scheme argument is retained for API
// compatibility but ignored. SugiIME dropped pinyin/shuangpin expansion, so only the direct
// full-code and English lookups are exercised here.
TEST_CASE(emoji_query_prefix_matches_full_pinyin)
{
    if (!EmojiDatabaseAvailable())
        return;
    const auto results = metasequoia::local_modes::query_emoji("xiaolian", SchemeType::JapaneseRomaji, 10).candidates;
    REQUIRE(!results.empty());
    REQUIRE_EQ(results[0].word, kSmiley);
    REQUIRE(results[0].source == CandidateSource::Emoji);
}

TEST_CASE(emoji_query_prefix_matches_english_word)
{
    if (!EmojiDatabaseAvailable())
        return;
    const auto results = metasequoia::local_modes::query_emoji("laugh", SchemeType::JapaneseRomaji, 50).candidates;
    REQUIRE(Contains(results, kSmiley));
}
