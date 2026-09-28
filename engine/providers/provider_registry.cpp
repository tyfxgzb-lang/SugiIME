#include "provider_registry.h"
#include <stdexcept>

ProviderRegistry::ProviderRegistry(metasequoia::RuntimePaths paths)
    : japanese_provider_(metasequoia::path_to_utf8(paths.dictionary(metasequoia::assets::main_dictionary)),
                         metasequoia::path_to_utf8(paths.resource(metasequoia::assets::japanese_model)))
{
}

ICandidateProvider &ProviderRegistry::resolve(SchemeType scheme_type)
{
    if (scheme_type == SchemeType::JapaneseRomaji || scheme_type == SchemeType::JapaneseKana)
        return japanese_provider_;
    throw std::runtime_error("Unknown scheme type.");
}

void ProviderRegistry::reset_cache(SchemeType scheme_type)
{
    if (scheme_type == SchemeType::JapaneseRomaji || scheme_type == SchemeType::JapaneseKana)
    {
        japanese_provider_.reset_cache();
        return;
    }
    throw std::runtime_error("Unknown scheme type.");
}

int ProviderRegistry::create_word(SchemeType scheme_type, std::string pinyin, std::string word)
{
    return resolve(scheme_type).create_word(scheme_type, std::move(pinyin), std::move(word));
}

int ProviderRegistry::update_weight_by_pinyin_and_word(SchemeType scheme_type, std::string pinyin, std::string word)
{
    return resolve(scheme_type).update_weight_by_pinyin_and_word(scheme_type, std::move(pinyin), std::move(word));
}

int ProviderRegistry::delete_by_pinyin_and_word(SchemeType scheme_type, std::string pinyin, std::string word)
{
    return resolve(scheme_type).delete_by_pinyin_and_word(scheme_type, std::move(pinyin), std::move(word));
}

int ProviderRegistry::cache_dynamic_candidate(SchemeType scheme_type, const std::string &pinyin,
                                              const std::string &word, CandidateSource source)
{
    return resolve(scheme_type).cache_dynamic_candidate(scheme_type, pinyin, word, source);
}

std::optional<WordItem> ProviderRegistry::find_candidate(SchemeType scheme_type, const std::string &key,
                                                         const std::string &value)
{
    return resolve(scheme_type).find_candidate(scheme_type, key, value);
}

bool ProviderRegistry::expand_initial_candidates(const QueryRequest &request, std::vector<WordItem> &candidates)
{
    return japanese_provider_.expand_initial_candidates(request, candidates);
}

int ProviderRegistry::cache_dynamic_candidate_for_request(const QueryRequest &request, const std::string &word,
                                                          CandidateSource source)
{
    return resolve(request.scheme).cache_dynamic_candidate_for_request(request, word, source);
}
