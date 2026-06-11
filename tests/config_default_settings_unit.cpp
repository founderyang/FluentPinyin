#include "config_winui/default_settings.h"

#include "common/candidate_font.h"
#include "common/constants.h"
#include "config_winui/hotkey_helpers.h"
#include "config_winui/settings_binding.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <windows.h>

namespace {

int g_failures = 0;
std::filesystem::path g_isolated_appdata_root;

void Expect(bool condition, std::string_view message) {
  if (condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAIL: " << message << "\n";
}

void UseIsolatedAppData() {
  g_isolated_appdata_root = std::filesystem::current_path() /
                            (L"config-default-settings-appdata-" +
                             std::to_wstring(GetCurrentProcessId()));
  const auto local = g_isolated_appdata_root / L"Local";
  const auto roaming = g_isolated_appdata_root / L"Roaming";
  const auto program_data = g_isolated_appdata_root / L"ProgramData";
  std::error_code error;
  std::filesystem::remove_all(g_isolated_appdata_root, error);
  std::filesystem::create_directories(local, error);
  std::filesystem::create_directories(roaming, error);
  std::filesystem::create_directories(program_data, error);
  SetEnvironmentVariableW(L"LOCALAPPDATA", local.c_str());
  SetEnvironmentVariableW(L"APPDATA", roaming.c_str());
  SetEnvironmentVariableW(L"PROGRAMDATA", program_data.c_str());
}

void CleanupIsolatedAppData() {
  std::error_code error;
  std::filesystem::remove_all(g_isolated_appdata_root, error);
}

void TestResetDefaultSettings() {
  namespace settings = fp::config_winui;

  settings::WriteStringSetting(fp::kCandidateFontFamilySetting, L"source_han_sans");
  settings::WriteStringSetting(fp::kInputSchemeSetting, fp::kInputSchemeDoublePinyin);
  settings::WriteBoolSetting(fp::kSyncAutoEnabledSetting, true);

  settings::ResetDefaultSettings();

  Expect(settings::ReadIntSetting(fp::kCandidateCountSetting,
                                  0,
                                  fp::kMinCandidateCount,
                                  fp::kMaxCandidateCount) == fp::kDefaultCandidateCount,
         "reset writes default candidate count");
  Expect(settings::ReadStringSetting(fp::kCandidateFontFamilySetting) ==
             std::wstring(fp::kDefaultCandidateFontFamily),
         "reset writes default candidate font");
  Expect(settings::ReadStringSetting(fp::kInputSchemeSetting) ==
             std::wstring(fp::kDefaultInputScheme),
         "reset writes default input scheme");
  Expect(settings::ReadStringSetting(fp::kDefaultInputModeSetting) ==
             std::wstring(fp::kDefaultInputMode),
         "reset writes default input mode");
  Expect(settings::ReadStringSetting(fp::kDefaultCharsetSetting) ==
             std::wstring(fp::kDefaultCharset),
         "reset writes default charset");
  Expect(!settings::ReadBoolSetting(fp::kSyncAutoEnabledSetting, true),
         "reset disables automatic sync");
  Expect(settings::ReadIntSetting(fp::kSyncAutoIntervalMinutesSetting,
                                  0,
                                  fp::kMinSyncAutoIntervalMinutes,
                                  fp::kMaxSyncAutoIntervalMinutes) ==
             fp::kDefaultSyncAutoIntervalMinutes,
         "reset writes default sync interval");
  Expect(settings::ReadStringSetting(fp::kSyncObjectKeySetting) ==
             std::wstring(fp::kDefaultSyncObjectKey),
         "reset writes default sync object key");
  for (const auto& shortcut : settings::HotkeyShortcutDefinitions()) {
    Expect(settings::ReadStringSetting(shortcut.key) == std::wstring(shortcut.fallback),
           "reset writes default shortcut binding");
  }
}

}  // namespace

int main() {
  UseIsolatedAppData();
  TestResetDefaultSettings();
  CleanupIsolatedAppData();
  if (g_failures != 0) {
    std::cerr << g_failures << " config default settings failure(s)\n";
    return 1;
  }
  std::cout << "Config default settings tests passed\n";
  return 0;
}
