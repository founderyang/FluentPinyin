#include "config_winui/app_paths.h"

#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>
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
                            (L"config-app-paths-appdata-" +
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

void TestAppPaths() {
  namespace paths = fp::config_winui;

  const auto module_dir = paths::ModuleDirectory();
  Expect(!module_dir.empty(), "ModuleDirectory is not empty");
  Expect(std::filesystem::exists(module_dir), "ModuleDirectory exists");

  const auto sibling = paths::SiblingExe(L"fluent-pinyin-settings.exe");
  Expect(sibling.parent_path() == module_dir, "SiblingExe uses module directory");
  Expect(sibling.filename() == L"fluent-pinyin-settings.exe",
         "SiblingExe preserves requested file name");

  const auto rime_user_data = paths::RimeUserDataPath();
  Expect(rime_user_data == g_isolated_appdata_root / L"Roaming" / L"FluentPinyin" / L"Rime",
         "RimeUserDataPath uses roaming FluentPinyin directory");

  (void)paths::WindowIconPath();
  paths::EnsureUiFontsLoaded();
  paths::EnsureUiFontsLoaded();
}

}  // namespace

int main() {
  UseIsolatedAppData();
  TestAppPaths();
  std::error_code cleanup_error;
  std::filesystem::remove_all(g_isolated_appdata_root, cleanup_error);
  if (g_failures != 0) {
    std::cerr << g_failures << " config app path failure(s)\n";
    return 1;
  }
  std::cout << "Config app path tests passed\n";
  return 0;
}
