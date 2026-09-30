#include "Globals.h"
#include "Private.h"
#include "resource.h"
#include "define.h"
#include "MetasequoiaIMEBaseStructure.h"
#include <unordered_set>
#include <windows.h>
#include <fstream>
#include <string>
#include <ctime>
#include "FanyUtils.h"

namespace Global
{
HINSTANCE dllInstanceHandle;

LONG dllRefCount = -1;

CRITICAL_SECTION CS;
HFONT defaultlFontHandle; // Global font object we use everywhere

//---------------------------------------------------------------------
// MetasequoiaIME CLSID
//---------------------------------------------------------------------
// {144AB26B-E6E3-404F-8C27-C530C183805B}
extern const CLSID MetasequoiaIMECLSID = {0x144ab26b, 0xe6e3, 0x404f, {0x8c, 0x27, 0xc5, 0x30, 0xc1, 0x83, 0x80, 0x5b}};

//---------------------------------------------------------------------
// Profile GUID
//---------------------------------------------------------------------
// {D8D572FD-68A8-4AE1-8997-10C3EBBC7BF2}
extern const GUID MetasequoiaIMEGuidProfile = {
    0xd8d572fd, 0x68a8, 0x4ae1, {0x89, 0x97, 0x10, 0xc3, 0xeb, 0xbc, 0x7b, 0xf2}};

//---------------------------------------------------------------------
// PreserveKey GUID
//---------------------------------------------------------------------
// {FFA70EBB-04F7-4D16-B099-2413898A55E7}
extern const GUID MetasequoiaIMEGuidImeModePreserveKey = {
    0xffa70ebb, 0x04f7, 0x4d16, {0xb0, 0x99, 0x24, 0x13, 0x89, 0x8a, 0x55, 0xe7}};

// {57EA1D5C-9A00-42D8-98C6-39F3C52058CF}
extern const GUID MetasequoiaIMEGuidImeModePreserveKey02 = {
    0x57ea1d5c, 0x9a00, 0x42d8, {0x98, 0xc6, 0x39, 0xf3, 0xc5, 0x20, 0x58, 0xcf}};

// {4B2C7B7F-4A13-4DFC-AD09-885830BA65C3}
extern const GUID MetasequoiaIMEGuidImeModePreserveKey03 = {
    0x4b2c7b7f, 0x4a13, 0x4dfc, {0xad, 0x09, 0x88, 0x58, 0x30, 0xba, 0x65, 0xc3}};

// {358AEF17-FE42-4802-936F-5FE81FFE6A16}
extern const GUID MetasequoiaIMEGuidEnglishInputModePreserveKey = {
    0x358aef17, 0xfe42, 0x4802, {0x93, 0x6f, 0x5f, 0xe8, 0x1f, 0xfe, 0x6a, 0x16}};

// {92894B0F-D015-4F36-9DD9-E1ADB6FD3219}
extern const GUID MetasequoiaIMEGuidDoubleSingleBytePreserveKey = {
    0x92894b0f, 0xd015, 0x4f36, {0x9d, 0xd9, 0xe1, 0xad, 0xb6, 0xfd, 0x32, 0x19}};

// {8664F199-A778-4B71-9984-7225600CF42B}
extern const GUID MetasequoiaIMEGuidPunctuationPreserveKey = {
    0x8664f199, 0xa778, 0x4b71, {0x99, 0x84, 0x72, 0x25, 0x60, 0x0c, 0xf4, 0x2b}};

//---------------------------------------------------------------------
// Compartments
//---------------------------------------------------------------------
// {76B9B0C3-7E69-4BA2-B431-14861C881256}
extern const GUID MetasequoiaIMEGuidCompartmentDoubleSingleByte = {
    0x76b9b0c3, 0x7e69, 0x4ba2, {0xb4, 0x31, 0x14, 0x86, 0x1c, 0x88, 0x12, 0x56}};

// {FFD4AD91-2B6D-4584-B6A2-EF90B234AAA6}
extern const GUID MetasequoiaIMEGuidCompartmentPunctuation = {
    0xffd4ad91, 0x2b6d, 0x4584, {0xb6, 0xa2, 0xef, 0x90, 0xb2, 0x34, 0xaa, 0xa6}};

//---------------------------------------------------------------------
// LanguageBars
//---------------------------------------------------------------------

// {73D81F20-1D84-4EA9-A157-73FB070CA7E1}
extern const GUID MetasequoiaIMEGuidLangBarIMEMode = {
    0x73d81f20, 0x1d84, 0x4ea9, {0xa1, 0x57, 0x73, 0xfb, 0x07, 0x0c, 0xa7, 0xe1}};

// {F37C764A-7CB9-44F2-B4A9-E35F44A7F677}
extern const GUID MetasequoiaIMEGuidLangBarDoubleSingleByte = {
    0xf37c764a, 0x7cb9, 0x44f2, {0xb4, 0xa9, 0xe3, 0x5f, 0x44, 0xa7, 0xf6, 0x77}};

// {ED9421F7-4A9F-4964-9564-F332C6074453}
extern const GUID MetasequoiaIMEGuidLangBarPunctuation = {
    0xed9421f7, 0x4a9f, 0x4964, {0x95, 0x64, 0xf3, 0x32, 0xc6, 0x07, 0x44, 0x53}};

// {E8D4E3E1-0F5A-4D62-9DB5-176FC8CBF7C3}
extern const GUID MetasequoiaIMEGuidDisplayAttributeInput = {
    0xe8d4e3e1, 0x0f5a, 0x4d62, {0x9d, 0xb5, 0x17, 0x6f, 0xc8, 0xcb, 0xf7, 0xc3}};

// {6B33EF0E-EEC3-4786-BC0A-7F236821CF11}
extern const GUID MetasequoiaIMEGuidDisplayAttributeConverted = {
    0x6b33ef0e, 0xeec3, 0x4786, {0xbc, 0x0a, 0x7f, 0x23, 0x68, 0x21, 0xcf, 0x11}};

//---------------------------------------------------------------------
// UI element
//---------------------------------------------------------------------

// {56BAFE83-3C66-4281-B96A-D9740E634275}
extern const GUID MetasequoiaIMEGuidCandUIElement = {
    0x56bafe83, 0x3c66, 0x4281, {0xb9, 0x6a, 0xd9, 0x74, 0x0e, 0x63, 0x42, 0x75}};

//---------------------------------------------------------------------
// Unicode byte order mark
//---------------------------------------------------------------------
extern const WCHAR UnicodeByteOrderMark = 0xFEFF;

//---------------------------------------------------------------------
// dictionary table delimiter
//---------------------------------------------------------------------
extern const WCHAR KeywordDelimiter = L'=';
extern const WCHAR StringDelimiter = L'\"';

//---------------------------------------------------------------------
// defined item in setting file table [PreservedKey] section
//---------------------------------------------------------------------
extern const WCHAR ImeModeDescription[] = L"Chinese/English input (Shift)";
extern const WCHAR ImeModeDescription02[] = L"Chinese/English input (Ctrl+Alt+Space)";
extern const WCHAR ImeModeDescription03[] = L"Chinese/English input (Ctrl)";
extern const WCHAR EnglishInputModeDescription[] = L"English candidate input (Ctrl+Shift+E)";
extern const int ImeModeOnIcoIndex = IME_MODE_ON_ICON_INDEX;
extern const int ImeModeOffIcoIndex = IME_MODE_OFF_ICON_INDEX;

extern const WCHAR DoubleSingleByteDescription[] = L"Double/Single byte (Ctrl+Shift+Space)";
extern const int DoubleSingleByteOnIcoIndex = IME_DOUBLE_ON_INDEX;
extern const int DoubleSingleByteOffIcoIndex = IME_DOUBLE_OFF_INDEX;

extern const WCHAR PunctuationDescription[] = L"Chinese/English punctuation (Ctrl+.)";
extern const int PunctuationOnIcoIndex = IME_PUNCTUATION_ON_INDEX;
extern const int PunctuationOffIcoIndex = IME_PUNCTUATION_OFF_INDEX;

//---------------------------------------------------------------------
// defined item in setting file table [LanguageBar] section
//---------------------------------------------------------------------
extern const WCHAR LangbarImeModeDescription[] = L"Conversion mode";
extern const WCHAR LangbarDoubleSingleByteDescription[] = L"Character width";
extern const WCHAR LangbarPunctuationDescription[] = L"Punctuation";

//---------------------------------------------------------------------
// defined full width characters for Double/Single byte conversion
//---------------------------------------------------------------------
extern const WCHAR FullWidthCharTable[] = {
    0x3000, // Full width space
    0xFF01, // ！
    0xFF02, // ＂
    0xFF03, // ＃
    0xFF04, // ＄
    0xFF05, // ％
    0xFF06, // ＆
    0xFF07, // ＇
    0xFF08, // （
    0xFF09, // ）
    0xFF0A, // ＊
    0xFF0B, // ＋
    0xFF0C, // ，
    0xFF0D, // －
    0xFF0E, // ．
    0xFF0F, // ／

    0xFF10, // ０
    0xFF11, // １
    0xFF12, // ２
    0xFF13, // ３
    0xFF14, // ４
    0xFF15, // ５
    0xFF16, // ６
    0xFF17, // ７
    0xFF18, // ８
    0xFF19, // ９
    0xFF1A, // ：
    0xFF1B, // ；
    0xFF1C, // ＜
    0xFF1D, // ＝
    0xFF1E, // ＞
    0xFF1F, // ？

    0xFF20, // ＠
    0xFF21, // Ａ
    0xFF22, // Ｂ
    0xFF23, // Ｃ
    0xFF24, // Ｄ
    0xFF25, // Ｅ
    0xFF26, // Ｆ
    0xFF27, // Ｇ
    0xFF28, // Ｈ
    0xFF29, // Ｉ
    0xFF2A, // Ｊ
    0xFF2B, // Ｋ
    0xFF2C, // Ｌ
    0xFF2D, // Ｍ
    0xFF2E, // Ｎ
    0xFF2F, // Ｏ

    0xFF30, // Ｐ
    0xFF31, // Ｑ
    0xFF32, // Ｒ
    0xFF33, // Ｓ
    0xFF34, // Ｔ
    0xFF35, // Ｕ
    0xFF36, // Ｖ
    0xFF37, // Ｗ
    0xFF38, // Ｘ
    0xFF39, // Ｙ
    0xFF3A, // Ｚ
    0xFF3B, // ［
    0xFF3C, // ＼
    0xFF3D, // ］
    0xFF3E, // ＾
    0xFF3F, // ＿

    0xFF40, // ｀
    0xFF41, // ａ
    0xFF42, // ｂ
    0xFF43, // ｃ
    0xFF44, // ｄ
    0xFF45, // ｅ
    0xFF46, // ｆ
    0xFF47, // ｇ
    0xFF48, // ｈ
    0xFF49, // ｉ
    0xFF4A, // ｊ
    0xFF4B, // ｋ
    0xFF4C, // ｌ
    0xFF4D, // ｍ
    0xFF4E, // ｎ
    0xFF4F, // ｏ

    0xFF50, // ｐ
    0xFF51, // ｑ
    0xFF52, // ｒ
    0xFF53, // ｓ
    0xFF54, // ｔ
    0xFF55, // ｕ
    0xFF56, // ｖ
    0xFF57, // ｗ
    0xFF58, // ｘ
    0xFF59, // ｙ
    0xFF5A, // ｚ
    0xFF5B, // ｛
    0xFF5C, // ｜
    0xFF5D, // ｝
    0xFF5E  // ～
};

//---------------------------------------------------------------------
// defined punctuation characters
//---------------------------------------------------------------------
// '<' and '>' are deliberately absent: they belong to the nest pair set up in
// SetupPunctuationPair, and GetPunctuation searches this table first. Listing them here
// would shadow the nest pair, and the nested marks would never be produced.
extern const struct _PUNCTUATION PunctuationTable[23] = {
    {L'`', L"·"},   // ·
    {L'~', L"~"},   // ~
    {L'!', L"！"},  // ！
    {L'@', L"@"},   // @
    {L'#', L"#"},   // #
    {L'$', L"￥"},  // ￥
    {L'%', L"%"},   // %
    {L'^', L"……"},  // ……
    {L'&', L"&"},   // &
    {L'*', L"*"},   // *
    {L'(', L"（"},  // （
    {L')', L"）"},  // ）
    {L'_', L"——"},  // ——
    {L'[', L"【"},  // 【
    {L']', L"】"},  // 】
    {L'{', L"{"},   // {
    {L'}', L"}"},   // }
    {L'\\', L"、"}, // 、
    {L';', L"；"},  // ；
    {L':', L"："},  // ：
    {L',', L"，"},  // ，
    {L'.', L"。"},  // 。
    {L'?', L"？"},  // ？
};

// Japanese-mode punctuation. Covers the same input keys as PunctuationTable so
// the lookup loop is uniform; only the outputs that differ from Chinese are
// rewritten (comma → 、, brackets → 「」, backslash/backtick → ・).
extern const struct _PUNCTUATION JapanesePunctuationTable[23] = {
    {L'`', L"・"},  // ・
    {L'~', L"~"},   // ~
    {L'!', L"！"},  // ！
    {L'@', L"@"},   // @
    {L'#', L"#"},   // #
    {L'$', L"￥"},  // ￥
    {L'%', L"%"},   // %
    {L'^', L"……"},  // ……
    {L'&', L"&"},   // &
    {L'*', L"*"},   // *
    {L'(', L"（"},  // （
    {L')', L"）"},  // ）
    {L'_', L"——"},  // ——
    {L'[', L"「"},  // 「
    {L']', L"」"},  // 」
    {L'{', L"{"},   // {
    {L'}', L"}"},   // }
    {L'\\', L"・"}, // ・
    {L';', L"；"},  // ；
    {L':', L"："},  // ：
    {L',', L"、"},  // 、
    {L'.', L"。"},  // 。
    {L'?', L"？"},  // ？
};

//
// Will commit the highlighted candidate string with a punctuation character.
//
extern const std::unordered_set<WCHAR> CommitWithHighlightedCandPunc = {
    L'`',  //
    L'!',  //
    L'@',  //
    L'#',  //
    L'$',  //
    L'%',  //
    L'^',  //
    L'&',  //
    L'*',  //
    L'(',  //
    L')',  //
    L'-',  //
    L'_',  //
    L'=',  //
    L'+',  //
    L'[',  //
    L']',  //
    L'\\', //
    L'/',  //
    L';',  //
    L':',  //
    L'\'', //
    L'"',  //
    L',',  //
    L'<',  //
    L'.',  //
    L'>',  //
    L'?'   //
};

//+---------------------------------------------------------------------------
//
// CheckModifiers
//
//----------------------------------------------------------------------------

#define TF_MOD_ALLALT (TF_MOD_RALT | TF_MOD_LALT | TF_MOD_ALT)
#define TF_MOD_ALLCONTROL (TF_MOD_RCONTROL | TF_MOD_LCONTROL | TF_MOD_CONTROL)
#define TF_MOD_ALLSHIFT (TF_MOD_RSHIFT | TF_MOD_LSHIFT | TF_MOD_SHIFT)
#define TF_MOD_RLALT (TF_MOD_RALT | TF_MOD_LALT)
#define TF_MOD_RLCONTROL (TF_MOD_RCONTROL | TF_MOD_LCONTROL)
#define TF_MOD_RLSHIFT (TF_MOD_RSHIFT | TF_MOD_LSHIFT)

#define CheckMod(m0, m1, mod)                                                                                          \
    if (m1 & TF_MOD_##mod##)                                                                                           \
    {                                                                                                                  \
        if (!(m0 & TF_MOD_##mod##))                                                                                    \
        {                                                                                                              \
            return FALSE;                                                                                              \
        }                                                                                                              \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        if ((m1 ^ m0) & TF_MOD_RL##mod##)                                                                              \
        {                                                                                                              \
            return FALSE;                                                                                              \
        }                                                                                                              \
    }

BOOL CheckModifiers(UINT modCurrent, UINT mod)
{
    mod &= ~TF_MOD_ON_KEYUP;

    if (mod & TF_MOD_IGNORE_ALL_MODIFIER)
    {
        return TRUE;
    }

    if (modCurrent == mod)
    {
        return TRUE;
    }

    if (modCurrent && !mod)
    {
        return FALSE;
    }

    CheckMod(modCurrent, mod, ALT);
    CheckMod(modCurrent, mod, SHIFT);
    CheckMod(modCurrent, mod, CONTROL);

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// UpdateModifiers
//
//    wParam - virtual-key code
//    lParam - [0-15]  Repeat count
//  [16-23] Scan code
//  [24]    Extended key
//  [25-28] Reserved
//  [29]    Context code
//  [30]    Previous key state
//  [31]    Transition state
//----------------------------------------------------------------------------

thread_local USHORT ModifiersValue = 0;
thread_local BOOL IsShiftKeyDownOnly = FALSE;
thread_local BOOL IsControlKeyDownOnly = FALSE;
thread_local BOOL IsAltKeyDownOnly = FALSE;
thread_local BOOL PureShiftKeyDown = FALSE;
thread_local BOOL PureShiftKeyUp = FALSE;

BOOL UpdateModifiers(WPARAM wParam, LPARAM lParam)
{
    // high-order bit : key down
    // low-order bit  : toggled
    SHORT sksMenu = GetKeyState(VK_MENU);
    SHORT sksCtrl = GetKeyState(VK_CONTROL);
    SHORT sksShft = GetKeyState(VK_SHIFT);

    PureShiftKeyUp = FALSE;

    switch (wParam & 0xff)
    {
    case VK_MENU:
        // is VK_MENU down?
        if (sksMenu & 0x8000)
        {
            // is extended key?
            if (lParam & 0x01000000)
            {
                ModifiersValue |= (TF_MOD_RALT | TF_MOD_ALT);
            }
            else
            {
                ModifiersValue |= (TF_MOD_LALT | TF_MOD_ALT);
            }

            // is previous key state up?
            if (!(lParam & 0x40000000))
            {
                // is VK_CONTROL and VK_SHIFT up?
                if (!(sksCtrl & 0x8000) && !(sksShft & 0x8000))
                {
                    IsAltKeyDownOnly = TRUE;
                }
                else
                {
                    IsShiftKeyDownOnly = FALSE;
                    IsControlKeyDownOnly = FALSE;
                    IsAltKeyDownOnly = FALSE;
                }
            }
        }
        break;

    case VK_CONTROL:
        // is VK_CONTROL down?
        if (sksCtrl & 0x8000)
        {
            // is extended key?
            if (lParam & 0x01000000)
            {
                ModifiersValue |= (TF_MOD_RCONTROL | TF_MOD_CONTROL);
            }
            else
            {
                ModifiersValue |= (TF_MOD_LCONTROL | TF_MOD_CONTROL);
            }

            // is previous key state up?
            if (!(lParam & 0x40000000))
            {
                // is VK_SHIFT and VK_MENU up?
                if (!(sksShft & 0x8000) && !(sksMenu & 0x8000))
                {
                    IsControlKeyDownOnly = TRUE;
                }
                else
                {
                    IsShiftKeyDownOnly = FALSE;
                    IsControlKeyDownOnly = FALSE;
                    IsAltKeyDownOnly = FALSE;
                }
            }
        }
        break;

    case VK_SHIFT: {
        // is VK_SHIFT down?
        if (sksShft & 0x8000)
        {
            PureShiftKeyDown = TRUE;
            // is scan code 0x36(right shift)?
            if (((lParam >> 16) & 0x00ff) == 0x36)
            {
                ModifiersValue |= (TF_MOD_RSHIFT | TF_MOD_SHIFT);
            }
            else
            {
                ModifiersValue |= (TF_MOD_LSHIFT | TF_MOD_SHIFT);
            }

            // is previous key state up?
            if (!(lParam & 0x40000000))
            {
                // is VK_MENU and VK_CONTROL up?
                if (!(sksMenu & 0x8000) && !(sksCtrl & 0x8000))
                {
                    IsShiftKeyDownOnly = TRUE;
                }
                else
                {
                    IsShiftKeyDownOnly = FALSE;
                    IsControlKeyDownOnly = FALSE;
                    IsAltKeyDownOnly = FALSE;
                }
            }
        }
        else
        {
            if (PureShiftKeyDown)
                PureShiftKeyUp = TRUE;
        }
        break;
    }

    default:
        IsShiftKeyDownOnly = FALSE;
        IsControlKeyDownOnly = FALSE;
        IsAltKeyDownOnly = FALSE;
        PureShiftKeyDown = FALSE;
        break;
    }

    if (!(sksMenu & 0x8000))
    {
        ModifiersValue &= ~TF_MOD_ALLALT;
    }
    if (!(sksCtrl & 0x8000))
    {
        ModifiersValue &= ~TF_MOD_ALLCONTROL;
    }
    if (!(sksShft & 0x8000))
    {
        ModifiersValue &= ~TF_MOD_ALLSHIFT;
    }

    return TRUE;
}

//---------------------------------------------------------------------
// override CompareElements
//---------------------------------------------------------------------
BOOL CompareElements(LCID locale, const CStringRange *pElement1, const CStringRange *pElement2)
{
    return (CStringRange::Compare(locale, (CStringRange *)pElement1, (CStringRange *)pElement2) == CSTR_EQUAL) ? TRUE
                                                                                                               : FALSE;
}
} // namespace Global
