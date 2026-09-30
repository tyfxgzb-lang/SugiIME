#include "input_session.h"

#include "../common/helpcode_utils.h"
#include "../contracts/assets/assets.h"
#include "../local_modes/date_time_query.h"
#include "../local_modes/emoji_query.h"
#include "../local_modes/kaomoji_query.h"
#include "../local_modes/quick_phrase_query.h"
#include "../local_modes/unicode_query.h"
#include "../user_dictionary/user_dictionary_journal.h"
#include "data_path.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <unordered_set>
#include <utility>

namespace metasequoia
{
namespace
{
const char *frequency_mode_name(FrequencyAdjustmentMode mode)
{
    switch (mode)
    {
    case FrequencyAdjustmentMode::Disabled:
        return "disabled";
    case FrequencyAdjustmentMode::Pin:
        return "pin";
    case FrequencyAdjustmentMode::Halve:
        return "halve";
    case FrequencyAdjustmentMode::Linear:
        return "linear";
    case FrequencyAdjustmentMode::Promote:
        return "promote";
    }
    return nullptr;
}

std::string online_identity(const QueryRequest &request)
{
    const std::string &input = request.raw_input_with_cases.empty() ? request.raw_input : request.raw_input_with_cases;
    return std::to_string(static_cast<int>(request.scheme)) + ":" + input;
}

} // namespace

InputSession::InputSession(SchemeType scheme_type, bool japanese_punctuation_enabled, bool candidate_learning_enabled,
                           RuntimePaths paths)
    : paths_(std::move(paths)), candidate_queries_(paths_), engine_(scheme_type, paths_),
      japanese_punctuation_enabled_(japanese_punctuation_enabled),
      candidate_learning_enabled_(candidate_learning_enabled)
{
}

KeyResult InputSession::handle_character(char character, bool shift_only)
{
    if (caret_position() < editing_text().size())
        return insert_at_caret(character);
    caret_.reset();
    if (dedicated_english_mode_)
    {
        const bool ascii_letter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
        if (!ascii_letter)
        {
            return {true, std::nullopt, std::nullopt};
        }
        dedicated_english_preedit_.push_back(character);
        update_dedicated_english_candidates();
        return {true, std::nullopt, std::nullopt};
    }
    if (local_input_mode_ != LocalInputMode::None)
    {
        return handle_local_character(character);
    }

    if (shift_only && character == 'U' && local_mode_options_.unicode && !has_composition() &&
        IsJapaneseScheme(scheme()))
    {
        local_input_mode_ = LocalInputMode::Unicode;
        local_preedit_ = "U";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'T' && local_mode_options_.date_time && !has_composition() &&
        IsJapaneseScheme(scheme()))
    {
        local_input_mode_ = LocalInputMode::DateTime;
        local_preedit_ = "T";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'K' && local_mode_options_.quick_phrase && !has_composition() &&
        IsJapaneseScheme(scheme()))
    {
        local_input_mode_ = LocalInputMode::QuickPhrase;
        local_preedit_ = "K";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'E' && local_mode_options_.emoji && !has_composition() && IsJapaneseScheme(scheme()))
    {
        local_input_mode_ = LocalInputMode::Emoji;
        local_preedit_ = "E";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'M' && local_mode_options_.kaomoji && !has_composition() &&
        IsJapaneseScheme(scheme()))
    {
        local_input_mode_ = LocalInputMode::Kaomoji;
        local_preedit_ = "M";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'Y' && local_mode_options_.temporary_english && !has_composition() &&
        IsJapaneseScheme(scheme()))
    {
        local_input_mode_ = LocalInputMode::TemporaryEnglish;
        local_preedit_ = "Y";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }

    const bool lowercase_letter = character >= 'a' && character <= 'z';
    if (!lowercase_letter && character != '\'')
    {
        return {};
    }
    if (character == '\'' && !has_composition())
    {
        return {};
    }

    const std::string previous_preedit = preedit();
    const auto unsigned_character = static_cast<unsigned char>(character);
    const ImeKeyCode key_code =
        character == '\'' ? ImeKey::Apostrophe : static_cast<ImeKeyCode>(std::toupper(unsigned_character));
    engine_.handle_key(key_code, 0, static_cast<ImeCharacter>(unsigned_character));
    update_mixed_candidates();
    const bool handled = preedit() != previous_preedit;
    if (handled)
    {
        online_requests_.invalidate();
    }
    return {handled, std::nullopt, std::nullopt};
}

KeyResult InputSession::handle_candidate_key(char character)
{
    if (!has_composition() || character < '1' || character > '9')
    {
        return {};
    }
    return select_candidate(static_cast<std::size_t>(character - '1'));
}

KeyResult InputSession::handle_punctuation(char character)
{
    if (!japanese_punctuation_enabled_)
    {
        return {};
    }

    const auto punctuation = punctuation_.translate(character);
    if (!punctuation)
        return {};

    KeyResult result = finish_composition();
    result.handled = true;
    std::string text = result.commit.value_or("");
    text += punctuation;
    result.commit = std::move(text);
    return result;
}

KeyResult InputSession::handle_command(Command command)
{
    if (!has_composition())
    {
        return {};
    }

    switch (command)
    {
    case Command::MoveLeft:
    case Command::MoveRight:
    case Command::MoveHome:
    case Command::MoveEnd:
    case Command::DeleteForward:
        return edit_at_caret(command);
    case Command::Backspace:
        if (caret_position() < editing_text().size())
            return edit_at_caret(command);
        caret_.reset();
        if (dedicated_english_mode_)
        {
            dedicated_english_preedit_.pop_back();
            update_dedicated_english_candidates();
            return {true, std::nullopt, std::nullopt};
        }
        if (local_input_mode_ != LocalInputMode::None)
        {
            std::optional<std::string> diagnostic;
            if (local_preedit_.size() <= 1)
            {
                reset_composition();
            }
            else if (local_input_mode_ == LocalInputMode::TemporaryJapanese)
            {
                engine_.handle_key(ImeKey::Backspace);
                local_preedit_ = "R" + engine_.get_preedit();
                local_candidates_ = engine_.get_candidates();
            }
            else
            {
                local_preedit_.pop_back();
                diagnostic = update_local_candidates();
            }
            return {true, std::nullopt, std::move(diagnostic)};
        }
        engine_.handle_key(ImeKey::Backspace);
        update_mixed_candidates();
        discard_abandoned_phrase_progress();
        online_requests_.invalidate();
        return {true, std::nullopt, std::nullopt};
    case Command::CommitCandidate:
        return commit(0);
    case Command::CommitRaw: {
        std::string raw = preedit();
        if ((local_input_mode_ == LocalInputMode::TemporaryEnglish ||
             local_input_mode_ == LocalInputMode::TemporaryJapanese) &&
            !raw.empty())
        {
            raw.erase(raw.begin());
        }
        std::optional<std::string> diagnostic;
        if (dedicated_english_mode_ &&
            !user_dictionary::learn_entered_english_word(path_to_utf8(paths_.dictionary(assets::english_dictionary)),
                                                         path_to_utf8(paths_.user(assets::user_journal)), raw))
        {
            diagnostic = "English word could not be learned.";
        }
        reset_composition();
        return {true, std::move(raw), std::move(diagnostic)};
    }
    case Command::Cancel:
        reset_composition();
        return {true, std::nullopt, std::nullopt};
    }
    return {};
}

KeyResult InputSession::select_candidate(std::size_t index)
{
    if (!has_composition() || index >= candidates().size())
    {
        return {};
    }
    return commit(index);
}

KeyResult InputSession::finish_composition(std::size_t first_index)
{
    KeyResult result;
    while (has_composition())
    {
        auto part = commit(first_index);
        first_index = 0;
        result.handled = true;
        if (part.commit)
        {
            if (!result.commit)
            {
                result.commit = std::string{};
            }
            *result.commit += *part.commit;
        }
        if (part.diagnostic)
        {
            result.diagnostic = std::move(part.diagnostic);
        }
    }
    return result;
}

KeyResult InputSession::select_candidate(const std::string &candidate)
{
    const auto found = std::find_if(candidates().begin(), candidates().end(),
                                    [&](const WordItem &item) { return item.word == candidate; });
    if (found == candidates().end())
    {
        return {};
    }
    return commit(static_cast<std::size_t>(std::distance(candidates().begin(), found)));
}

KeyResult InputSession::select_candidate_edge(std::size_t index, CandidateEdge edge)
{
    if (!has_composition() || index >= candidates().size())
    {
        return {};
    }

    const std::string &candidate = candidates()[index].word;
    std::string character = edge == CandidateEdge::FirstHan ? HelpcodeUtils::get_first_han_char(candidate)
                                                            : HelpcodeUtils::get_last_han_char(candidate);
    if (character.empty())
    {
        return {};
    }

    reset_composition();
    return {true, std::move(character), std::nullopt};
}

bool InputSession::set_frequency_adjustment(FrequencyAdjustmentOptions options)
{
    if (frequency_mode_name(options.mode) == nullptr || options.trigger_count < 1 || options.trigger_count > 10 ||
        options.linear_step < 1 || options.linear_step > 10)
    {
        return false;
    }
    frequency_adjustment_ = options;
    frequency_adjustment_configured_ = true;
    return true;
}

const FrequencyAdjustmentOptions &InputSession::frequency_adjustment() const
{
    return frequency_adjustment_;
}

void InputSession::set_local_mode_options(LocalModeOptions options)
{
    local_mode_options_ = options;
    if ((local_input_mode_ == LocalInputMode::Unicode && !local_mode_options_.unicode) ||
        (local_input_mode_ == LocalInputMode::DateTime && !local_mode_options_.date_time) ||
        (local_input_mode_ == LocalInputMode::QuickPhrase && !local_mode_options_.quick_phrase) ||
        (local_input_mode_ == LocalInputMode::Emoji && !local_mode_options_.emoji) ||
        (local_input_mode_ == LocalInputMode::Kaomoji && !local_mode_options_.kaomoji) ||
        (local_input_mode_ == LocalInputMode::SuperJianpin && !local_mode_options_.super_jianpin) ||
        (local_input_mode_ == LocalInputMode::TemporaryEnglish && !local_mode_options_.temporary_english) ||
        (local_input_mode_ == LocalInputMode::TemporaryJapanese && !local_mode_options_.temporary_japanese))
    {
        reset_composition();
    }
}

const LocalModeOptions &InputSession::local_mode_options() const
{
    return local_mode_options_;
}

bool InputSession::set_english_input_options(EnglishInputOptions options)
{
    if (options.minimum_prefix < 1 || options.minimum_prefix > 8)
    {
        return false;
    }
    english_input_options_ = options;
    update_mixed_candidates();
    return true;
}

const EnglishInputOptions &InputSession::english_input_options() const
{
    return english_input_options_;
}

void InputSession::set_mixed_expressive_options(MixedExpressiveOptions options)
{
    mixed_expressive_options_ = options;
    update_mixed_candidates();
}

const MixedExpressiveOptions &InputSession::mixed_expressive_options() const
{
    return mixed_expressive_options_;
}

void InputSession::set_dedicated_english_mode(bool enabled)
{
    if (dedicated_english_mode_ == enabled)
    {
        return;
    }
    reset_composition();
    dedicated_english_mode_ = enabled;
}

bool InputSession::dedicated_english_mode() const
{
    return dedicated_english_mode_;
}

LocalInputMode InputSession::local_input_mode() const
{
    return local_input_mode_;
}

void InputSession::set_local_date_time_provider(std::function<local_modes::LocalDateTime()> provider)
{
    local_date_time_provider_ = std::move(provider);
}

std::optional<OnlineQuery> InputSession::online_query() const
{
    if (dedicated_english_mode_ || local_input_mode_ != LocalInputMode::None || !has_composition())
    {
        return std::nullopt;
    }

    const QueryRequest &request = engine_.get_request();
    OnlineQuery query;
    query.scheme = request.scheme;
    online_requests_.stamp(query);
    query.identity = online_identity(request);

    const auto state = get_cloud_query_state();
    if (!state.should_query)
        return std::nullopt;
    query.query_text = state.query_text;
    query.cache_key = state.cache_key;
    if (IsJapaneseScheme(request.scheme))
    {
        query.cloud_eligible = true;
    }
    return query;
}

bool InputSession::apply_online_candidate(const OnlineQuery &query, std::string candidate, CandidateSource source)
{
    if (candidate.empty() || (source != CandidateSource::CloudSuggestion && source != CandidateSource::AiSuggestion))
    {
        return false;
    }
    const auto current = online_query();
    if (!current.has_value() || !online_requests_.matches(*current, query) ||
        (source == CandidateSource::CloudSuggestion && !current->cloud_eligible) ||
        (source == CandidateSource::AiSuggestion && !current->ai_eligible) ||
        std::any_of(candidates().begin(), candidates().end(),
                    [&](const WordItem &item) { return item.word == candidate; }))
    {
        return false;
    }
    if (engine_.apply_dynamic_candidate(candidate, source) != 0)
    {
        return false;
    }
    update_mixed_candidates();
    return std::any_of(candidates().begin(), candidates().end(),
                       [&](const WordItem &item) { return item.word == candidate && item.source == source; });
}

void InputSession::switch_scheme(SchemeType scheme_type)
{
    reset_composition();
    engine_.switch_scheme(scheme_type);
    update_mixed_candidates();
}

KeyResult InputSession::set_japanese_kana_form(JapaneseKanaForm form)
{
    if (!is_japanese() || !has_composition())
    {
        return {};
    }
    engine_.set_japanese_kana_form(form);
    update_mixed_candidates();
    return {true, std::nullopt, std::nullopt};
}

void InputSession::set_default_japanese_kana_form(JapaneseKanaForm form)
{
    engine_.set_default_japanese_kana_form(form);
}

SchemeType InputSession::scheme() const
{
    return engine_.current_scheme_type();
}

bool InputSession::has_composition() const
{
    if (dedicated_english_mode_)
    {
        return !dedicated_english_preedit_.empty();
    }
    return !preedit().empty();
}

const std::string &InputSession::preedit() const
{
    if (dedicated_english_mode_)
    {
        return dedicated_english_preedit_;
    }
    if (local_input_mode_ != LocalInputMode::None)
    {
        return local_preedit_;
    }
    return engine_.get_preedit();
}

const std::string &InputSession::raw_segmentation() const
{
    if (dedicated_english_mode_)
    {
        return dedicated_english_preedit_;
    }
    if (local_input_mode_ != LocalInputMode::None)
    {
        return local_preedit_;
    }
    return engine_.get_request().raw_segmentation;
}

const std::string &InputSession::normalized_segmentation() const
{
    if (dedicated_english_mode_)
    {
        return dedicated_english_preedit_;
    }
    if (local_input_mode_ != LocalInputMode::None)
    {
        return local_preedit_;
    }
    return engine_.get_request().normalized_segmentation;
}

const std::vector<WordItem> &InputSession::candidates() const
{
    if (dedicated_english_mode_)
    {
        return dedicated_english_candidates_;
    }
    if (local_input_mode_ != LocalInputMode::None)
    {
        return local_candidates_;
    }
    if (fixed_positions_enabled_ ||
        ((english_input_options_.mixed_candidates || mixed_expressive_options_.emoji_candidates ||
          mixed_expressive_options_.kaomoji_candidates) &&
         IsJapaneseScheme(scheme())))
    {
        // update_mixed_candidates() assembles this from the decoded prefix while it is active.
        return mixed_candidates_;
    }
    if (prefix_candidates_active_)
    {
        return prefix_candidates_;
    }
    return engine_.get_candidates();
}

SchemeType InputSession::scheme_type() const
{
    return scheme();
}

bool InputSession::japanese_punctuation_enabled() const
{
    return japanese_punctuation_enabled_;
}

bool InputSession::candidate_learning_enabled() const
{
    return candidate_learning_enabled_;
}

KeyResult InputSession::commit(std::size_t index)
{
    caret_.reset();
    std::optional<std::string> text;
    std::optional<WordItem> selected;
    if (index < candidates().size())
    {
        selected = candidates()[index];
        text = selected->word;
    }
    else if (!((local_input_mode_ == LocalInputMode::TemporaryEnglish ||
                local_input_mode_ == LocalInputMode::TemporaryJapanese) &&
               local_preedit_.size() == 1))
    {
        text = preedit();
    }
    std::optional<std::string> diagnostic = learn_candidate(index);
    reset_composition();
    return {true, std::move(text), std::move(diagnostic)};
}

KeyResult InputSession::handle_local_character(char character)
{
    if (local_input_mode_ == LocalInputMode::TemporaryEnglish)
    {
        const bool ascii_letter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
        if (!ascii_letter)
        {
            return {};
        }
        local_preedit_.push_back(character);
        return {true, std::nullopt, update_local_candidates()};
    }
    if (local_input_mode_ == LocalInputMode::TemporaryJapanese)
    {
        const auto unsigned_character = static_cast<unsigned char>(character);
        if (!std::isalpha(unsigned_character) && character != '\'')
        {
            return {};
        }
        const ImeKeyCode key_code =
            character == '\'' ? ImeKey::Apostrophe : static_cast<ImeKeyCode>(std::toupper(unsigned_character));
        engine_.handle_key(key_code, 0, static_cast<ImeCharacter>(unsigned_character));
        local_preedit_ = "R" + engine_.get_preedit();
        local_candidates_ = engine_.get_candidates();
        return {true, std::nullopt, std::nullopt};
    }
    if (local_input_mode_ == LocalInputMode::SuperJianpin)
    {
        const bool ascii_letter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
        if (!ascii_letter)
        {
            return {true, std::nullopt, std::nullopt};
        }
        local_preedit_.push_back(character);
        return {true, std::nullopt, update_local_candidates()};
    }
    if (local_input_mode_ == LocalInputMode::Emoji || local_input_mode_ == LocalInputMode::Kaomoji)
    {
        const bool ascii_letter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
        if (!ascii_letter && character != '\'')
        {
            return {true, std::nullopt, std::nullopt};
        }
        local_preedit_.push_back(character);
        return {true, std::nullopt, update_local_candidates()};
    }
    if (local_input_mode_ == LocalInputMode::QuickPhrase)
    {
        if (character < 'a' || character > 'z')
        {
            return {true, std::nullopt, std::nullopt};
        }
        local_preedit_.push_back(character);
        return {true, std::nullopt, update_local_candidates()};
    }
    if (local_input_mode_ == LocalInputMode::DateTime)
    {
        if (character < 'a' || character > 'z')
        {
            return {true, std::nullopt, std::nullopt};
        }
        local_preedit_.push_back(character);
        return {true, std::nullopt, update_local_candidates()};
    }
    if (local_input_mode_ != LocalInputMode::Unicode)
    {
        return {};
    }

    const auto unsigned_character = static_cast<unsigned char>(character);
    const bool optional_plus = character == '+' && local_preedit_ == "U";
    if (!optional_plus && std::isxdigit(unsigned_character) == 0)
    {
        return {true, std::nullopt, std::nullopt};
    }
    local_preedit_.push_back(character);
    return {true, std::nullopt, update_local_candidates()};
}

std::optional<std::string> InputSession::update_local_candidates()
{
    auto result = candidate_queries_.local(local_input_mode_, local_preedit_, scheme(), local_date_time_provider_,
                                           engine_.get_candidates());
    local_candidates_ = std::move(result.candidates);
    apply_candidate_positions(local_candidates_);
    return std::move(result.diagnostic);
}

void InputSession::update_mixed_candidates()
{
    refresh_prefix_candidates();
    const auto &decoded = prefix_candidates_active_ ? prefix_candidates_ : engine_.get_candidates();
    // Association (English/emoji suggestions) follows the string being converted: the caret
    // prefix while it is active, the full raw input otherwise.
    const std::string association_prefix =
        prefix_candidates_active_ ? prefix_query_input_ : engine_.get_request().raw_input;
    mixed_candidates_ = candidate_queries_.mixed(decoded, association_prefix, scheme(), english_input_options_,
                                                 mixed_expressive_options_, dedicated_english_mode_, local_input_mode_);
    apply_candidate_positions(mixed_candidates_);
}

void InputSession::set_caret(std::optional<std::size_t> caret)
{
    if (caret.has_value())
    {
        *caret = std::min(*caret, editing_text().size());
    }
    caret_ = caret;
}

std::size_t InputSession::quantized_prefix_end() const
{
    // Unset caret means the end of the string: full-string decoding, exactly the legacy path.
    const std::string &raw = get_pinyin_sequence_with_cases();
    if (!caret_.has_value())
    {
        return raw.size();
    }
    // Segmentation contract (engine spec #187): the caret prefix may only be quantized against
    // segment_raw_boundaries(), never against a re-derived segmentation.
    const std::vector<std::size_t> boundaries = segment_raw_boundaries();
    // No unit model (wubi, japanese, local modes, dedicated English): do not quantize.
    if (boundaries.empty())
    {
        return raw.size();
    }
    // Floor to the last complete unit at or before the caret: an incomplete trailing unit
    // belongs to the pending suffix. boundaries always contains 0, so the boundary before
    // upper_bound always exists.
    return *std::prev(std::upper_bound(boundaries.begin(), boundaries.end(), caret_position()));
}

std::size_t InputSession::prefix_end() const
{
    return quantized_prefix_end();
}

std::string InputSession::pending_suffix() const
{
    const std::string &raw = get_pinyin_sequence_with_cases();
    const std::size_t end = quantized_prefix_end();
    return end < raw.size() ? raw.substr(end) : std::string{};
}

void InputSession::refresh_prefix_candidates()
{
    prefix_candidates_active_ = false;
    if (!caret_.has_value() || dedicated_english_mode_ || local_input_mode_ != LocalInputMode::None)
    {
        prefix_candidates_.clear();
        prefix_query_input_.clear();
        return;
    }
    const std::string &raw_with_cases = get_pinyin_sequence_with_cases();
    const std::size_t end = quantized_prefix_end();
    if (end >= raw_with_cases.size())
    {
        // The caret sits on the final boundary or the scheme has no unit model: the full-string
        // engine decode already on hand is the answer.
        prefix_candidates_.clear();
        prefix_query_input_.clear();
        return;
    }
    std::string prefix;
    prefix.reserve(end);
    std::transform(raw_with_cases.begin(), raw_with_cases.begin() + static_cast<std::ptrdiff_t>(end),
                   std::back_inserter(prefix),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    if (prefix_query_input_ != prefix)
    {
        // The with-cases prefix keeps the display casing the scheme expects; the lowercased
        // copy doubles as the cache key (the query key itself is always lowercase).
        prefix_candidates_ = engine_.query_raw_candidates(prefix, raw_with_cases.substr(0, end));
        prefix_query_input_ = prefix;
    }
    // An empty prefix (caret at/before the first boundary) decodes to no candidates on purpose:
    // the host hides its candidate window while the preedit keeps showing the full pinyin.
    prefix_candidates_active_ = true;
}

void InputSession::update_dedicated_english_candidates()
{
    dedicated_english_candidates_.clear();
    if (dedicated_english_preedit_.empty())
    {
        return;
    }

    std::string prefix = dedicated_english_preedit_;
    std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    dedicated_english_candidates_ = candidate_queries_.english_dictionary().query_prefix(prefix, 1000);
    apply_candidate_positions(dedicated_english_candidates_);
    if (dedicated_english_candidates_.empty())
    {
        dedicated_english_candidates_.emplace_back("", dedicated_english_preedit_, 0, CandidateSource::Generated);
    }
}

void InputSession::reset_composition()
{
    caret_.reset();
    prefix_candidates_.clear();
    prefix_query_input_.clear();
    prefix_candidates_active_ = false;
    immediate_phrase_progress_ = {};
    clear_pending_sequence();
    online_requests_.invalidate();
    local_input_mode_ = LocalInputMode::None;
    local_preedit_.clear();
    local_candidates_.clear();
    dedicated_english_preedit_.clear();
    dedicated_english_candidates_.clear();
    mixed_candidates_.clear();
    engine_.reset();
}

// The phrase assembled across segmented selections only means anything while its composition stays alive. Every path
// that empties the preedit without going through reset_composition() - backspacing the remaining pinyin away, or a
// continuing selection whose leftover input collapses to nothing - abandons that phrase, so it has to be dropped here
// instead of being concatenated with the next composition's selections and stored as a phrase nobody typed.
void InputSession::discard_abandoned_phrase_progress()
{
    if (!has_composition())
    {
        immediate_phrase_progress_ = {};
    }
}

std::optional<std::string> InputSession::learn_candidate(std::size_t index)
{
    if (!candidate_learning_enabled_ || index >= candidates().size())
    {
        return std::nullopt;
    }
    const bool temporary_english = local_input_mode_ == LocalInputMode::TemporaryEnglish;
    if ((dedicated_english_mode_ || temporary_english) &&
        candidates()[index].source == CandidateSource::EnglishDictionary && frequency_adjustment_configured_ &&
        frequency_adjustment_.mode != FrequencyAdjustmentMode::Disabled && index != 0)
    {
        return adjust_candidate_frequency(index, frequency_adjustment_, false);
    }

    const WordItem &selected = candidates()[index];
    // 整句候选走造词，不走调频：它在词库里没有行可改。不受 frequency_adjustment_
    // 约束（那是候选词频策略），也不跳过 index == 0——首位整句同样没有落库，
    // 下次仍要靠猜，存下来才稳定。
    if (selected.source == CandidateSource::Generated || selected.source == CandidateSource::Fallback ||
        selected.source == CandidateSource::NeuralDesktop || selected.source == CandidateSource::NeuralKeyboard)
    {
        return learn_sentence_candidate(selected);
    }
    if (!frequency_adjustment_configured_)
    {
        if (selected.source == CandidateSource::Database || selected.source == CandidateSource::UserDatabase)
        {
            const std::string &pinyin = selected.canonical_pinyin.empty() ? selected.pinyin : selected.canonical_pinyin;
            (void)engine_.update_weight_by_pinyin_and_word(pinyin, selected.word);
        }
        return std::nullopt;
    }
    if (frequency_adjustment_.mode == FrequencyAdjustmentMode::Disabled || index == 0)
    {
        return std::nullopt;
    }
    if ((selected.source != CandidateSource::Database && selected.source != CandidateSource::UserDatabase))
    {
        return std::nullopt;
    }

    return adjust_candidate_frequency(index, frequency_adjustment_, false);
}

KeyResult InputSession::pin_candidate(std::size_t index)
{
    if (index >= candidates().size())
        return {};
    const auto source = candidates()[index].source;
    if (source != CandidateSource::EnglishDictionary &&
        (source != CandidateSource::Database && source != CandidateSource::UserDatabase))
        return {};

    // Manual pinning is independent of automatic learning preferences and never selects text.
    auto diagnostic = adjust_candidate_frequency(index, {FrequencyAdjustmentMode::Pin, 1, 1}, true);
    if (diagnostic)
        return {true, std::nullopt, std::move(diagnostic)};
    reset_cache();
    if (dedicated_english_mode_)
        update_dedicated_english_candidates();
    else if (local_input_mode_ != LocalInputMode::None)
        diagnostic = update_local_candidates();
    else
        recompute_candidates();
    return {true, std::nullopt, std::move(diagnostic)};
}

std::optional<std::string> InputSession::adjust_candidate_frequency(std::size_t index,
                                                                    FrequencyAdjustmentOptions options, bool force_top)
{
    const WordItem &selected = candidates()[index];
    if (selected.source == CandidateSource::EnglishDictionary)
    {
        std::string context = dedicated_english_mode_ ? dedicated_english_preedit_
                                                      : (local_input_mode_ == LocalInputMode::TemporaryEnglish
                                                             ? local_preedit_.substr(1)
                                                             : engine_.get_request().raw_input);
        std::transform(context.begin(), context.end(), context.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        std::vector<WordItem> ranked_candidates;
        std::copy_if(candidates().begin(), candidates().end(), std::back_inserter(ranked_candidates),
                     [](const WordItem &candidate) { return candidate.source == CandidateSource::EnglishDictionary; });
        const bool adjusted = user_dictionary::adjust_english_candidate_ranking(
            path_to_utf8(paths_.dictionary(assets::english_dictionary)),
            path_to_utf8(paths_.user(assets::user_journal)), "english:" + context, ranked_candidates, selected.pinyin,
            selected.word, frequency_mode_name(options.mode), options.linear_step, options.trigger_count, force_top);
        return adjusted ? std::nullopt
                        : std::optional<std::string>("English candidate frequency could not be persisted.");
    }
    std::string context_key = engine_.get_request().normalized_segmentation;
    if (context_key.empty())
    {
        context_key = engine_.get_request().segmentation;
    }
    const std::string entry_key = selected.canonical_pinyin.empty() ? context_key : selected.canonical_pinyin;
    bool ranking_changed = false;
    const auto kind = IsJapaneseScheme(scheme()) ? user_dictionary::DictionaryKind::Japanese
                                                 : user_dictionary::DictionaryKind::Pinyin;
    const bool adjusted = user_dictionary::adjust_candidate_ranking(
        path_to_utf8(paths_.dictionary(assets::main_dictionary)), path_to_utf8(paths_.user(assets::user_journal)),
        context_key, candidates(), entry_key, selected.word, frequency_mode_name(options.mode), options.linear_step,
        options.trigger_count, force_top, &ranking_changed, kind);
    if (!adjusted)
    {
        return std::string("Unable to persist candidate frequency adjustment.");
    }
    if (ranking_changed)
    {
        engine_.reset_cache();
    }
    return std::nullopt;
}

} // namespace metasequoia
