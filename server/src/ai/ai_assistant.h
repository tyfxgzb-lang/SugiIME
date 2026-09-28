#pragma once

#include "config/ime_config.h"
#include <string>
#include <vector>

// SugiIME removes the Chinese AI assistant. Stub kept for source compatibility.
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

    template <typename Callback>
    static void Start(Callback)
    {
    }

    static void Stop()
    {
    }

    static void OnInputChanged(Request)
    {
    }
};
