#pragma once

#include "japanese_candidate_provider.h"
#include "../core/scheme_type.h"
#include "../core/runtime_paths.h"
#include <string>

class ProviderRegistry
{
  public:
    explicit ProviderRegistry(metasequoia::RuntimePaths paths = metasequoia::RuntimePaths::legacy());
    ICandidateProvider &resolve(SchemeType scheme_type);
    std::optional<WordItem> find_candidate(SchemeType scheme_type, const std::string &key, const std::string &value);
    bool expand_initial_candidates(const QueryRequest &request, std::vector<WordItem> &candidates);
    void reset_cache(SchemeType scheme_type);
    int create_word(SchemeType scheme_type, std::string pinyin, std::string word);
    int update_weight_by_pinyin_and_word(SchemeType scheme_type, std::string pinyin, std::string word);
    int delete_by_pinyin_and_word(SchemeType scheme_type, std::string pinyin, std::string word);
    int cache_dynamic_candidate(SchemeType scheme_type, const std::string &pinyin, const std::string &word,
                                CandidateSource source);
    int cache_dynamic_candidate_for_request(const QueryRequest &request, const std::string &word,
                                            CandidateSource source);

  private:
    JapaneseCandidateProvider japanese_provider_;
};
