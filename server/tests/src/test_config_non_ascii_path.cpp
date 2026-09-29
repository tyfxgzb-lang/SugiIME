#include "tests/includes/test_framework.h"

#include "config/ime_config.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace
{
class ScopedEnv
{
  public:
    ScopedEnv(const wchar_t *name, const std::wstring &value) : name_(name)
    {
        wchar_t buffer[32768];
        const DWORD length = GetEnvironmentVariableW(name, buffer, 32768);
        had_previous_ = length != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
        previous_.assign(buffer, length);
        SetEnvironmentVariableW(name, value.c_str());
    }
    ~ScopedEnv()
    {
        SetEnvironmentVariableW(name_.c_str(), had_previous_ ? previous_.c_str() : nullptr);
    }

    ScopedEnv(const ScopedEnv &) = delete;
    ScopedEnv &operator=(const ScopedEnv &) = delete;

  private:
    std::wstring name_;
    std::wstring previous_;
    bool had_previous_ = false;
};

// LOCALAPPDATA alone no longer decides where the config lives: an installed product records its
// data directory in HKLM, which outranks the profile. Pin the config directory explicitly so these
// cases stay isolated on a machine that has the IME installed.
class ScopedConfigLocation
{
  public:
    explicit ScopedConfigLocation(const std::filesystem::path &local_app_data)
        : local_app_data_(L"LOCALAPPDATA", local_app_data.wstring()),
          config_dir_(L"METASEQUOIA_IME_CONFIG_DIR", (local_app_data / L"metasequoiaime").wstring())
    {
    }

  private:
    ScopedEnv local_app_data_;
    ScopedEnv config_dir_;
};

std::filesystem::path MakeProfileRoot()
{
    return std::filesystem::temp_directory_path() / (L"msime-配置测试-" + std::to_wstring(GetCurrentProcessId()));
}

void SeedTemplate(const std::filesystem::path &data_dir)
{
    std::error_code ec;
    std::filesystem::create_directories(data_dir, ec);
    REQUIRE(!ec);
    std::filesystem::copy_file(MSIME_DEFAULT_CONFIG_PATH, data_dir / L"config.default.toml",
                               std::filesystem::copy_options::overwrite_existing, ec);
    REQUIRE(!ec);
}

void WriteText(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(static_cast<bool>(output));
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string ReadText(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE(config_round_trips_under_non_ascii_profile_path)
{
    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot();
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    SeedTemplate(data_dir);

    {
        ScopedConfigLocation local_app_data_env(local_app_data);

        InitImeConfig();
        REQUIRE(fs::exists(data_dir / L"config.toml"));

        REQUIRE(SetConfiguredInputMode("japanese"));
        REQUIRE(SetConfiguredInputScheme("wubi"));
        for (const std::string &position : {"top-left", "top", "top-right", "bottom-left", "bottom", "bottom-right"})
        {
            REQUIRE(SetConfiguredCaretStateIndicatorPosition(position));
            InitImeConfig();
            REQUIRE_EQ(GetConfiguredCaretStateIndicatorPosition(), position);
        }
        for (const bool onFocus : {true, false})
        {
            REQUIRE(SetConfiguredCaretStateIndicatorOnFocus(onFocus));
            InitImeConfig();
            REQUIRE_EQ(GetConfiguredCaretStateIndicatorOnFocus(), onFocus);
        }
        // An unknown value is rejected and leaves the stored position alone.
        REQUIRE(!SetConfiguredCaretStateIndicatorPosition("left"));
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredCaretStateIndicatorPosition(), std::string("bottom-right"));

        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
        REQUIRE_EQ(GetConfiguredInputSchemeName(), std::string("wubi"));

        const std::vector<std::string> fonts = {"SimSun", "Font#1", "Font]2", "Font\\\"3", "微软雅黑"};
        REQUIRE(SetConfiguredCandidateFallbackFonts(fonts));
        InitImeConfig();
        REQUIRE(GetConfiguredCandidateFallbackFonts() == fonts);
        auto reordered = fonts;
        std::reverse(reordered.begin(), reordered.end());
        REQUIRE(SetConfiguredCandidateFallbackFonts(reordered));
        InitImeConfig();
        REQUIRE(GetConfiguredCandidateFallbackFonts() == reordered);
        REQUIRE(!SetConfiguredCandidateFallbackFonts({"bad\nfont"}));
        REQUIRE(GetConfiguredCandidateFallbackFonts() == reordered);
        REQUIRE(SetConfiguredCandidateFallbackFonts({}));
        InitImeConfig();
        REQUIRE(GetConfiguredCandidateFallbackFonts().empty());
    }

    fs::remove_all(unique_root, ec);
}

TEST_CASE(config_recovers_unparseable_file_and_saves)
{
    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot() / L"损坏";
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    SeedTemplate(data_dir);
    WriteText(data_dir / L"config.toml", "this is not toml {{{");
    WriteText(data_dir / L"config.base.toml", ReadText(data_dir / L"config.default.toml"));

    {
        ScopedConfigLocation local_app_data_env(local_app_data);
        InitImeConfig();
        REQUIRE(SetConfiguredInputMode("japanese"));
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
    }

    fs::remove_all(unique_root, ec);
}

TEST_CASE(config_overwrites_readonly_file)
{
    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot() / L"只读";
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    SeedTemplate(data_dir);

    {
        ScopedConfigLocation local_app_data_env(local_app_data);
        InitImeConfig();
        const fs::path config_path = data_dir / L"config.toml";
        REQUIRE(SetFileAttributesW(config_path.c_str(), FILE_ATTRIBUTE_READONLY));
        REQUIRE(SetConfiguredInputMode("japanese"));
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
    }

    fs::remove_all(unique_root, ec);
}

TEST_CASE(config_migrates_legacy_acp_mangled_path)
{
    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot() / L"遗留";
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    SeedTemplate(data_dir);

    fs::path mangled_dir;
    try
    {
        mangled_dir = fs::path(data_dir.u8string());
    }
    catch (...)
    {
        mangled_dir.clear();
    }
    if (mangled_dir.empty() || mangled_dir == data_dir)
    {
        fs::remove_all(unique_root, ec);
        return;
    }

    fs::create_directories(mangled_dir, ec);
    REQUIRE(!ec);
    const std::string stock = ReadText(data_dir / L"config.default.toml");
    const std::string from = "mode = \"chinese\"";
    const auto pos = stock.find(from);
    REQUIRE(pos != std::string::npos);
    WriteText(data_dir / L"config.toml", stock + "\n# leftover-installer-marker\n");
    std::string leftover = stock;
    leftover.replace(pos, from.size(), "mode = \"japanese\"");
    WriteText(mangled_dir / L"config.toml", leftover);

    {
        ScopedConfigLocation local_app_data_env(local_app_data);
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
        REQUIRE(SetConfiguredInputScheme("wubi"));
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
        REQUIRE_EQ(GetConfiguredInputSchemeName(), std::string("wubi"));
    }

    fs::remove_all(unique_root, ec);
}

// 全拼纠错的两个开关必须能在全新安装上落盘（出厂模板没有 [quanpin] 段，首次写入要能创建它），
// 重启（重新 InitImeConfig）后保持；旧的单一 autocorrect 键已废弃，即使配置文件里还留着它、
// 甚至只有它，纠错也必须保持默认关闭（R2：废弃不迁移）。
TEST_CASE(quanpin_autocorrect_keys_persist_and_legacy_key_stays_ignored)
{
    namespace fs = std::filesystem;
    const fs::path unique_root =
        fs::temp_directory_path() / (L"msime-纠错配置测试-" + std::to_wstring(GetCurrentProcessId()));
    const fs::path local_app_data = unique_root / L"profile";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    fs::create_directories(data_dir, ec);
    REQUIRE(!ec);
    // 出厂模板没有 [quanpin] 段：这正是全新安装后第一次开开关的真实起点。
    fs::copy_file(MSIME_DEFAULT_CONFIG_PATH, data_dir / L"config.default.toml", fs::copy_options::overwrite_existing,
                  ec);
    REQUIRE(!ec);

    {
        ScopedConfigLocation local_app_data_env(local_app_data);

        InitImeConfig();
        REQUIRE(fs::exists(data_dir / L"config.toml"));

        // 缺失段上的首次写入不能失败；重读磁盘后两个开关独立保持。
        REQUIRE(SetConfiguredQuanpinAutocorrectTransposition(true));
        REQUIRE(SetConfiguredQuanpinAutocorrectNeighbor(false));
        InitImeConfig();
        REQUIRE(GetConfiguredQuanpinAutocorrectTransposition());
        REQUIRE(!GetConfiguredQuanpinAutocorrectNeighbor());

        // 旧键（哪怕显式 true）不得再影响纠错状态：清掉新键、只留旧键后重读，两者都必须默认关。
        auto config_text = std::string("[quanpin]\nautocorrect = true\n");
        {
            std::ofstream config(data_dir / L"config.toml", std::ios::binary | std::ios::trunc);
            REQUIRE(static_cast<bool>(config));
            config.write(config_text.data(), static_cast<std::streamsize>(config_text.size()));
        }
        InitImeConfig();
        REQUIRE(!GetConfiguredQuanpinAutocorrectTransposition());
        REQUIRE(!GetConfiguredQuanpinAutocorrectNeighbor());
    }

    fs::remove_all(unique_root, ec);
}

// Fuzzy-pinyin rule/master-switch tests were removed with the Chinese engine.
// 智能标点的五个键：默认值三形态（无文件 / 模板缺键 / 出厂模板）、逐键往返、重读保持。
// 默认组合是「全部关闭」：整个智能标点家族需要用户显式开启，升级用户不改配置保持零回归。
TEST_CASE(smart_punctuation_conversion_keys_default_and_round_trip)
{
    namespace fs = std::filesystem;
    const fs::path unique_root =
        fs::temp_directory_path() / (L"msime-智能标点测试-" + std::to_wstring(GetCurrentProcessId()));
    const fs::path local_app_data = unique_root / L"profile";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);

    // 无文件形态：配置目录里既没有 config.toml 也没有模板，InitImeConfig 读不到文件，
    // 全局量保持静态默认——五个键全部为关，不依赖任何模板内容。
    {
        ScopedConfigLocation local_app_data_env(local_app_data);
        InitImeConfig();
        REQUIRE(!GetConfiguredSmartPunctuationEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationSpaceConvertEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectDigitEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectLetterEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationRepeatToChineseEnabled());
    }

    SeedTemplate(data_dir);

    {
        ScopedConfigLocation local_app_data_env(local_app_data);

        // 出厂模板形态：五个键都从模板读出，缺键时 value_or 与模板一致（全关）。
        InitImeConfig();
        REQUIRE(!GetConfiguredSmartPunctuationEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationSpaceConvertEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectDigitEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectLetterEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationRepeatToChineseEnabled());

        // 模板缺键：手写一份没有任何 smart_punctuation_* 键的配置，重读回落到默认值（全关）。
        WriteText(data_dir / L"config.toml", "[input]\nschema = \"quanpin\"\n");
        InitImeConfig();
        REQUIRE(!GetConfiguredSmartPunctuationEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationSpaceConvertEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectDigitEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectLetterEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationRepeatToChineseEnabled());

        // 逐键往返：只翻目标键，另外两个直出键与相邻键不被动。
        REQUIRE(SetConfiguredSmartPunctuationSpaceConvertEnabled(true));
        REQUIRE(GetConfiguredSmartPunctuationSpaceConvertEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectDigitEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectLetterEnabled());
        REQUIRE(SetConfiguredSmartPunctuationDirectDigitEnabled(true));
        REQUIRE(GetConfiguredSmartPunctuationDirectDigitEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectLetterEnabled());
        REQUIRE(SetConfiguredSmartPunctuationDirectLetterEnabled(true));
        REQUIRE(GetConfiguredSmartPunctuationDirectLetterEnabled());
        REQUIRE(GetConfiguredSmartPunctuationSpaceConvertEnabled());

        // 重读保持：三个键都真实落盘且跨 InitImeConfig 存活。
        InitImeConfig();
        REQUIRE(GetConfiguredSmartPunctuationSpaceConvertEnabled());
        REQUIRE(GetConfiguredSmartPunctuationDirectDigitEnabled());
        REQUIRE(GetConfiguredSmartPunctuationDirectLetterEnabled());
        {
            const std::string text = ReadText(data_dir / L"config.toml");
            REQUIRE(text.find("smart_punctuation_space_convert = true") != std::string::npos);
            REQUIRE(text.find("smart_punctuation_direct_digit = true") != std::string::npos);
            REQUIRE(text.find("smart_punctuation_direct_letter = true") != std::string::npos);
        }

        // 显式切换回默认：逐键可逆，总开关与撤销键不动（默认全关）。
        REQUIRE(SetConfiguredSmartPunctuationSpaceConvertEnabled(false));
        REQUIRE(SetConfiguredSmartPunctuationDirectDigitEnabled(false));
        REQUIRE(SetConfiguredSmartPunctuationDirectLetterEnabled(false));
        REQUIRE(!GetConfiguredSmartPunctuationSpaceConvertEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectDigitEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationDirectLetterEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationEnabled());
        REQUIRE(!GetConfiguredSmartPunctuationRepeatToChineseEnabled());
    }

    fs::remove_all(unique_root, ec);
}

// 出厂模板（安装包真正分发的那份）必须携带五个键：模板缺键会在升级合并时被静默丢弃，
// 用户改过的值下次升级消失且无任何报错。默认全部关闭，整个智能标点家族需要显式开启。
TEST_CASE(shipped_template_carries_smart_punctuation_conversion_keys)
{
    std::ifstream input(MSIME_DEFAULT_CONFIG_PATH, std::ios::binary);
    REQUIRE(static_cast<bool>(input));
    const std::string installed((std::istreambuf_iterator<char>(input)), {});
    const auto parsed = toml::parse(installed);
    REQUIRE(!parsed["input"]["smart_punctuation"].value_or(true));
    REQUIRE(!parsed["input"]["smart_punctuation_space_convert"].value_or(true));
    REQUIRE(!parsed["input"]["smart_punctuation_direct_digit"].value_or(true));
    REQUIRE(!parsed["input"]["smart_punctuation_direct_letter"].value_or(true));
    REQUIRE(!parsed["input"]["smart_punctuation_repeat_to_chinese"].value_or(true));
}

// 统计总开关：默认关闭（无文件 / 出厂模板 / 模板缺段三形态），能落盘并在重读后保持。
// 出厂模板若不给 [statistics] 段，首次打开开关也必须成功——setter 不能假设段存在，
// 否则设置页会报「保存失败」（先例：全拼纠错的 [quanpin] 段）。
TEST_CASE(statistics_enabled_defaults_off_and_round_trips)
{
    namespace fs = std::filesystem;
    const fs::path unique_root =
        fs::temp_directory_path() / (L"msime-统计开关测试-" + std::to_wstring(GetCurrentProcessId()));
    const fs::path local_app_data = unique_root / L"profile";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);

    // 无文件形态：配置目录里什么都没有，全局量保持静态默认 false。
    {
        ScopedConfigLocation local_app_data_env(local_app_data);
        InitImeConfig();
        REQUIRE(!GetConfiguredStatisticsEnabled());
    }

    SeedTemplate(data_dir);

    {
        ScopedConfigLocation local_app_data_env(local_app_data);

        // 出厂模板形态：键存在且为 false。
        InitImeConfig();
        REQUIRE(!GetConfiguredStatisticsEnabled());

        // 缺段写入：手写一份没有 [statistics] 的配置，首次打开开关必须自己把段补出来。
        WriteText(data_dir / L"config.toml", "[input]\nschema = \"quanpin\"\n");
        REQUIRE(SetConfiguredStatisticsEnabled(true));
        REQUIRE(GetConfiguredStatisticsEnabled());
        {
            const std::string text = ReadText(data_dir / L"config.toml");
            REQUIRE(text.find("[statistics]") != std::string::npos);
            REQUIRE(text.find("enabled = true") != std::string::npos);
        }

        // 缺键回落：抹掉段后重读，模板缺键时默认必须是 false。
        WriteText(data_dir / L"config.toml", "[input]\nschema = \"quanpin\"\n");
        InitImeConfig();
        REQUIRE(!GetConfiguredStatisticsEnabled());

        // 重读保持：开关真实落盘、跨 InitImeConfig 存活，关闭同样可逆。
        REQUIRE(SetConfiguredStatisticsEnabled(true));
        InitImeConfig();
        REQUIRE(GetConfiguredStatisticsEnabled());
        REQUIRE(SetConfiguredStatisticsEnabled(false));
        REQUIRE(!GetConfiguredStatisticsEnabled());
        InitImeConfig();
        REQUIRE(!GetConfiguredStatisticsEnabled());
        {
            const std::string text = ReadText(data_dir / L"config.toml");
            REQUIRE(text.find("enabled = false") != std::string::npos);
        }
    }

    fs::remove_all(unique_root, ec);
}

// 出厂模板（安装包真正分发的那份）必须携带 statistics.enabled 且默认 false；缺键会在
// 升级合并时被静默丢弃，用户改过的值下次升级消失且无任何报错。
TEST_CASE(shipped_template_carries_statistics_switch)
{
    std::ifstream input(MSIME_DEFAULT_CONFIG_PATH, std::ios::binary);
    REQUIRE(static_cast<bool>(input));
    const std::string installed((std::istreambuf_iterator<char>(input)), {});
    const auto parsed = toml::parse(installed);
    REQUIRE(!parsed["statistics"]["enabled"].value_or(true));
    REQUIRE_EQ(parsed["statistics"]["retention"].value_or(std::string()), std::string("forever"));
}

// 统计保留策略：默认 forever（无文件 / 出厂模板 / 缺键 / 非法值四种形态），
// 合法值能落盘并在重读后保持，非法 setter 被拒绝且不污染文件。
TEST_CASE(statistics_retention_defaults_forever_and_round_trips)
{
    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot();
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);

    // 无文件形态：全局量保持静态默认 forever。
    {
        ScopedConfigLocation local_app_data_env(local_app_data);
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredStatisticsRetention(), std::string("forever"));
    }

    SeedTemplate(data_dir);

    {
        ScopedConfigLocation local_app_data_env(local_app_data);

        // 出厂模板形态：显式携带 forever。
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredStatisticsRetention(), std::string("forever"));

        // 缺键形态：手写一份没有 retention 的配置 → forever。
        WriteText(data_dir / L"config.toml", "[statistics]\nenabled = true\n");
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredStatisticsRetention(), std::string("forever"));

        // 非法值形态：手写坏值 → forever。坏配置绝不能触发自动清理误删数据。
        WriteText(data_dir / L"config.toml", "[statistics]\nretention = \"2d\"\n");
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredStatisticsRetention(), std::string("forever"));

        // setter 拒绝非法枚举：不写文件、内存值不动。
        const std::string before_invalid_set = ReadText(data_dir / L"config.toml");
        REQUIRE(!SetConfiguredStatisticsRetention("2d"));
        REQUIRE_EQ(GetConfiguredStatisticsRetention(), std::string("forever"));
        REQUIRE_EQ(ReadText(data_dir / L"config.toml"), before_invalid_set);

        // 逐值落盘重读：每个合法枚举都能往返。
        for (const char *value : {"30d", "90d", "180d", "365d", "forever"})
        {
            REQUIRE(SetConfiguredStatisticsRetention(value));
            REQUIRE_EQ(GetConfiguredStatisticsRetention(), std::string(value));
            InitImeConfig();
            REQUIRE_EQ(GetConfiguredStatisticsRetention(), std::string(value));
        }

        // 缺段写入：段不存在时 setter 必须自己补出 [statistics]。
        WriteText(data_dir / L"config.toml", "[input]\nschema = \"quanpin\"\n");
        REQUIRE(SetConfiguredStatisticsRetention("90d"));
        {
            const std::string text = ReadText(data_dir / L"config.toml");
            REQUIRE(text.find("[statistics]") != std::string::npos);
            REQUIRE(text.find("retention = \"90d\"") != std::string::npos);
        }
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredStatisticsRetention(), std::string("90d"));
    }

    fs::remove_all(unique_root, ec);
}

// 设置窗口驻留时长：出厂模板必须带这个键（否则升级合并时用户的选择会被丢弃），
// 缺键和非法值回退到 off（关闭即退出），合法值能落盘并在重读后保持。
TEST_CASE(settings_window_linger_defaults_off_and_round_trips)
{
    {
        std::ifstream input(MSIME_DEFAULT_CONFIG_PATH, std::ios::binary);
        REQUIRE(static_cast<bool>(input));
        const std::string installed((std::istreambuf_iterator<char>(input)), {});
        const auto parsed = toml::parse(installed);
        REQUIRE_EQ(parsed["appearance"]["settings_window_linger"].value_or(std::string()), std::string("off"));
    }

    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot();
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    SeedTemplate(data_dir);

    {
        ScopedConfigLocation local_app_data_env(local_app_data);

        InitImeConfig();
        REQUIRE_EQ(GetConfiguredSettingsWindowLinger(), std::string("off"));

        WriteText(data_dir / L"config.toml", "[appearance]\ntheme_mode = \"dark\"\n");
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredSettingsWindowLinger(), std::string("off"));

        WriteText(data_dir / L"config.toml", "[appearance]\nsettings_window_linger = \"2h\"\n");
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredSettingsWindowLinger(), std::string("off"));

        const std::string before_invalid_set = ReadText(data_dir / L"config.toml");
        REQUIRE(!SetConfiguredSettingsWindowLinger("2h"));
        REQUIRE_EQ(GetConfiguredSettingsWindowLinger(), std::string("off"));
        REQUIRE_EQ(ReadText(data_dir / L"config.toml"), before_invalid_set);

        for (const char *value : {"off", "1m", "5m", "10m", "30m", "60m", "forever"})
        {
            REQUIRE(SetConfiguredSettingsWindowLinger(value));
            REQUIRE_EQ(GetConfiguredSettingsWindowLinger(), std::string(value));
            InitImeConfig();
            REQUIRE_EQ(GetConfiguredSettingsWindowLinger(), std::string(value));
        }
    }

    fs::remove_all(unique_root, ec);
}

// 悬浮工具栏自动隐藏：两个模板都必须带键（否则升级合并会丢掉用户的选择），
// 默认关闭、延时 5 秒；延时越界的读值回退到 5，越界的写入被拒且不落盘。
TEST_CASE(floating_toolbar_auto_hide_defaults_and_round_trips)
{
    {
        std::ifstream input(MSIME_DEFAULT_CONFIG_PATH, std::ios::binary);
        REQUIRE(static_cast<bool>(input));
        const std::string installed((std::istreambuf_iterator<char>(input)), {});
        const auto working_path =
            std::filesystem::path(MSIME_DEFAULT_CONFIG_PATH).parent_path().parent_path().parent_path() /
            "server/assets/config/config.toml";
        std::ifstream development(working_path, std::ios::binary);
        REQUIRE(static_cast<bool>(development));
        const std::string working((std::istreambuf_iterator<char>(development)), {});
        for (const std::string *text : {&installed, &working})
        {
            const auto parsed = toml::parse(*text);
            REQUIRE(!parsed["general"]["floating_toolbar_auto_hide"].value_or(true));
            REQUIRE_EQ(parsed["general"]["floating_toolbar_auto_hide_delay"].value_or(0), 5);
        }
    }

    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot();
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    SeedTemplate(data_dir);

    {
        ScopedConfigLocation local_app_data_env(local_app_data);

        InitImeConfig();
        REQUIRE(!GetConfiguredFloatingToolbarAutoHide());
        REQUIRE_EQ(GetConfiguredFloatingToolbarAutoHideDelay(), 5);

        WriteText(data_dir / L"config.toml", "[general]\nfloating_toolbar_auto_hide_delay = 0\n");
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredFloatingToolbarAutoHideDelay(), 5);
        WriteText(data_dir / L"config.toml", "[general]\nfloating_toolbar_auto_hide_delay = 61\n");
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredFloatingToolbarAutoHideDelay(), 5);

        const std::string before_invalid_set = ReadText(data_dir / L"config.toml");
        REQUIRE(!SetConfiguredFloatingToolbarAutoHideDelay(0));
        REQUIRE(!SetConfiguredFloatingToolbarAutoHideDelay(61));
        REQUIRE_EQ(GetConfiguredFloatingToolbarAutoHideDelay(), 5);
        REQUIRE_EQ(ReadText(data_dir / L"config.toml"), before_invalid_set);

        REQUIRE(SetConfiguredFloatingToolbarAutoHide(true));
        for (const int seconds : {1, 12, 60})
        {
            REQUIRE(SetConfiguredFloatingToolbarAutoHideDelay(seconds));
            InitImeConfig();
            REQUIRE(GetConfiguredFloatingToolbarAutoHide());
            REQUIRE_EQ(GetConfiguredFloatingToolbarAutoHideDelay(), seconds);
        }
    }

    fs::remove_all(unique_root, ec);
}
