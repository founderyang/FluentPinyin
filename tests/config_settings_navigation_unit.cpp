#include "config_winui/settings_navigation.h"

#include <iostream>
#include <string_view>

namespace {

int g_failures = 0;

void Expect(bool condition, std::string_view message) {
  if (condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAIL: " << message << "\n";
}

void TestCommandLinePageExtraction() {
  namespace nav = fp::config_winui;

  Expect(nav::ExtractSettingsPageValue(L"fluent-pinyin-settings.exe") == L"",
         "missing page switch returns empty page value");
  Expect(nav::ExtractSettingsPageValue(L"fluent-pinyin-settings.exe --page=appearance") ==
             L"appearance",
         "unquoted page switch is parsed");
  Expect(nav::ExtractSettingsPageValue(L"fluent-pinyin-settings.exe --page=\"hotkeys\"") ==
             L"hotkeys",
         "quoted page switch is parsed");
  Expect(nav::ExtractSettingsPageValue(L"--page=sync --other") == L"sync",
         "page switch stops at whitespace");
}

void TestPageNormalization() {
  namespace nav = fp::config_winui;

  Expect(nav::NormalizeSettingsPageTag(L"") == std::wstring(nav::kSettingsPageGeneral),
         "empty page normalizes to general");
  Expect(nav::NormalizeSettingsPageTag(L"input") == std::wstring(nav::kSettingsPageGeneral),
         "input alias normalizes to general");
  Expect(nav::NormalizeSettingsPageTag(L"keys") == std::wstring(nav::kSettingsPageHotkeys),
         "keys alias normalizes to hotkeys");
  Expect(nav::NormalizeSettingsPageTag(L"custom-phrases") ==
             std::wstring(nav::kSettingsPageLexicon),
         "custom phrases alias normalizes to lexicon");
  Expect(nav::NormalizeSettingsPageTag(L"imports") ==
             std::wstring(nav::kSettingsPageLexicon),
         "imports alias normalizes to lexicon");
  Expect(nav::NormalizeSettingsPageTag(L"about") == std::wstring(nav::kSettingsPageAbout),
         "known page tag is preserved");
  Expect(nav::NormalizeSettingsPageTag(L"unknown") == std::wstring(nav::kSettingsPageGeneral),
         "unknown page normalizes to general");
}

void TestInitialPageTag() {
  namespace nav = fp::config_winui;

  Expect(nav::InitialSettingsPageTag(L"--page=dictionaries") ==
             std::wstring(nav::kSettingsPageLexicon),
         "initial page applies alias normalization");
  Expect(nav::InitialSettingsPageTag(L"--page=advanced") ==
             std::wstring(nav::kSettingsPageAdvanced),
         "initial page keeps known page tags");
}

}  // namespace

int main() {
  TestCommandLinePageExtraction();
  TestPageNormalization();
  TestInitialPageTag();
  if (g_failures != 0) {
    std::cerr << g_failures << " config settings navigation failure(s)\n";
    return 1;
  }
  std::cout << "Config settings navigation tests passed\n";
  return 0;
}
