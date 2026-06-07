#include "config_winui/settings_binding.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <system_error>

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
                            (L"config-settings-binding-appdata-" +
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

std::wstring ReadFileText(const std::filesystem::path& path) {
  std::wifstream input(path);
  std::wstring text;
  std::wstring line;
  while (std::getline(input, line)) {
    text += line;
    text += L"\n";
  }
  return text;
}

void TestSettingsBinding() {
  namespace binding = fp::config_winui;

  const auto settings_path = binding::SettingsPath();
  binding::ResetSettingsCache();

  Expect(binding::ReadStringSetting(L"missing", L"default") == L"default",
         "ReadStringSetting returns default values");
  Expect(binding::WriteStringSetting(L"alpha", L"one"),
         "WriteStringSetting creates settings.ini");
  Expect(!binding::WriteStringSetting(L"alpha", L"one"),
         "WriteStringSetting reports unchanged values");
  Expect(binding::ReadStringSetting(L"alpha") == L"one",
         "ReadStringSetting sees written values");

  Expect(binding::WriteBoolSetting(L"flag", true), "WriteBoolSetting writes true");
  Expect(binding::ReadBoolSetting(L"flag", false), "ReadBoolSetting reads true");
  Expect(binding::WriteStringSetting(L"legacy_flag", L"yes"),
         "legacy bool value is written");
  Expect(binding::ReadBoolSettingMigrated(L"new_flag", false, L"legacy_flag"),
         "ReadBoolSettingMigrated falls back to legacy key");

  Expect(binding::WriteIntSetting(L"count", 99), "WriteIntSetting writes integers");
  Expect(binding::ReadIntSetting(L"count", 3, 3, 9) == 9,
         "ReadIntSetting clamps values to max");
  Expect(binding::WriteStringSetting(L"count", L"bad"),
         "invalid integer string is written");
  Expect(binding::ReadIntSetting(L"count", 5, 3, 9) == 5,
         "ReadIntSetting returns default for invalid values");

  Expect(binding::WriteStringSetting(L"sanitized", L"a\r\nb"),
         "WriteStringSetting accepts multiline source values");
  const std::wstring text = ReadFileText(settings_path);
  Expect(text.find(L"sanitized=a,,b") != std::wstring::npos,
         "WriteStringSetting sanitizes CR/LF");

  binding::ResetSettingsCache();
  Expect(binding::ReadStringSetting(L"alpha") == L"one",
         "ResetSettingsCache reloads persisted values");
}

}  // namespace

int main() {
  UseIsolatedAppData();
  TestSettingsBinding();
  std::error_code cleanup_error;
  std::filesystem::remove_all(g_isolated_appdata_root, cleanup_error);
  if (g_failures != 0) {
    std::cerr << g_failures << " config settings binding failure(s)\n";
    return 1;
  }
  std::cout << "Config settings binding tests passed\n";
  return 0;
}
