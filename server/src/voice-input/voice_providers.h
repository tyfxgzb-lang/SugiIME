#pragma once

#include <string>
#include <vector>

// Voice input was removed from SugiIME. This stub keeps the config and UI code
// compiling with no-op / identity implementations.
namespace VoiceInput
{
struct PolishPromptPreset
{
    std::string id;
    std::string name;
    std::string prompt;
};

inline const std::vector<PolishPromptPreset> &BuiltinPolishPromptPresets()
{
    static const std::vector<PolishPromptPreset> kEmpty;
    return kEmpty;
}

inline std::string NormalizeProviderId(const std::string &provider)
{
    return provider;
}

inline std::string NormalizeDoubaoAuthMode(const std::string &mode, const std::string &)
{
    return mode.empty() ? "api_key" : mode;
}

inline const std::vector<std::string> &AsrProviders()
{
    static const std::vector<std::string> kEmpty;
    return kEmpty;
}

inline const std::vector<std::string> &PolishProviders()
{
    static const std::vector<std::string> kEmpty;
    return kEmpty;
}

inline std::string UsableToken(const std::string &token)
{
    return token;
}

inline bool IsPlaceholderToken(const std::string &token)
{
    return token.empty();
}

inline std::string AsrTokenSlotKey(const std::string &id)
{
    return "asr_token_" + id;
}

inline std::string PolishTokenSlotKey(const std::string &id)
{
    return "polish_token_" + id;
}

inline std::string DefaultAsrModel(const std::string &)
{
    return {};
}

inline std::string ResolveAsrToken(const struct VoiceInputConfig &)
{
    return {};
}

inline std::string ResolvePolishToken(const struct VoiceInputConfig &)
{
    return {};
}

// Voice input service controls (no-ops).
inline void SetImeActive(bool)
{
}
inline void RefreshKeyboardHook()
{
}
inline void ToggleRecording()
{
}
} // namespace VoiceInput
