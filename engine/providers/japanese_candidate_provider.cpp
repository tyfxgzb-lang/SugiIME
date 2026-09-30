#include "japanese_candidate_provider.h"
#include "../japanese/japanese_glossary.h"
#include "../japanese/japanese_matrix_search.h"
#include "../japanese/romaji_converter.h"
#include <algorithm>
#include <cctype>
#include <mutex>
#include <iterator>
#include <string_view>
#include <unordered_map>
#include "../core/data_path.h"
#include "../contracts/assets/assets.h"
#include <utf8.h>
#include <unordered_set>
#include <utility>

namespace
{
constexpr int kNoMutation = 0;

using SharedDecoder = std::shared_ptr<const japanese::JapaneseSentenceDecoder>;

SharedDecoder SharedSentenceDecoder(const std::string &path)
{
    static std::mutex mutex;
    static std::unordered_map<std::string, std::weak_ptr<const japanese::JapaneseSentenceDecoder>> models;
    std::lock_guard lock(mutex);
    for (auto it = models.begin(); it != models.end();)
        it = it->second.expired() ? models.erase(it) : std::next(it);
    auto &entry = models[path];
    if (auto existing = entry.lock())
        return existing;
    auto model = std::make_shared<const japanese::JapaneseSentenceDecoder>(path);
    if (model->ready())
        entry = model;
    return model;
}

void AppendUnique(std::vector<WordItem> &items, std::unordered_set<std::string> &seen, const std::string &code,
                  const std::string &value, std::int64_t weight, CandidateSource source = CandidateSource::Database)
{
    if (!value.empty() && seen.insert(value).second)
    {
        items.emplace_back(code, value, weight, source, code);
    }
}

std::string EscapeLikePrefix(const std::string &code)
{
    std::string escaped;
    escaped.reserve(code.size() + 1);
    for (const char ch : code)
    {
        if (ch == '%' || ch == '_' || ch == '#')
            escaped.push_back('#');
        escaped.push_back(ch);
    }
    escaped.push_back('%');
    return escaped;
}

// Count UTF-8 code points in a string. Japanese "字数" is the number of
// characters, not bytes, so length-based association sorting must use this.
std::size_t Utf8CharCount(std::string_view s)
{
    std::size_t count = 0;
    for (unsigned char c : s)
    {
        if ((c & 0xC0) != 0x80)
            ++count;
    }
    return count;
}

} // namespace

JapaneseCandidateProvider::JapaneseCandidateProvider(std::string db_path, std::string model_path)
    : db_path_(db_path.empty()
                   ? metasequoia::path_to_utf8(metasequoia::data_file_path(metasequoia::assets::main_dictionary))
                   : std::move(db_path)),
      model_path_(model_path.empty()
                      ? metasequoia::path_to_utf8(metasequoia::data_file_path(metasequoia::assets::japanese_model))
                      : std::move(model_path))
{
}

JapaneseCandidateProvider::~JapaneseCandidateProvider()
{
    close_database();
}

std::vector<WordItem> JapaneseCandidateProvider::query(const QueryRequest &request)
{
    if (!request.valid || (request.scheme != SchemeType::JapaneseRomaji && request.scheme != SchemeType::JapaneseKana))
    {
        return {};
    }

    const bool direct_kana_input = request.scheme == SchemeType::JapaneseKana;

    // 单独按 '-' 时给出两个候选：长音符 ー 在前，普通连字符 '-' 在后。
    // 这里直接返回，避免句子搜索在两项之间插进无关候选。
    if (request.raw_input == "-")
    {
        std::vector<WordItem> candidates;
        std::unordered_set<std::string> seen;
        AppendUnique(candidates, seen, request.raw_input_with_cases, "ー", 1000000, CandidateSource::Generated);
        AppendUnique(candidates, seen, request.raw_input_with_cases, "-", 999999, CandidateSource::Generated);
        return candidates;
    }

    // Romaji mode converts the typed letters; direct-kana (JIS) mode already
    // holds the full hiragana string with no pending tail.
    japanese::RomajiConversion conversion;
    if (direct_kana_input)
    {
        conversion.hiragana = request.raw_input;
        conversion.pending.clear();
        conversion.complete = true;
    }
    else
    {
        conversion = japanese::ConvertRomaji(request.raw_input);
    }

    if (!sentence_decoder_)
        sentence_decoder_ = SharedSentenceDecoder(model_path_);

    // Gather every non-kana candidate into one pool. The kana forms are placed
    // at the very top afterwards, and the pool is split into the top two common
    // words (by weight) plus an association tail sorted by character length.
    std::vector<WordItem> word_pool;
    std::unordered_set<std::string> seen;

    // Internet slang abbreviations (w, ktkr, ggrks, ...) are not romaji the
    // converter understands, so they would never surface from the model. On an
    // exact match inject the slang surface as a high-priority QuickPhrase
    // candidate (right after the kana lead); its full-form gloss is attached
    // at candidate-page build time via japanese::LookUpCandidateGloss.
    if (!direct_kana_input)
    {
        std::string typed_lower = request.raw_input;
        std::transform(typed_lower.begin(), typed_lower.end(), typed_lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const auto &slang : japanese::InternetSlangEntries())
        {
            if (slang.code == typed_lower)
            {
                AppendUnique(word_pool, seen, request.raw_input_with_cases, slang.surface, 1250000,
                             CandidateSource::QuickPhrase);
            }
        }
    }

    const bool hiragana_complete = !conversion.hiragana.empty();

    if (sentence_decoder_ && sentence_decoder_->ready() && hiragana_complete)
    {
        const auto pending_kana = japanese::KanaForRomajiPrefix(conversion.pending);
        if (!conversion.pending.empty())
        {
            const std::string typed = request.raw_input;
            for (const auto &kana : pending_kana)
            {
                for (const auto &lemma : sentence_decoder_->PrefixLemmas(conversion.hiragana + kana, 24))
                {
                    const std::string romaji = japanese::HiraganaToRomaji(lemma.reading);
                    if (romaji.size() < typed.size() || romaji.compare(0, typed.size(), typed) != 0)
                        continue;
                    AppendUnique(word_pool, seen, request.raw_input_with_cases, lemma.surface, 980000 - lemma.word_cost,
                                 CandidateSource::Database);
                }
            }
        }
        else if (conversion.hiragana.size() >= 6)
        {
            for (const auto &lemma : sentence_decoder_->PrefixLemmas(conversion.hiragana, 16))
                AppendUnique(word_pool, seen, request.raw_input_with_cases, lemma.surface, 980000 - lemma.word_cost,
                             CandidateSource::Database);
        }
        japanese::JapaneseMatrixSearch search(*sentence_decoder_);
        for (const auto &sentence : search.SearchConverted(conversion, 12))
        {
            AppendUnique(word_pool, seen, request.raw_input_with_cases, sentence.text, 900000 - sentence.cost,
                         CandidateSource::Database);
        }
    }

    if (ensure_query_statement())
    {
        sqlite3_reset(query_statement_);
        sqlite3_clear_bindings(query_statement_);
        sqlite3_bind_text(query_statement_, 1, request.raw_input_with_cases.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(query_statement_, 2, request.raw_input.c_str(), -1, SQLITE_TRANSIENT);
        const std::string like_raw = EscapeLikePrefix(request.raw_input);
        const std::string like_q = EscapeLikePrefix(std::string("q") + request.raw_input);
        sqlite3_bind_text(query_statement_, 3, like_raw.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(query_statement_, 4, like_q.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(query_statement_) == SQLITE_ROW)
        {
            const auto *code = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 0));
            const auto *value = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 1));
            if (code && value)
            {
                AppendUnique(word_pool, seen, code, value, sqlite3_column_int64(query_statement_, 2));
            }
        }
    }

    const auto dynamic = dynamic_candidates_.get(request.raw_input);
    if (dynamic.has_value())
    {
        for (const auto &item : *dynamic)
        {
            if (seen.insert(item.word).second)
            {
                word_pool.push_back(item);
            }
        }
    }

    // Fuzzy voicing (濁音・半濁音の曖昧入力): users who confuse ga/ka, za/sa,
    // da/ta, ba/ha, pa/ha etc. get the corrected reading offered as a candidate.
    // For each mora we try adding / stripping dakuten (and handakuten on the
    // は row) and surface any lemmas the swapped reading resolves to. Each swap
    // is gated on its row pair in request.japanese_fuzzy_mask; a zero mask
    // disables fuzzy candidates entirely.
    if (request.japanese_fuzzy_mask != 0 && sentence_decoder_ && sentence_decoder_->ready() && hiragana_complete)
    {
        const std::string &reading = conversion.hiragana;
        std::vector<std::uint32_t> cps;
        {
            auto it = reading.begin();
            while (it != reading.end())
            {
                try
                {
                    cps.push_back(utf8::next(it, reading.end()));
                }
                catch (...)
                {
                    break;
                }
            }
        }
        // Clear kana that take dakuten: かきくけこ さしすせそ たちつてと はひふへほ
        static const std::unordered_set<std::uint32_t> kDakutenCapable = {
            0x304B, 0x304D, 0x304F, 0x3051, 0x3053, // かきくけこ
            0x3055, 0x3057, 0x3059, 0x305B, 0x305D, // さしすせそ
            0x305F, 0x3061, 0x3064, 0x3066, 0x3068, // たちつてと
            0x306F, 0x3072, 0x3075, 0x3078, 0x307B  // はひふへほ
        };
        // は行 (U+306F..) supports handakuten as well: clear + 2.
        static const std::unordered_set<std::uint32_t> kHandakutenCapable = {0x306F, 0x3072, 0x3075, 0x3078, 0x307B};
        // Dakuten versions: clear + 1. Handakuten: は行 clear + 2.
        auto is_dakuten = [](std::uint32_t cp) {
            return (cp >= 0x304C && cp <= 0x3054 && cp % 2 == 0) || // がぎぐげご etc (+1 from clear)
                   (cp >= 0x3056 && cp <= 0x305E && cp % 2 == 0) || (cp >= 0x3060 && cp <= 0x3069 && cp % 2 == 0) ||
                   (cp >= 0x3070 && cp <= 0x307D); // ばびぶべぼ (+1 from は行)
        };
        auto is_handakuten = [](std::uint32_t cp) {
            return cp >= 0x3071 && cp <= 0x307D && (cp - 0x306F) % 3 == 2; // ぱぴぷぺぽ
        };

        const int kMaxFuzzyVariants = 12;
        int variants_tried = 0;
        // Each swap carries the configuration bit of its row pair; a disabled
        // pair never generates a variant reading.
        struct FuzzySwap
        {
            std::uint32_t cp;
            std::uint32_t bit;
        };
        for (std::size_t i = 0; i < cps.size() && variants_tried < kMaxFuzzyVariants; ++i)
        {
            const std::uint32_t cp = cps[i];
            std::vector<FuzzySwap> swaps;
            if (kDakutenCapable.count(cp))
            {
                const std::uint32_t bit = cp <= 0x3053   ? kJapaneseFuzzyKaGa
                                          : cp <= 0x305D ? kJapaneseFuzzySaZa
                                          : cp <= 0x3068 ? kJapaneseFuzzyTaDa
                                                         : kJapaneseFuzzyHaBa;
                swaps.push_back({cp + 1, bit}); // add dakuten
                if (kHandakutenCapable.count(cp))
                    swaps.push_back({cp + 2, kJapaneseFuzzyHaPa}); // add handakuten
            }
            else if (is_dakuten(cp))
            {
                const std::uint32_t bit = cp <= 0x3054   ? kJapaneseFuzzyKaGa
                                          : cp <= 0x305E ? kJapaneseFuzzySaZa
                                          : cp <= 0x3069 ? kJapaneseFuzzyTaDa
                                          : is_handakuten(cp) ? kJapaneseFuzzyHaPa
                                                              : kJapaneseFuzzyHaBa;
                swaps.push_back({cp - 1, bit}); // strip dakuten (ぱ行 → ば行 also lands here)
            }
            else if (is_handakuten(cp))
            {
                swaps.push_back({cp - 2, kJapaneseFuzzyHaPa}); // strip handakuten → clear
                swaps.push_back({cp - 1, kJapaneseFuzzyHaPa}); // strip handakuten → dakuten
            }
            for (const FuzzySwap &swap : swaps)
            {
                if ((request.japanese_fuzzy_mask & swap.bit) == 0)
                    continue;
                if (variants_tried >= kMaxFuzzyVariants)
                    break;
                ++variants_tried;
                std::vector<std::uint32_t> variant_cps = cps;
                variant_cps[i] = swap.cp;
                std::string variant_reading;
                for (std::uint32_t v : variant_cps)
                    utf8::append(v, std::back_inserter(variant_reading));
                for (const auto &lemma : sentence_decoder_->PrefixLemmas(variant_reading, 8))
                {
                    if (lemma.reading == variant_reading)
                    {
                        AppendUnique(word_pool, seen, request.raw_input_with_cases, lemma.surface,
                                     970000 - lemma.word_cost, CandidateSource::Generated);
                    }
                }
            }
        }
    }

    // Candidate 1 is the kana form of the whole reading. In Auto mode the lead
    // is hiragana by default, matching the inline preedit; if the top decoded
    // surface is a katakana-only loanword (外来語), the lead flips to katakana
    // so loanwords like アルバイト surface directly. F6-F11 pin a specific form.
    std::vector<WordItem> kana_leads;
    if (hiragana_complete)
    {
        std::string lead;
        switch (request.japanese_kana_form)
        {
        case JapaneseKanaForm::Hiragana:
            lead = conversion.hiragana;
            break;
        case JapaneseKanaForm::HalfWidthKatakana:
            lead = japanese::HiraganaToHalfWidthKatakana(conversion.hiragana);
            break;
        case JapaneseKanaForm::FullWidthRomaji:
            lead = japanese::AsciiToFullWidth(japanese::HiraganaToRomaji(conversion.hiragana));
            break;
        case JapaneseKanaForm::HalfWidthRomaji:
            lead = japanese::HiraganaToRomaji(conversion.hiragana);
            break;
        case JapaneseKanaForm::Auto: {
            // Default lead is hiragana. If the top decoded surface is a
            // katakana-only loanword (外来語), flip the lead to katakana so
            // words like アルバイト appear directly as candidate 1.
            lead = conversion.hiragana;
            for (const auto &item : word_pool)
            {
                if (item.word.empty())
                    continue;
                // Decode UTF-8 and check every codepoint is katakana.
                auto it = item.word.begin();
                std::vector<std::uint32_t> cps;
                while (it != item.word.end())
                {
                    try
                    {
                        cps.push_back(utf8::next(it, item.word.end()));
                    }
                    catch (...)
                    {
                        break;
                    }
                }
                if (!cps.empty() && std::all_of(cps.begin(), cps.end(), [](std::uint32_t cp) {
                        return (cp >= 0x30A1 && cp <= 0x30FA) || cp == 0x30FC;
                    }))
                {
                    lead = japanese::HiraganaToKatakana(conversion.hiragana);
                }
                break;
            }
            break;
        }
        case JapaneseKanaForm::Katakana:
        default:
            lead = japanese::HiraganaToKatakana(conversion.hiragana);
            break;
        }
        AppendUnique(kana_leads, seen, request.raw_input_with_cases, lead, 1000000, CandidateSource::Generated);
    }

    // Split the word pool: top two by weight are "common" candidates, the rest
    // become association candidates sorted by character count ascending
    // (shorter words first), ties broken by weight descending.
    std::sort(word_pool.begin(), word_pool.end(),
              [](const WordItem &a, const WordItem &b) { return a.weight > b.weight; });

    std::vector<WordItem> common;
    std::vector<WordItem> association;
    const std::size_t common_count = std::min<std::size_t>(2, word_pool.size());
    common.assign(word_pool.begin(), word_pool.begin() + static_cast<std::ptrdiff_t>(common_count));
    association.assign(word_pool.begin() + static_cast<std::ptrdiff_t>(common_count), word_pool.end());
    std::stable_sort(association.begin(), association.end(), [](const WordItem &a, const WordItem &b) {
        const std::size_t la = Utf8CharCount(a.word);
        const std::size_t lb = Utf8CharCount(b.word);
        if (la != lb)
            return la < lb;
        return a.weight > b.weight;
    });

    std::vector<WordItem> candidates;
    // When the user pinned a specific kana form (hiragana/katakana/romaji via
    // F6-F11 or the character-set switch), the candidate bar must show kana
    // only — no kanji words from the dictionary.
    if (request.japanese_kana_form != JapaneseKanaForm::Auto)
    {
        candidates.reserve(kana_leads.size());
        candidates.insert(candidates.end(), kana_leads.begin(), kana_leads.end());
        return candidates;
    }
    candidates.reserve(kana_leads.size() + common.size() + association.size());
    candidates.insert(candidates.end(), kana_leads.begin(), kana_leads.end());
    candidates.insert(candidates.end(), common.begin(), common.end());
    candidates.insert(candidates.end(), association.begin(), association.end());
    return candidates;
}

std::optional<WordItem> JapaneseCandidateProvider::find_candidate(SchemeType scheme, const std::string &key,
                                                                  const std::string &value)
{
    if (!IsJapaneseScheme(scheme) || !ensure_query_statement())
        return std::nullopt;
    sqlite3_reset(query_statement_);
    sqlite3_clear_bindings(query_statement_);
    sqlite3_bind_text(query_statement_, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(query_statement_, 2, key.c_str(), -1, SQLITE_TRANSIENT);
    const std::string like_raw = EscapeLikePrefix(key);
    const std::string like_q = EscapeLikePrefix(std::string("q") + key);
    sqlite3_bind_text(query_statement_, 3, like_raw.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(query_statement_, 4, like_q.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(query_statement_) == SQLITE_ROW)
    {
        const auto *code = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 0));
        const auto *candidate = reinterpret_cast<const char *>(sqlite3_column_text(query_statement_, 1));
        if (code && candidate && value == candidate)
            return WordItem(code, candidate, sqlite3_column_int64(query_statement_, 2), CandidateSource::Database,
                            code);
    }
    return std::nullopt;
}

void JapaneseCandidateProvider::reset_cache()
{
    close_database();
    dynamic_candidates_.clear();
    // The sentence model is immutable and shared process-wide. Keep it warm when
    // SQLite/user-dictionary caches are reset.
}

int JapaneseCandidateProvider::create_word(SchemeType, std::string, std::string)
{
    return kNoMutation;
}
int JapaneseCandidateProvider::update_weight_by_pinyin_and_word(SchemeType, std::string, std::string)
{
    return kNoMutation;
}
int JapaneseCandidateProvider::delete_by_pinyin_and_word(SchemeType, std::string, std::string)
{
    return kNoMutation;
}
int JapaneseCandidateProvider::cache_dynamic_candidate(SchemeType scheme, const std::string &code,
                                                       const std::string &word, CandidateSource source)
{
    if (!IsJapaneseScheme(scheme) || code.empty() || word.empty() || source != CandidateSource::CloudSuggestion)
    {
        return -1;
    }
    auto items = dynamic_candidates_.get(code).value_or(std::vector<WordItem>{});
    items.erase(
        std::remove_if(items.begin(), items.end(), [source](const WordItem &item) { return item.source == source; }),
        items.end());
    items.emplace_back(code, word, 1, source, code);
    dynamic_candidates_.insert(code, items);
    return kNoMutation;
}

int JapaneseCandidateProvider::cache_dynamic_candidate_for_request(const QueryRequest &request, const std::string &word,
                                                                   CandidateSource source)
{
    return cache_dynamic_candidate(request.scheme, request.raw_input, word, source);
}

bool JapaneseCandidateProvider::ensure_query_statement()
{
    if (query_statement_)
        return true;
    if (!db_ &&
        sqlite3_open_v2(db_path_.c_str(), &db_, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK)
    {
        close_database();
        return false;
    }
    constexpr const char *sql = "SELECT code, value, weight FROM japanese_lexicon "
                                "WHERE code=?1 OR code=?2 OR code LIKE ?3 ESCAPE '#' OR code LIKE ?4 ESCAPE '#' "
                                "ORDER BY weight DESC, rowid ASC LIMIT 64";
    if (sqlite3_prepare_v2(db_, sql, -1, &query_statement_, nullptr) != SQLITE_OK)
    {
        close_database();
        return false;
    }
    return true;
}

void JapaneseCandidateProvider::close_database()
{
    if (query_statement_)
    {
        sqlite3_finalize(query_statement_);
        query_statement_ = nullptr;
    }
    if (db_)
    {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}
