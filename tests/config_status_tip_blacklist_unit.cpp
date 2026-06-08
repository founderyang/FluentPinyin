#include "config_winui/status_tip_blacklist.h"

#include "common/constants.h"
#include "config_winui/settings_binding.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
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
                            (L"config-status-tip-blacklist-appdata-" +
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

void TestNormalize() {
  namespace blacklist = fp::config_winui;

  Expect(blacklist::NormalizeStatusTipBlacklistToken(L"  explorer.exe  ") ==
             L"explorer.exe",
         "trims tokens");
  Expect(blacklist::NormalizeStatusTipBlacklistToken(L"\"QQ.exe\"") == L"QQ.exe",
         "strips matching double quotes");
  Expect(blacklist::NormalizeStatusTipBlacklistToken(L"'TIM.exe'") == L"TIM.exe",
         "strips matching single quotes");
  Expect(blacklist::NormalizeStatusTipBlacklistToken(L"C:\\Program Files\\App\\QQ.exe") ==
             L"QQ.exe",
         "reduces paths to file names");
  Expect(blacklist::NormalizeStatusTipBlacklistToken(L"*Host*") == L"*Host*",
         "keeps wildcard tokens intact");
}

void TestParseAndJoin() {
  namespace blacklist = fp::config_winui;

  const auto items = blacklist::ParseStatusTipBlacklistSetting(
      L" explorer.exe;QQ.exe|qq.exe\r\nC:\\Tools\\TIM.exe,, ");
  Expect(items.size() == 3, "parses separators and removes duplicates");
  Expect(items[0] == L"explorer.exe", "keeps first parsed item");
  Expect(items[1] == L"QQ.exe", "keeps original casing of first duplicate");
  Expect(items[2] == L"TIM.exe", "normalizes file path items");

  Expect(blacklist::StatusTipBlacklistContains(items, L"qq.EXE"),
         "contains comparison is case-insensitive");
  Expect(!blacklist::StatusTipBlacklistContains(items, L"notepad.exe"),
         "contains comparison rejects missing values");

  const std::vector<std::wstring> joined_items{L" app.exe ", L"", L"C:\\Temp\\tool.exe"};
  Expect(blacklist::JoinStatusTipBlacklistItems(joined_items) == L"app.exe,tool.exe",
         "joins normalized non-empty items");
}

void TestCurrentSettingAccessors() {
  namespace blacklist = fp::config_winui;

  blacklist::WriteStringSetting(fp::kStatusTipBlacklistSetting, L"app.exe,app.exe;tool.exe");
  const auto items = blacklist::CurrentStatusTipBlacklistItems();
  Expect(items.size() == 2, "current blacklist reader parses persisted settings");
  Expect(blacklist::CurrentStatusTipBlacklistCount() == 2,
         "current blacklist count reflects persisted settings");
}

}  // namespace

int main() {
  UseIsolatedAppData();
  TestNormalize();
  TestParseAndJoin();
  TestCurrentSettingAccessors();
  CleanupIsolatedAppData();
  if (g_failures != 0) {
    std::cerr << g_failures << " config status tip blacklist failure(s)\n";
    return 1;
  }
  std::cout << "Config status tip blacklist tests passed\n";
  return 0;
}
