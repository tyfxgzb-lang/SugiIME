#pragma once

#include <string>
#include <vector>

// Voice input was removed from SugiIME. This stub keeps the settings snapshot
// payload compiling with an empty preset list.
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
} // namespace VoiceInput
