#pragma once

#include "common/settings_store.h"

#include <windows.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace fp::tsf {

std::optional<std::wstring> FindSettingValue(std::wstring_view key);
void WriteSettingLine(std::wstring_view key, std::wstring_view value);
void WriteStringSetting(std::wstring_view key, std::wstring_view value);
void WriteBoolSetting(std::wstring_view key, bool value);
void WritePointSetting(std::wstring_view key, POINT point);
void WriteIntSetting(std::wstring_view key, int value);
bool WriteSettings(std::span<const fp::SettingUpdate> updates);

std::wstring ReadStringSetting(std::wstring_view key, std::wstring_view default_value = L"");
std::optional<bool> ReadOptionalBoolSetting(std::wstring_view key);
bool ReadBoolSetting(std::wstring_view key, bool default_value);
std::optional<POINT> ReadPointSetting(std::wstring_view key);
int ReadIntSetting(std::wstring_view key, int default_value, int min_value, int max_value);
bool ReadBoolSettingMigrated(std::wstring_view key,
                             bool default_value,
                             std::wstring_view legacy_key);
bool ReadToolbarVisibleSetting(std::wstring_view current_key, bool default_value);

}  // namespace fp::tsf
