#pragma once

#include <cstddef>
#include <string>

namespace SettingsDictionary::Validation
{
inline constexpr int kDefaultCodedImportWeight = 10000;

bool ShouldSkipImportLine(const std::string &line, bool &in_yaml_header);
bool ParseCodedImportLine(const std::string &line, std::string &word, std::string &code, int &weight,
                          std::string &message);
bool QuickPhraseFitsNamedPipe(const std::string &phrase);
} // namespace SettingsDictionary::Validation
