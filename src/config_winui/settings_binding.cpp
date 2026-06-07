#include "config_winui/settings_binding.h"

#include "common/settings_store.h"

#include <algorithm>
#include <optional>

namespace fp::config_winui {
namespace {

fp::SettingsStore& RuntimeSettingsStore() {
  static fp::SettingsStore store;
  static const bool migrated = store.EnsureSchemaVersion();
  (void)migrated;
  return store;
}

bool WriteSettingLine(std::wstring_view key, std::wstring_view value) {
  if (ReadStringSetting(key) == value) {
    return false;
  }
  return RuntimeSettingsStore().WriteString(key, value);
}

}  // namespace

std::filesystem::path SettingsPath() {
  return fp::GetSettingsPath();
}

void ResetSettingsCache() {
  RuntimeSettingsStore().Reset();
}

std::wstring ReadStringSetting(std::wstring_view key,
                               std::wstring_view default_value) {
  return RuntimeSettingsStore().ReadString(key, default_value);
}

int ReadIntSetting(std::wstring_view key,
                   int default_value,
                   int min_value,
                   int max_value) {
  try {
    return std::clamp(std::stoi(ReadStringSetting(key, std::to_wstring(default_value))),
                      min_value,
                      max_value);
  } catch (...) {
    return default_value;
  }
}

bool ReadBoolSetting(std::wstring_view key, bool default_value) {
  return RuntimeSettingsStore().ReadBool(key, default_value);
}

bool ReadBoolSettingMigrated(std::wstring_view key,
                             bool default_value,
                             std::wstring_view legacy_key) {
  if (const std::optional<bool> value = RuntimeSettingsStore().ReadOptionalBool(key)) {
    return *value;
  }
  return ReadBoolSetting(legacy_key, default_value);
}

bool WriteStringSetting(std::wstring_view key, std::wstring_view value) {
  return WriteSettingLine(key, fp::SanitizeSettingValue(value));
}

bool WriteBoolSetting(std::wstring_view key, bool value) {
  return WriteSettingLine(key, value ? L"1" : L"0");
}

bool WriteIntSetting(std::wstring_view key, int value) {
  return WriteSettingLine(key, std::to_wstring(value));
}

}  // namespace fp::config_winui
