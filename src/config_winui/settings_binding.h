#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace fp::config_winui {

std::filesystem::path SettingsPath();
void ResetSettingsCache();

std::wstring ReadStringSetting(std::wstring_view key,
                               std::wstring_view default_value = L"");
int ReadIntSetting(std::wstring_view key,
                   int default_value,
                   int min_value,
                   int max_value);
bool ReadBoolSetting(std::wstring_view key, bool default_value);
bool ReadBoolSettingMigrated(std::wstring_view key,
                             bool default_value,
                             std::wstring_view legacy_key);

bool WriteStringSetting(std::wstring_view key, std::wstring_view value);
bool WriteBoolSetting(std::wstring_view key, bool value);
bool WriteIntSetting(std::wstring_view key, int value);

}  // namespace fp::config_winui
