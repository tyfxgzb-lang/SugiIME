#include "ime_session.h"
#include "../schemes/japanese_romaji_scheme.h"
#include "../schemes/japanese_kana_scheme.h"
#include <stdexcept>

ImeSession::ImeSession(SchemeType scheme_type, metasequoia::RuntimePaths paths)
    : provider_registry_(std::move(paths)), scheme_(create_scheme(scheme_type))
{
}

void ImeSession::handle_key(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch)
{
    scheme_->handle_key(vk, modifiers_down, wch);
    refresh_candidates();
}

void ImeSession::switch_scheme(SchemeType scheme_type)
{
    scheme_ = create_scheme(scheme_type);
    state_ = CompositionState{};
    japanese_kana_form_ = default_japanese_kana_form_;
}

void ImeSession::reset()
{
    scheme_->reset();
    state_ = CompositionState{};
    japanese_kana_form_ = default_japanese_kana_form_;
}

SchemeType ImeSession::candidate_scheme() const
{
    return current_scheme_type();
}

void ImeSession::reset_cache()
{
    provider_registry_.reset_cache(candidate_scheme());
    refresh_candidates();
}

int ImeSession::create_word(std::string pinyin, std::string word)
{
    return provider_registry_.create_word(current_scheme_type(), std::move(pinyin), std::move(word));
}

int ImeSession::update_weight_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return provider_registry_.update_weight_by_pinyin_and_word(candidate_scheme(), std::move(pinyin), std::move(word));
}

int ImeSession::delete_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return provider_registry_.delete_by_pinyin_and_word(current_scheme_type(), std::move(pinyin), std::move(word));
}

int ImeSession::cache_dynamic_candidate(const std::string &pinyin, const std::string &word, CandidateSource source)
{
    return provider_registry_.cache_dynamic_candidate(current_scheme_type(), pinyin, word, source);
}

std::vector<WordItem> ImeSession::query_raw_candidates(const std::string &raw_input,
                                                       const std::string &raw_input_with_cases)
{
    const std::unique_ptr<IInputScheme> query_scheme = create_scheme(scheme_->type());
    query_scheme->set_raw_input(raw_input, raw_input_with_cases);
    QueryRequest request = query_scheme->build_request();
    apply_request_options(request);
    if (!request.valid)
    {
        return {};
    }
    return provider_registry_.resolve(request.scheme).query(request);
}

void ImeSession::replace_japanese_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases)
{
    if (scheme_->type() != SchemeType::JapaneseRomaji && scheme_->type() != SchemeType::JapaneseKana)
        return;
    scheme_->set_raw_input(raw_input, raw_input_with_cases);
    refresh_candidates();
}

void ImeSession::replace_active_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases)
{
    replace_japanese_raw_input(raw_input, raw_input_with_cases);
}

int ImeSession::cache_dynamic_candidate_for_current_request(const std::string &word, CandidateSource source)
{
    return provider_registry_.cache_dynamic_candidate_for_request(state_.request, word, source);
}

int ImeSession::apply_dynamic_candidate(const std::string &word, CandidateSource source)
{
    const int result = cache_dynamic_candidate_for_current_request(word, source);
    if (result == 0)
    {
        refresh_candidates();
    }
    return result;
}

SchemeType ImeSession::current_scheme_type() const
{
    return scheme_->type();
}

const std::string &ImeSession::get_preedit() const
{
    return state_.preedit;
}

const QueryRequest &ImeSession::get_request() const
{
    return state_.request;
}

const std::vector<WordItem> &ImeSession::get_candidates() const
{
    return state_.candidates;
}

std::optional<WordItem> ImeSession::find_candidate(const std::string &key, const std::string &value)
{
    return provider_registry_.find_candidate(candidate_scheme(), key, value);
}

bool ImeSession::expand_initial_candidates()
{
    return provider_registry_.expand_initial_candidates(state_.request, state_.candidates);
}

void ImeSession::apply_request_options(QueryRequest &request) const
{
    request.japanese_kana_form = japanese_kana_form_;
    request.japanese_fuzzy_mask = japanese_fuzzy_mask_;
}

void ImeSession::refresh_candidates()
{
    state_.preedit = scheme_->get_preedit();
    state_.request = scheme_->build_request();
    apply_request_options(state_.request);

    if (!state_.request.valid)
    {
        state_.candidates.clear();
        return;
    }

    state_.candidates = provider_registry_.resolve(state_.request.scheme).query(state_.request);
}

std::unique_ptr<IInputScheme> ImeSession::create_scheme(SchemeType scheme_type) const
{
    switch (scheme_type)
    {
    case SchemeType::JapaneseRomaji:
        return std::make_unique<JapaneseRomajiScheme>();
    case SchemeType::JapaneseKana:
        return std::make_unique<JapaneseKanaScheme>();
    default:
        throw std::runtime_error("Unknown scheme type.");
    }
}
