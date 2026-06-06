#include "tsf/tsf_settings_adapter.h"

#include <algorithm>

namespace fp::tsf {
namespace {

fp::SettingsStore& RuntimeSettingsStore() {
  static fp::SettingsStore store;
  return store;
}

bool IsReasonableStoredScreenPoint(POINT point) {
  const int virtual_left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int virtual_top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int virtual_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int virtual_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  const int virtual_right = virtual_left + virtual_width;
  const int virtual_bottom = virtual_top + virtual_height;
  const int padding = std::max({4096, virtual_width, virtual_height});
  return point.x >= virtual_left - padding && point.x <= virtual_right + padding &&
         point.y >= virtual_top - padding && point.y <= virtual_bottom + padding;
}

}  // namespace

std::optional<std::wstring> FindSettingValue(std::wstring_view key) {
  return RuntimeSettingsStore().FindString(key);
}

void WriteSettingLine(std::wstring_view key, std::wstring_view value) {
  RuntimeSettingsStore().WriteString(key, value);
}

void WriteStringSetting(std::wstring_view key, std::wstring_view value) {
  WriteSettingLine(key, value);
}

void WriteBoolSetting(std::wstring_view key, bool value) {
  WriteSettingLine(key, value ? L"1" : L"0");
}

void WritePointSetting(std::wstring_view key, POINT point) {
  WriteSettingLine(key, std::to_wstring(point.x) + L"," + std::to_wstring(point.y));
}

void WriteIntSetting(std::wstring_view key, int value) {
  WriteSettingLine(key, std::to_wstring(value));
}

bool WriteSettings(std::span<const fp::SettingUpdate> updates) {
  return RuntimeSettingsStore().WriteStrings(updates);
}

std::wstring ReadStringSetting(std::wstring_view key, std::wstring_view default_value) {
  const std::optional<std::wstring> value = FindSettingValue(key);
  return value.has_value() ? *value : std::wstring(default_value);
}

std::optional<bool> ReadOptionalBoolSetting(std::wstring_view key) {
  const std::optional<std::wstring> value = FindSettingValue(key);
  if (!value.has_value()) {
    return std::nullopt;
  }
  return fp::IsTruthySettingValue(*value);
}

bool ReadBoolSetting(std::wstring_view key, bool default_value) {
  const std::optional<bool> value = ReadOptionalBoolSetting(key);
  return value.has_value() ? *value : default_value;
}

std::optional<POINT> ReadPointSetting(std::wstring_view key) {
  const std::wstring value = ReadStringSetting(key);
  const size_t separator = value.find(L',');
  if (separator == std::wstring::npos) {
    return std::nullopt;
  }
  try {
    POINT point{std::stoi(value.substr(0, separator)),
                std::stoi(value.substr(separator + 1))};
    return IsReasonableStoredScreenPoint(point) ? std::optional<POINT>(point) : std::nullopt;
  } catch (...) {
    return std::nullopt;
  }
}

int ReadIntSetting(std::wstring_view key, int default_value, int min_value, int max_value) {
  const std::optional<std::wstring> value = FindSettingValue(key);
  if (!value.has_value()) {
    return default_value;
  }
  try {
    return std::clamp(std::stoi(*value), min_value, max_value);
  } catch (...) {
    return default_value;
  }
}

bool ReadBoolSettingMigrated(std::wstring_view key,
                             bool default_value,
                             std::wstring_view legacy_key) {
  if (const std::optional<bool> value = ReadOptionalBoolSetting(key)) {
    return *value;
  }
  return ReadBoolSetting(legacy_key, default_value);
}

bool ReadToolbarVisibleSetting(std::wstring_view current_key, bool default_value) {
  if (const std::optional<bool> value = ReadOptionalBoolSetting(current_key)) {
    return *value;
  }
  const std::optional<bool> legacy_value = ReadOptionalBoolSetting(L"toolbar_visible");
  return legacy_value.has_value() && *legacy_value ? true : default_value;
}

}  // namespace fp::tsf
