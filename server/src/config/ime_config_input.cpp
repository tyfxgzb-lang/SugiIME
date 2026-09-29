// 输入方案与输入行为配置：方案、中日文模式、简繁、中英切换键、双拼与五笔方案、上屏样式、辅助码、全拼纠错、模糊音和调频。
#include "config/ime_config_internal.h"
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>
#include "global/globals.h"
#include "defines/defines.h"
#include "engine/common/helpcode_utils.h"
#include "engine/core/data_path.h"

using namespace ime_config_detail;

SchemeType GetConfiguredInputScheme()
{
    return g_input_scheme;
}

SchemeType GetConfiguredActiveInputScheme()
{
    if (g_input_mode == "japanese")
        return g_japanese_schema == "kana" ? SchemeType::JapaneseKana : SchemeType::JapaneseRomaji;
    return g_input_scheme;
}

std::string GetConfiguredInputSchemeName()
{
    switch (g_input_scheme)
    {
    case SchemeType::JapaneseKana:
        return "japanese_kana";
    default:
        return "japanese_romaji";
    }
}

bool SetConfiguredInputScheme(const std::string &scheme)
{
    if (scheme != "japanese_romaji" && scheme != "japanese_kana")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "schema", EscapeTomlBasicString(scheme)))
    {
        return false;
    }
    g_input_scheme = ParseScheme(scheme);
    NotifyImeServerInputSchemeChanged();
    return true;
}

const std::string &GetConfiguredCharacterSet()
{
    return g_character_set;
}

bool SetConfiguredCharacterSet(const std::string &character_set)
{
    if (character_set != "simplified" && character_set != "traditional")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "character_set", EscapeTomlBasicString(character_set)))
    {
        return false;
    }
    g_character_set = character_set;
    // Marshal WebView/native toolbar refreshes to their owner thread. Settings
    // in a separate process reaches the same refresh through ConfigChanged.
    NotifyImeServer(WM_REFRESH_CHARACTER_SET, L"ConfigChanged");
    return true;
}

bool GetConfiguredCharacterSetShortcutEnabled()
{
    return g_character_set_shortcut_enabled;
}

bool SetConfiguredCharacterSetShortcutEnabled(bool enabled)
{
    if (!WriteConfiguredValue("keybindings", "toggle_character_set_ctrl_shift_f", enabled ? "true" : "false"))
        return false;
    g_character_set_shortcut_enabled = enabled;
    return true;
}

const std::string &GetConfiguredDefaultImeMode()
{
    return g_default_ime_mode;
}

bool SetConfiguredDefaultImeMode(const std::string &mode)
{
    if (mode != "chinese" && mode != "english")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "default_ime_mode", EscapeTomlBasicString(mode)))
    {
        return false;
    }
    g_default_ime_mode = mode;
    return true;
}

const std::string &GetConfiguredImeModeScope()
{
    return g_ime_mode_scope;
}

bool SetConfiguredImeModeScope(const std::string &scope)
{
    if (scope != "app" && scope != "global")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "ime_mode_scope", EscapeTomlBasicString(scope)))
    {
        return false;
    }
    g_ime_mode_scope = scope;
    return true;
}

bool IsConfiguredImeModeScopeGlobal()
{
    return g_ime_mode_scope == "global";
}

bool GetConfiguredSwitchLanguageShiftEnabled()
{
    return g_switch_language_shift_enabled;
}

bool SetConfiguredSwitchLanguageShiftEnabled(bool enabled)
{
    if (!WriteConfiguredValue("keybindings", "switch_language_shift", enabled ? "true" : "false"))
    {
        return false;
    }
    g_switch_language_shift_enabled = enabled;
    return true;
}

bool GetConfiguredSwitchLanguageCtrlEnabled()
{
    return g_switch_language_ctrl_enabled;
}

bool SetConfiguredSwitchLanguageCtrlEnabled(bool enabled)
{
    if (!WriteConfiguredValue("keybindings", "switch_language_ctrl", enabled ? "true" : "false"))
    {
        return false;
    }
    g_switch_language_ctrl_enabled = enabled;
    return true;
}

bool GetConfiguredSwitchLanguageCtrlAltSpaceEnabled()
{
    return g_switch_language_ctrl_alt_space_enabled;
}

bool SetConfiguredSwitchLanguageCtrlAltSpaceEnabled(bool enabled)
{
    if (!WriteConfiguredValue("keybindings", "switch_language_ctrl_alt_space", enabled ? "true" : "false"))
    {
        return false;
    }
    g_switch_language_ctrl_alt_space_enabled = enabled;
    return true;
}

const std::string &GetConfiguredShuangpinSchema()
{
    return g_shuangpin_schema;
}

bool SetConfiguredShuangpinSchema(const std::string &schema)
{
    if (schema != "xiaohe" && schema != "ziranma" && schema != "shoudao" && schema != "microsoft")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "shuangpin_schema", EscapeTomlBasicString(schema)))
    {
        return false;
    }
    g_shuangpin_schema = schema;
    return true;
}

const std::string &GetConfiguredWubiSchema()
{
    return g_wubi_schema;
}

bool SetConfiguredWubiSchema(const std::string &schema)
{
    if (schema != "wubi86")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "wubi_schema", EscapeTomlBasicString(schema)))
    {
        return false;
    }
    g_wubi_schema = schema;
    return true;
}

const std::string &GetConfiguredShuangpinPreeditMode()
{
    return g_shuangpin_preedit_mode;
}

const std::string &GetConfiguredTsfPreeditStyle()
{
    return g_tsf_preedit_style;
}

bool SetConfiguredTsfPreeditStyle(const std::string &style)
{
    if (!GlobalSettings::isKnownTsfPreeditStyle(style))
    {
        return false;
    }
    if (!WriteConfiguredValue("appearance", "tsf_preedit_style", EscapeTomlBasicString(style)))
    {
        return false;
    }
    g_tsf_preedit_style = style;
    GlobalSettings::setTsfPreeditStyle(style);
    return true;
}

bool GetConfiguredShuangpinHelpcodeEnabled()
{
    return g_shuangpin_helpcode_enabled;
}

bool SetConfiguredShuangpinHelpcodeEnabled(bool enabled)
{
    if (!WriteConfiguredValue("helpcode", "shuangpin_helpcode", enabled ? "true" : "false"))
    {
        return false;
    }
    g_shuangpin_helpcode_enabled = enabled;
    return true;
}

const std::string &GetConfiguredShuangpinHelpcodeSchema()
{
    return g_shuangpin_helpcode_schema;
}

bool SetConfiguredShuangpinHelpcodeSchema(const std::string &schema)
{
    if (!IsHelpcodeSchemaAvailable(schema))
        return false;
    if (!WriteConfiguredValue("helpcode", "shuangpin_helpcode_schema", EscapeTomlBasicString(schema)))
        return false;
    g_shuangpin_helpcode_schema = schema;
    return true;
}

bool GetConfiguredQuanpinHelpcodeEnabled()
{
    return g_quanpin_helpcode_enabled;
}

bool SetConfiguredQuanpinHelpcodeEnabled(bool enabled)
{
    if (!WriteConfiguredValue("helpcode", "quanpin_helpcode", enabled ? "true" : "false"))
    {
        return false;
    }
    g_quanpin_helpcode_enabled = enabled;
    return true;
}

bool GetConfiguredQuanpinAutocorrectTransposition()
{
    return g_quanpin_autocorrect_transposition;
}

bool SetConfiguredQuanpinAutocorrectTransposition(bool enabled)
{
    if (!WriteConfiguredValue("quanpin", "autocorrect_transposition", enabled ? "true" : "false"))
    {
        return false;
    }
    g_quanpin_autocorrect_transposition = enabled;
    return true;
}

bool GetConfiguredQuanpinAutocorrectNeighbor()
{
    return g_quanpin_autocorrect_neighbor;
}

bool SetConfiguredQuanpinAutocorrectNeighbor(bool enabled)
{
    if (!WriteConfiguredValue("quanpin", "autocorrect_neighbor", enabled ? "true" : "false"))
    {
        return false;
    }
    g_quanpin_autocorrect_neighbor = enabled;
    return true;
}

bool GetConfiguredFuzzyPinyinEnabled()
{
    return g_fuzzy_pinyin_enabled;
}

bool SetConfiguredFuzzyPinyinEnabled(bool enabled)
{
    // SugiIME removed the fuzzy-pinyin engine; only the master-switch key is still
    // persisted so old configs round-trip. No rule seeding or bitmask updates.
    if (!WriteConfiguredValue("input", "fuzzy_pinyin", enabled ? "true" : "false"))
        return false;
    g_fuzzy_pinyin_enabled = enabled;
    return true;
}

bool SetConfiguredFuzzyPinyinRule(const std::string &key, bool enabled)
{
    (void)key;
    (void)enabled;
    // Individual fuzzy rules no longer exist without the Chinese engine.
    return false;
}

const std::string &GetConfiguredQuanpinHelpcodeSchema()
{
    return g_quanpin_helpcode_schema;
}

bool SetConfiguredQuanpinHelpcodeSchema(const std::string &schema)
{
    if (!IsHelpcodeSchemaAvailable(schema))
        return false;
    if (!WriteConfiguredValue("helpcode", "quanpin_helpcode_schema", EscapeTomlBasicString(schema)))
        return false;
    g_quanpin_helpcode_schema = schema;
    return true;
}

std::vector<CustomHelpcodeSchemaInfo> GetCustomHelpcodeSchemas()
{
    std::vector<CustomHelpcodeSchemaInfo> result;
    for (const auto &schema : HelpcodeUtils::list_custom_helpcode_schemas(metasequoia::data_directory()))
    {
        const std::string &name = schema.name.empty() ? schema.name_en : schema.name;
        const std::string &name_en = schema.name_en.empty() ? schema.name : schema.name_en;
        result.push_back(
            {schema.schema, name.empty() ? schema.file_stem : name, name_en.empty() ? schema.file_stem : name_en});
    }
    return result;
}

std::string GetCustomHelpcodeDirectory()
{
    return metasequoia::path_to_utf8(HelpcodeUtils::custom_helpcode_directory(metasequoia::data_directory()));
}

bool GetConfiguredShowShuangpinHelpcodeInCandidateWindow()
{
    return g_show_shuangpin_helpcode_in_candidate_window;
}

bool SetConfiguredShowShuangpinHelpcodeInCandidateWindow(bool enabled)
{
    if (!WriteConfiguredValue("helpcode", "show_sp_helpcode_in_candidate_window", enabled ? "true" : "false"))
    {
        return false;
    }
    g_show_shuangpin_helpcode_in_candidate_window = enabled;
    return true;
}

bool GetConfiguredShowQuanpinHelpcodeInCandidateWindow()
{
    return g_show_quanpin_helpcode_in_candidate_window;
}

bool SetConfiguredShowQuanpinHelpcodeInCandidateWindow(bool enabled)
{
    if (!WriteConfiguredValue("helpcode", "show_qp_helpcode_in_candidate_window", enabled ? "true" : "false"))
    {
        return false;
    }
    g_show_quanpin_helpcode_in_candidate_window = enabled;
    return true;
}

const std::string &GetConfiguredInputMode()
{
    return g_input_mode;
}

bool SetConfiguredInputMode(const std::string &mode)
{
    if (mode != "chinese" && mode != "japanese")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "mode", EscapeTomlBasicString(mode)))
    {
        return false;
    }
    g_input_mode = mode;
    NotifyImeServerInputSchemeChanged();
    return true;
}

const std::string &GetConfiguredJapaneseSchema()
{
    return g_japanese_schema;
}

bool SetConfiguredJapaneseSchema(const std::string &schema)
{
    if (schema != "romaji" && schema != "kana")
    {
        return false;
    }
    if (!WriteConfiguredValue("input", "japanese_schema", EscapeTomlBasicString(schema)))
    {
        return false;
    }
    g_japanese_schema = schema;
    // Rebuild the input session (romaji vs JIS kana are distinct schemes/backends)
    // and broadcast InputModeChanged so the TSF DLL re-reads the kana-layout flag.
    NotifyImeServerInputSchemeChanged();
    return true;
}

bool GetConfiguredJapanesePunctuation()
{
    return g_japanese_punctuation;
}

bool SetConfiguredJapanesePunctuation(bool enabled)
{
    if (!WriteConfiguredValue("input", "japanese_punctuation", enabled ? "true" : "false"))
    {
        return false;
    }
    g_japanese_punctuation = enabled;
    return true;
}

bool GetConfiguredJapaneseKatakanaFkey()
{
    return g_japanese_katakana_fkey;
}

bool SetConfiguredJapaneseKatakanaFkey(bool enabled)
{
    if (!WriteConfiguredValue("input", "japanese_katakana_fkey", enabled ? "true" : "false"))
    {
        return false;
    }
    g_japanese_katakana_fkey = enabled;
    return true;
}

const FrequencyAdjustmentConfig &GetConfiguredFrequencyAdjustment()
{
    return g_frequency_adjustment;
}

bool SetConfiguredFrequencyAdjustmentString(const std::string &key, const std::string &value)
{
    if (key != "mode" ||
        (value != "disabled" && value != "pin" && value != "halve" && value != "linear" && value != "promote") ||
        !WriteConfiguredValue("frequency_adjustment", key, EscapeTomlBasicString(value)))
        return false;
    g_frequency_adjustment.mode = value;
    return true;
}

bool SetConfiguredFrequencyAdjustmentInt(const std::string &key, int value)
{
    if ((key != "trigger_count" && key != "linear_step") || value < 1 || value > 10 ||
        !WriteConfiguredValue("frequency_adjustment", key, std::to_string(value)))
        return false;
    if (key == "trigger_count")
        g_frequency_adjustment.trigger_count = value;
    else
        g_frequency_adjustment.linear_step = value;
    return true;
}
