#include "dictionary_validation.h"

#include "ipc/ipc_protocol_limits.h"
#include "utils/common_utils.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace SettingsDictionary::Validation
{
bool ShouldSkipImportLine(const std::string &line, bool &in_yaml_header)
{
    const auto begin = line.find_first_not_of(" \t");
    if (begin == std::string::npos)
        return true;
    const auto end = line.find_last_not_of(" \t");
    const auto size = end - begin + 1;
    if (size == 3 && line.compare(begin, 3, "---") == 0)
    {
        in_yaml_header = true;
        return true;
    }
    if (size == 3 && line.compare(begin, 3, "...") == 0)
    {
        in_yaml_header = false;
        return true;
    }
    if (in_yaml_header)
        return true;
    return line[begin] == '#';
}

bool ParseCodedImportLine(const std::string &line, std::string &word, std::string &code, int &weight,
                          std::string &message)
{
    const auto trim = [](std::string value) {
        const auto begin = value.find_first_not_of(' ');
        if (begin == std::string::npos)
            return std::string{};
        const auto end = value.find_last_not_of(' ');
        return value.substr(begin, end - begin + 1);
    };

    std::vector<std::string> fields;
    std::string::size_type start = 0;
    for (;;)
    {
        const auto separator = line.find('\t', start);
        fields.push_back(trim(line.substr(start, separator == std::string::npos ? separator : separator - start)));
        if (separator == std::string::npos)
            break;
        start = separator + 1;
    }
    if (fields.size() != 2 && fields.size() != 3)
    {
        message = "格式错误，应为：词语<Tab>编码[<Tab>权重]";
        return false;
    }

    word = fields[0];
    code = fields[1];
    if (word.empty() || code.empty())
    {
        message = "词语和编码不能为空";
        return false;
    }
    if (fields.size() == 2)
    {
        weight = kDefaultCodedImportWeight;
        return true;
    }

    const std::string &weight_text = fields[2];
    if (weight_text.empty() || weight_text.find('=') != std::string::npos)
    {
        weight = kDefaultCodedImportWeight;
        return true;
    }
    if (!std::all_of(weight_text.begin(), weight_text.end(), [](unsigned char ch) { return std::isdigit(ch); }))
    {
        message = "权重必须是非负整数";
        return false;
    }
    try
    {
        weight = std::stoi(weight_text);
        return true;
    }
    catch (...)
    {
        message = "权重数值无效";
        return false;
    }
}

bool QuickPhraseFitsNamedPipe(const std::string &phrase)
{
    try
    {
        return string_to_wstring(phrase).size() <= FanyImePipeLimits::CandidateTextMaxLength;
    }
    catch (...)
    {
        return false;
    }
}
} // namespace SettingsDictionary::Validation
