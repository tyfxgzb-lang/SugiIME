#include "input_session.h"
#include "../japanese/romaji_converter.h"
#include <algorithm>
#include <cctype>

namespace metasequoia
{
namespace
{
std::string remove_delimiters(const std::string &segmented)
{
    std::string normalized;
    normalized.reserve(segmented.size());
    for (const char ch : segmented)
    {
        if (ch != '\'')
        {
            normalized.push_back(ch);
        }
    }
    return normalized;
}
} // namespace

void InputSession::handle_engine_key(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch)
{
    engine_.handle_key(vk, modifiers_down, wch);
    online_requests_.invalidate();
    update_mixed_candidates();
}

void InputSession::recompute_candidates()
{
    if (has_pending_pinyin_sequence_ || has_pending_pinyin_sequence_with_cases_)
    {
        apply_pending_sequence();
        return;
    }
    engine_.handle_key(0, 0, 0);
    update_mixed_candidates();
}

SchemeType InputSession::current_scheme_type() const
{
    return engine_.current_scheme_type();
}

void InputSession::reset_state()
{
    clear_pending_sequence();
    reset_composition();
}

void InputSession::reset_cache()
{
    engine_.reset_cache();
    // 前缀候选是按文本缓存的，不跟着引擎缓存失效：只清缓存键，让下一次
    // refresh_prefix_candidates 按新权重/选项重查；保留当前列表，避免 caret
    // 激活期间出现空候选窗。
    prefix_query_input_.clear();
}

const std::vector<WordItem> &InputSession::get_candidates() const
{
    return candidates();
}

bool InputSession::expand_initial_candidates()
{
    return engine_.expand_initial_candidates();
}

std::optional<WordItem> InputSession::find_candidate(const std::string &key, const std::string &value)
{
    return engine_.find_candidate(key, value);
}

const QueryRequest &InputSession::request() const
{
    return engine_.get_request();
}

const std::string &InputSession::get_pinyin_sequence() const
{
    return request().raw_input;
}

const std::string &InputSession::get_pinyin_sequence_with_cases() const
{
    return request().raw_input_with_cases.empty() ? request().raw_input : request().raw_input_with_cases;
}

const std::string &InputSession::get_pure_pinyin_sequence() const
{
    return request().normalized_input;
}

const std::string &InputSession::get_pinyin_segmentation() const
{
    return request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
}

std::string InputSession::get_pinyin_segmentation_with_cases() const
{
    if (is_japanese())
    {
        // Japanese composition displays (and, on Enter, commits) the live kana
        // conversion, not the typed romaji. The romaji remains available through
        // raw_input for candidate queries and backspace editing.
        const auto converted = japanese::ConvertRomaji(request().raw_input);
        return converted.hiragana + converted.pending;
    }
    std::string preedit =
        request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
    if (!request().raw_input_with_cases.empty() && request().raw_input_with_cases.back() == '\'' &&
        (preedit.empty() || preedit.back() != '\''))
    {
        preedit.push_back('\'');
    }
    return preedit;
}

bool InputSession::is_all_complete_pure_pinyin() const
{
    if (is_japanese())
    {
        return japanese::ConvertRomaji(request().raw_input).complete;
    }
    const auto &segmentation =
        request().normalized_segmentation.empty() ? request().segmentation : request().normalized_segmentation;
    return !segmentation.empty();
}

bool InputSession::has_active_helpcode() const
{
    return false;
}

void InputSession::set_pinyin_sequence(const std::string &pinyin_sequence)
{
    pending_pinyin_sequence_ = pinyin_sequence;
    has_pending_pinyin_sequence_ = true;
}

void InputSession::set_pinyin_sequence_with_cases(const std::string &pinyin_sequence)
{
    pending_pinyin_sequence_with_cases_ = pinyin_sequence;
    has_pending_pinyin_sequence_with_cases_ = true;
}

int InputSession::store_user_phrase(std::string pinyin, std::string word)
{
    return engine_.create_word(std::move(pinyin), std::move(word));
}

int InputSession::store_user_phrase_from_canonical_pinyin(std::string pinyin, std::string word)
{
    return engine_.create_word(std::move(pinyin), std::move(word));
}

std::optional<std::string> InputSession::learn_sentence_candidate(const WordItem &selected)
{
    // Japanese sentence candidates are not persisted through the pinyin user-phrase path.
    (void)selected;
    return std::nullopt;
}

int InputSession::pin_candidate(std::string pinyin, std::string word)
{
    return engine_.update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int InputSession::remove_candidate(std::string pinyin, std::string word)
{
    if (remove_delimiters(request().raw_input).size() == 1)
    {
        return -1;
    }
    return engine_.delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int InputSession::cache_dynamic_candidate(const std::string &pinyin, const std::string &word, CandidateSource source)
{
    const int cache_result = engine_.cache_dynamic_candidate(pinyin, word, source);
    (void)engine_.cache_dynamic_candidate_for_current_request(word, source);
    return cache_result;
}

InputSession::SelectionTransition InputSession::advance_composition_after_selection(
    const std::string &selected_pinyin, const std::string &selected_word, const std::string &selected_canonical_pinyin)
{
    SelectionTransition transition;
    transition.selected_canonical_pinyin = selected_canonical_pinyin;
    (void)selected_pinyin;
    (void)selected_word;
    if (is_japanese())
    {
        transition.full_pure_pinyin = request().raw_input;
        transition.current_segmentation = request().segmentation;
        transition.current_segmentation_with_cases = request().raw_input_with_cases;
        return transition;
    }
    transition.full_pure_pinyin = request().normalized_input;
    transition.current_segmentation = get_pinyin_segmentation();
    transition.current_segmentation_with_cases = get_pinyin_segmentation_with_cases();
    return transition;
}

InputSession::CloudQueryState InputSession::get_cloud_query_state() const
{
    CloudQueryState state;
    if (is_japanese())
    {
        state.cache_key = request().raw_input;
        state.committed_pinyin = request().raw_input;
        state.should_query = !request().raw_input.empty();
        state.query_text = state.should_query ? request().raw_input : std::string{};
        return state;
    }
    state.cache_key = request().normalized_input;
    state.committed_pinyin = request().normalized_input;
    return state;
}

InputSession::CreatingWordProgress InputSession::update_creating_word_progress(
    const std::string &current_pinyin, const std::string &current_word, const std::string &selected_word,
    const SelectionTransition &selection_transition) const
{
    CreatingWordProgress progress;
    progress.pinyin = current_pinyin;
    progress.word = current_word + selected_word;
    progress.preedit = progress.word + selection_transition.current_segmentation_with_cases;
    progress.completed = !selection_transition.continues_composition;
    progress.can_store = false;
    return progress;
}

bool InputSession::is_japanese() const
{
    return IsJapaneseScheme(current_scheme_type());
}

void InputSession::clear_pending_sequence()
{
    pending_pinyin_sequence_.clear();
    pending_pinyin_sequence_with_cases_.clear();
    has_pending_pinyin_sequence_ = false;
    has_pending_pinyin_sequence_with_cases_ = false;
}

void InputSession::apply_pending_sequence()
{
    caret_.reset();
    const std::string raw_input = has_pending_pinyin_sequence_ ? pending_pinyin_sequence_ : request().raw_input;
    const std::string raw_input_with_cases =
        has_pending_pinyin_sequence_with_cases_ ? pending_pinyin_sequence_with_cases_ : raw_input;

    switch (current_scheme_type())
    {
    case SchemeType::JapaneseRomaji:
    case SchemeType::JapaneseKana:
        engine_.replace_japanese_raw_input(raw_input, raw_input_with_cases);
        break;
    default:
        break;
    }
    clear_pending_sequence();
    online_requests_.invalidate();
    update_mixed_candidates();
}
} // namespace metasequoia
