#pragma once

#include "config/ime_config.h"
#include <functional>
#include <string>
#include <vector>

// SugiIME AI assistant: calls a Chat Completions endpoint to generate
// contextual Japanese candidates.
class AiAssistant
{
  public:
    struct Request
    {
        std::vector<std::string> pinyin_segments;
        std::string context;
        std::string identity;
        AiAssistantConfig config;
    };

    using ApplyCallback =
        std::function<void(const std::string &candidate, const std::string &identity, uint64_t generation)>;

    static void Start(ApplyCallback apply_callback);
    static void Stop();
    static void OnInputChanged(Request request);
};
