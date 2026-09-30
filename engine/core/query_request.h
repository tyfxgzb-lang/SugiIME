#pragma once

#include "key_event.h"
#include "scheme_type.h"
#include <cstdint>
#include <string>
#include <vector>

// Japanese fuzzy voicing (濁音・半濁音の曖昧入力) row bits: each bit enables
// correction candidates for one confused kana pair. All on preserves the
// historical always-on behaviour; the settings page stores the master switch
// plus one key per pair and folds them into this mask.
inline constexpr std::uint32_t kJapaneseFuzzyKaGa = 1u << 0; // か行 ↔ が行
inline constexpr std::uint32_t kJapaneseFuzzySaZa = 1u << 1; // さ行 ↔ ざ行
inline constexpr std::uint32_t kJapaneseFuzzyTaDa = 1u << 2; // た行 ↔ だ行
inline constexpr std::uint32_t kJapaneseFuzzyHaBa = 1u << 3; // は行 ↔ ば行
inline constexpr std::uint32_t kJapaneseFuzzyHaPa = 1u << 4; // は行 ↔ ぱ行
inline constexpr std::uint32_t kJapaneseFuzzyAll =
    kJapaneseFuzzyKaGa | kJapaneseFuzzySaZa | kJapaneseFuzzyTaDa | kJapaneseFuzzyHaBa | kJapaneseFuzzyHaPa;

// Japanese kana display form requested for the first candidate. Auto lets the
// provider decide (hiragana for regular words, katakana for loanword lemmas);
// the other values force the requested form as the top candidate. F6-F10 map
// to Hiragana / Katakana / HalfWidthKatakana / FullWidthRomaji /
// HalfWidthRomaji respectively, matching the Microsoft Japanese IME.
enum class JapaneseKanaForm
{
    Auto,
    Hiragana,
    Katakana,
    HalfWidthKatakana,
    FullWidthRomaji,
    HalfWidthRomaji,
};

struct KeyStroke
{
    ImeKeyCode vk = 0;
    ImeModifierMask modifiers_down = 0;
    ImeCharacter wch = 0;
};

struct QueryRequest
{
    SchemeType scheme = SchemeType::JapaneseRomaji;
    std::string raw_input;
    std::string raw_input_with_cases;
    std::string normalized_input;
    std::string raw_segmentation;
    std::string normalized_segmentation;
    std::string segmentation;
    // Japanese-only: forces the top candidate kana form. Default Auto keeps the
    // provider's word-class heuristic; F6-F10 override it per composition.
    JapaneseKanaForm japanese_kana_form = JapaneseKanaForm::Auto;
    // Japanese-only: enabled fuzzy-voicing correction pairs, see the
    // kJapaneseFuzzy* bits above. Zero disables fuzzy candidates entirely.
    std::uint32_t japanese_fuzzy_mask = kJapaneseFuzzyAll;
    std::vector<KeyStroke> key_strokes;
    bool valid = false;
};
