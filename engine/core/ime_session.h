#pragma once

#include "composition_state.h"
#include "input_session_types.h"
#include "scheme_type.h"
#include "../providers/provider_registry.h"
#include "../schemes/input_scheme.h"
#include <memory>

class ImeSession
{
  public:
    explicit ImeSession(SchemeType scheme_type = SchemeType::JapaneseRomaji,
                        metasequoia::RuntimePaths paths = metasequoia::RuntimePaths::legacy());

    void handle_key(ImeKeyCode vk, ImeModifierMask modifiers_down = 0, ImeCharacter wch = 0);
    void switch_scheme(SchemeType scheme_type);
    // Japanese-only: pins the top candidate kana form for the current
    // composition. Reset to Auto on scheme switch / reset.
    void set_japanese_kana_form(JapaneseKanaForm form)
    {
        japanese_kana_form_ = form;
        refresh_candidates();
    }
    JapaneseKanaForm japanese_kana_form() const
    {
        return japanese_kana_form_;
    }
    void replace_japanese_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases);
    // Writes back to whichever scheme is composing.
    void replace_active_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases);
    // Runs one standalone candidate query for the given raw input without touching the live
    // composition: the active scheme's raw/key strokes and state_'s request/candidates stay put.
    std::vector<WordItem> query_raw_candidates(const std::string &raw_input, const std::string &raw_input_with_cases);
    void reset();
    void reset_cache();
    int create_word(std::string pinyin, std::string word);
    int update_weight_by_pinyin_and_word(std::string pinyin, std::string word);
    int delete_by_pinyin_and_word(std::string pinyin, std::string word);
    int cache_dynamic_candidate(const std::string &pinyin, const std::string &word, CandidateSource source);
    int cache_dynamic_candidate_for_current_request(const std::string &word, CandidateSource source);
    int apply_dynamic_candidate(const std::string &word, CandidateSource source);
    std::optional<WordItem> find_candidate(const std::string &key, const std::string &value);

    SchemeType current_scheme_type() const;
    const std::string &get_preedit() const;
    const QueryRequest &get_request() const;
    const std::vector<WordItem> &get_candidates() const;
    bool expand_initial_candidates();

  private:
    void apply_request_options(QueryRequest &request) const;
    void refresh_candidates();
    SchemeType candidate_scheme() const;
    std::unique_ptr<IInputScheme> create_scheme(SchemeType scheme_type) const;

  private:
    ProviderRegistry provider_registry_;
    std::unique_ptr<IInputScheme> scheme_;
    CompositionState state_;
    JapaneseKanaForm japanese_kana_form_ = JapaneseKanaForm::Auto;
};
