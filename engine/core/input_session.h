#pragma once

#include "ime_session.h"
#include "input_session_types.h"
#include "candidate_queries.h"
#include "punctuation_policy.h"
#include "online_request_guard.h"
#include "../local_modes/date_time_query.h"
#include "../english/english_dictionary.h"
#include "word_item.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace metasequoia
{
// Platform-neutral composition session shared by the native frontends. It owns an ImeSession and
// applies the key-handling and commit policy that each frontend would otherwise reimplement, so a
// frontend only has to translate platform key events into these calls.
class InputSession
{
  public:
    explicit InputSession(SchemeType scheme_type = SchemeType::JapaneseRomaji,
                          bool japanese_punctuation_enabled = true, bool candidate_learning_enabled = true,
                          RuntimePaths paths = RuntimePaths::legacy());

    KeyResult handle_character(char character, bool shift_only = false);
    KeyResult handle_command(Command command);
    KeyResult handle_candidate_key(char character);
    KeyResult handle_punctuation(char character);
    KeyResult select_candidate(std::size_t index);
    KeyResult finish_composition(std::size_t first_index = 0);
    KeyResult select_candidate(const std::string &candidate);
    KeyResult select_candidate_edge(std::size_t index, CandidateEdge edge);
    KeyResult pin_candidate(std::size_t index);
    KeyResult remove_candidate(std::size_t index);
    KeyResult set_candidate_position(std::size_t index, int position);
    void enable_fixed_positions();
    bool set_frequency_adjustment(FrequencyAdjustmentOptions options);
    const FrequencyAdjustmentOptions &frequency_adjustment() const;
    void set_local_mode_options(LocalModeOptions options);
    const LocalModeOptions &local_mode_options() const;
    bool set_english_input_options(EnglishInputOptions options);
    const EnglishInputOptions &english_input_options() const;
    void set_mixed_expressive_options(MixedExpressiveOptions options);
    const MixedExpressiveOptions &mixed_expressive_options() const;
    void set_dedicated_english_mode(bool enabled);
    bool dedicated_english_mode() const;
    LocalInputMode local_input_mode() const;
    void set_local_date_time_provider(std::function<local_modes::LocalDateTime()> provider);
    std::optional<OnlineQuery> online_query() const;
    bool apply_online_candidate(const OnlineQuery &query, std::string candidate, CandidateSource source);

    SchemeType scheme_type() const;
    bool japanese_punctuation_enabled() const;
    bool candidate_learning_enabled() const;
    void switch_scheme(SchemeType scheme_type);
    KeyResult set_japanese_kana_form(JapaneseKanaForm form);
    SchemeType scheme() const;

    bool has_composition() const;
    const std::string &preedit() const;
    std::string editing_text() const;
    std::size_t caret_position() const;
    const std::string &raw_segmentation() const;
    const std::string &normalized_segmentation() const;
    const std::vector<WordItem> &candidates() const;

    struct SelectionTransition
    {
        bool continues_composition = false;
        std::string full_pure_pinyin;
        std::string current_segmentation;
        std::string current_segmentation_with_cases;
        std::string selected_canonical_pinyin;
        std::string consumed_raw_input_with_cases;
    };

    struct CloudQueryState
    {
        bool should_query = false;
        std::string query_text;
        std::string cache_key;
        std::string committed_pinyin;
    };

    struct CreatingWordProgress
    {
        std::string pinyin;
        std::string word;
        std::string preedit;
        bool completed = false;
        bool can_store = false;
    };

    void handle_engine_key(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch);
    void recompute_candidates();
    void set_caret(std::optional<std::size_t> caret);
    std::size_t prefix_end() const;
    std::string pending_suffix() const;
    SchemeType current_scheme_type() const;

    void reset_state();
    void reset_cache();

    const std::vector<WordItem> &get_candidates() const;
    bool expand_initial_candidates();
    std::optional<WordItem> find_candidate(const std::string &key, const std::string &value);

    const std::string &get_pinyin_sequence() const;
    const std::string &get_pinyin_sequence_with_cases() const;
    const std::string &get_pure_pinyin_sequence() const;
    const std::string &get_pinyin_segmentation() const;
    std::string get_pinyin_segmentation_with_cases() const;
    std::vector<std::size_t> segment_raw_boundaries() const;
    bool has_active_helpcode() const
    {
        return false;
    }

    void set_pinyin_sequence(const std::string &pinyin_sequence);
    void set_pinyin_sequence_with_cases(const std::string &pinyin_sequence);

    int store_user_phrase(std::string pinyin, std::string word);
    int store_user_phrase_from_canonical_pinyin(std::string pinyin, std::string word);
    int pin_candidate(std::string pinyin, std::string word);
    int remove_candidate(std::string pinyin, std::string word);
    int cache_dynamic_candidate(const std::string &pinyin, const std::string &word, CandidateSource source);
    SelectionTransition advance_composition_after_selection(const std::string &selected_pinyin,
                                                            const std::string &selected_word,
                                                            const std::string &selected_canonical_pinyin);
    CloudQueryState get_cloud_query_state() const;
    CreatingWordProgress update_creating_word_progress(const std::string &current_pinyin,
                                                       const std::string &current_word,
                                                       const std::string &selected_word,
                                                       const SelectionTransition &selection_transition) const;

    void set_japanese_punctuation_enabled(bool enabled)
    {
        japanese_punctuation_enabled_ = enabled;
    }
    void set_candidate_learning_enabled(bool enabled)
    {
        candidate_learning_enabled_ = enabled;
    }

  private:
    const QueryRequest &request() const;
    bool is_japanese() const;
    void clear_pending_sequence();
    void apply_pending_sequence();

    KeyResult commit(std::size_t index);
    KeyResult handle_local_character(char character);
    KeyResult insert_at_caret(char character);
    KeyResult edit_at_caret(Command command);
    KeyResult replace_editing_text(std::string text, std::size_t caret);
    std::optional<std::size_t> caret_;
    std::size_t quantized_prefix_end() const;
    void refresh_prefix_candidates();
    std::vector<WordItem> prefix_candidates_;
    std::string prefix_query_input_;
    bool prefix_candidates_active_ = false;
    std::optional<std::string> update_local_candidates();
    void update_mixed_candidates();
    void apply_candidate_positions(std::vector<WordItem> &items);
    std::string position_context(bool english) const;
    bool fixed_positions_enabled_ = false;
    void update_dedicated_english_candidates();
    void reset_composition();
    void discard_abandoned_phrase_progress();
    std::optional<std::string> learn_candidate(std::size_t index);
    std::optional<std::string> learn_sentence_candidate(const WordItem &selected);
    std::optional<std::string> adjust_candidate_frequency(std::size_t index, FrequencyAdjustmentOptions options,
                                                          bool force_top);

    CreatingWordProgress immediate_phrase_progress_;
    std::string pending_pinyin_sequence_;
    std::string pending_pinyin_sequence_with_cases_;
    bool has_pending_pinyin_sequence_ = false;
    bool has_pending_pinyin_sequence_with_cases_ = false;

    RuntimePaths paths_;
    CandidateQueries candidate_queries_;
    ImeSession engine_;
    bool japanese_punctuation_enabled_ = true;
    bool candidate_learning_enabled_ = true;
    PunctuationPolicy punctuation_;
    FrequencyAdjustmentOptions frequency_adjustment_;
    bool frequency_adjustment_configured_ = false;
    LocalModeOptions local_mode_options_;
    EnglishInputOptions english_input_options_;
    MixedExpressiveOptions mixed_expressive_options_;
    bool dedicated_english_mode_ = false;
    std::string dedicated_english_preedit_;
    std::vector<WordItem> dedicated_english_candidates_;
    std::vector<WordItem> mixed_candidates_;
    LocalInputMode local_input_mode_ = LocalInputMode::None;
    std::string local_preedit_;
    std::vector<WordItem> local_candidates_;
    std::function<local_modes::LocalDateTime()> local_date_time_provider_;
    OnlineRequestGuard online_requests_;
};
} // namespace metasequoia
