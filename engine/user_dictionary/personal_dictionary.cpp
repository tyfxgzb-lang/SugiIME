#include <metasequoia/personal_dictionary.h>
#include <utf8.h>
#include <algorithm>

namespace metasequoia
{
PersonalDictionaryValidation validate_personal_dictionary_entry(PersonalDictionaryEntry entry)
{
    const auto invalid = [](const char *message) { return PersonalDictionaryValidation{std::nullopt, message}; };
    if (entry.weight < 1 || entry.weight > 100000000)
        return invalid("Weight must be between 1 and 100000000");
    if (entry.key.empty() || entry.key.size() > 512 || entry.value.empty() || entry.value.size() > 4096 ||
        !utf8::is_valid(entry.value.begin(), entry.value.end()))
        return invalid("A valid, bounded UTF-8 word and input code are required");
    const bool quick = entry.kind == PersonalDictionaryKind::QuickPhrase;
    for (unsigned char ch : entry.value)
        if ((ch < 32 && !(quick && (ch == '\n' || ch == '\t'))) || ch == 127)
            return invalid("The word contains an unsupported control character");
    for (char &ch : entry.key)
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    auto letters = [](unsigned char ch) { return ch >= 'a' && ch <= 'z'; };
    switch (entry.kind)
    {
    case PersonalDictionaryKind::Pinyin: {
        // Japanese romaji (and legacy pinyin) keys are lowercase letters and apostrophes.
        std::replace(entry.key.begin(), entry.key.end(), ' ', '\'');
        if (!std::all_of(entry.key.begin(), entry.key.end(),
                         [&](unsigned char ch) { return letters(ch) || ch == '\''; }))
            return invalid("Use letters separated by apostrophes or spaces");
        break;
    }
    case PersonalDictionaryKind::Wubi:
        if (entry.key.size() > 4 || !std::all_of(entry.key.begin(), entry.key.end(), letters))
            return invalid("Wubi codes contain one to four letters");
        break;
    case PersonalDictionaryKind::QuickPhrase:
        if (entry.key.size() > 32 || !std::all_of(entry.key.begin(), entry.key.end(), [&](unsigned char ch) {
                return letters(ch) || (ch >= '0' && ch <= '9');
            }))
            return invalid("Quick-phrase codes contain one to 32 letters or digits");
        break;
    case PersonalDictionaryKind::English: {
        std::string normalized = entry.value;
        for (char &ch : normalized)
            if (ch >= 'A' && ch <= 'Z')
                ch = static_cast<char>(ch - 'A' + 'a');
        if (entry.key.size() > 64 || !std::all_of(entry.key.begin(), entry.key.end(), letters) ||
            normalized != entry.key)
            return invalid("English code must match the word's letters, ignoring case");
        break;
    }
    default:
        return invalid("Unsupported dictionary kind");
    }
    return {std::move(entry), {}};
}
} // namespace metasequoia
