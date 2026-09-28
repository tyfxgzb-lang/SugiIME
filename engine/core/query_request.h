#pragma once

#include "key_event.h"
#include "scheme_type.h"
#include <string>
#include <vector>

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
    std::vector<KeyStroke> key_strokes;
    bool valid = false;
};
