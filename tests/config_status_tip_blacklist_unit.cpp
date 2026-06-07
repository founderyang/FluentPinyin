#include "config_winui/status_tip_blacklist.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace {

int g_failures = 0;

void Expect(bool condition, std::string_view message) {
  if (condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAIL: " << message << "\n";
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

}  // namespace

int main() {
  TestNormalize();
  TestParseAndJoin();
  if (g_failures != 0) {
    std::cerr << g_failures << " config status tip blacklist failure(s)\n";
    return 1;
  }
  std::cout << "Config status tip blacklist tests passed\n";
  return 0;
}
