#include "common/status_tip_blacklist.h"

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

}  // namespace

int main() {
  Expect(fp::NormalizeStatusTipBlacklistToken(L"  explorer.exe  ") == L"explorer.exe",
         "normalizer trims tokens");
  Expect(fp::NormalizeStatusTipBlacklistToken(L"\"QQ.exe\"") == L"QQ.exe",
         "normalizer strips quotes");
  Expect(fp::NormalizeStatusTipBlacklistToken(L"C:\\Program Files\\App\\QQ.exe") ==
             L"QQ.exe",
         "normalizer reduces paths to file names");
  Expect(fp::NormalizeStatusTipBlacklistToken(L"*Host*") == L"*Host*",
         "normalizer keeps wildcard tokens intact");

  const auto items =
      fp::ParseStatusTipBlacklistSetting(L" explorer.exe;QQ.exe|qq.exe\r\nC:\\Tools\\TIM.exe,, ");
  Expect(items.size() == 3, "parser splits separators and removes duplicates");
  Expect(items[0] == L"explorer.exe", "parser keeps first item");
  Expect(items[1] == L"QQ.exe", "parser keeps first duplicate casing");
  Expect(items[2] == L"TIM.exe", "parser normalizes path items");
  Expect(fp::StatusTipBlacklistContains(items, L"qq.EXE"),
         "contains comparison is case-insensitive");

  const std::vector<std::wstring> joined_items{L" app.exe ", L"", L"C:\\Temp\\tool.exe"};
  Expect(fp::JoinStatusTipBlacklistItems(joined_items) == L"app.exe,tool.exe",
         "join emits normalized non-empty items");

  Expect(fp::StatusTipProcessNameMatchesPattern(L"explorer.exe", L"explorer"),
         "bare process names match exe names");
  Expect(fp::StatusTipProcessNameMatchesPattern(L"ApplicationFrameHost.exe", L"*host.exe"),
         "wildcards match process names");
  Expect(fp::StatusTipProcessNameMatchesPattern(L"QQ.exe", L"q?.exe"),
         "question wildcard matches one character");
  Expect(!fp::StatusTipProcessNameMatchesPattern(L"QQ.exe", L"q??.exe"),
         "question wildcard does not match extra characters");
  Expect(fp::StatusTipProcessNameInList(L"TIM.exe", L"explorer.exe; C:\\Tools\\tim.exe"),
         "process list matching uses normalized tokens");

  if (g_failures != 0) {
    std::cerr << g_failures << " common status tip blacklist failure(s)\n";
    return 1;
  }
  return 0;
}
