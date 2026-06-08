#include "config_winui/candidate_layout_settings.h"

#include "common/constants.h"
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
                            (L"config-candidate-layout-appdata-" +
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

void TestCandidateLayoutSettings() {
  namespace settings = fp::config_winui;

  Expect(settings::ReadCandidateLayoutSetting(fp::kDefaultCandidateLayout) ==
             std::wstring(fp::kDefaultCandidateLayout),
         "missing layout uses the supplied default");

  settings::WriteBoolSetting(fp::kCandidateHorizontalSetting, false);
  Expect(settings::ReadCandidateLayoutSetting(fp::kDefaultCandidateLayout) ==
             std::wstring(fp::kCandidateLayoutVertical),
         "legacy horizontal flag is used when layout key is missing");

  Expect(settings::WriteCandidateLayoutSetting(fp::kCandidateLayoutHorizontal),
         "writing horizontal layout reports a change");
  Expect(settings::ReadStringSetting(fp::kCandidateLayoutSetting) ==
             std::wstring(fp::kCandidateLayoutHorizontal),
         "writing horizontal layout updates the string setting");
  Expect(settings::ReadBoolSetting(fp::kCandidateHorizontalSetting, false),
         "writing horizontal layout updates the legacy bool setting");

  Expect(settings::WriteCandidateLayoutSetting(fp::kCandidateLayoutVertical),
         "writing vertical layout reports a change");
  Expect(settings::ReadStringSetting(fp::kCandidateLayoutSetting) ==
             std::wstring(fp::kCandidateLayoutVertical),
         "writing vertical layout updates the string setting");
  Expect(!settings::ReadBoolSetting(fp::kCandidateHorizontalSetting, true),
         "writing vertical layout updates the legacy bool setting");
}

}  // namespace

int main() {
  UseIsolatedAppData();
  TestCandidateLayoutSettings();
  CleanupIsolatedAppData();
  if (g_failures != 0) {
    std::cerr << g_failures << " config candidate layout settings failure(s)\n";
    return 1;
  }
  std::cout << "Config candidate layout settings tests passed\n";
  return 0;
}
