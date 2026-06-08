#include "config_winui/settings_options.h"

#include "common/constants.h"

#include <iostream>
#include <string>
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

void TestChoiceTables() {
  namespace options = fp::config_winui;

  Expect(options::InputSchemeChoices().size() == 2,
         "input scheme choices expose full and double pinyin");
  Expect(options::InputSchemeChoices()[0].second == std::wstring(fp::kInputSchemePinyin),
         "full pinyin choice uses the shared setting value");
  Expect(options::DoublePinyinSchemeChoices().front().second ==
             std::wstring(fp::kDefaultDoublePinyinScheme),
         "double pinyin choices keep the default first");
  Expect(options::DefaultInputModeChoices()[0].second == std::wstring(fp::kDefaultInputMode),
         "default input mode choices keep Chinese as the default");
  Expect(options::DefaultCharsetChoices()[0].second == std::wstring(fp::kDefaultCharset),
         "default charset choices keep simplified as the default");
  Expect(options::CandidateLayoutChoices()[0].second ==
             std::wstring(fp::kDefaultCandidateLayout),
         "candidate layout choices keep horizontal as the default");
  Expect(options::DefaultShapeChoices().size() == 2,
         "shape choices expose half and full shape");
  Expect(options::DefaultPunctuationChoices().size() == 2,
         "punctuation choices expose Chinese and English punctuation");
}

void TestIconTextAndIntervals() {
  namespace options = fp::config_winui;

  Expect(options::InputSchemeIconText(fp::kInputSchemeDoublePinyin) == L"双",
         "double pinyin icon text is stable");
  Expect(options::InputSchemeIconText(fp::kInputSchemePinyin) == L"全",
         "full pinyin icon text is stable");
  Expect(options::DefaultInputModeChoiceIconText(fp::kInputModeEnglish) == L"英",
         "English input-mode icon text is stable");
  Expect(options::DefaultInputModeChoiceIconText(fp::kInputModeChinese) == L"中",
         "Chinese input-mode icon text is stable");
  Expect(options::DefaultCharsetChoiceIconText(fp::kCharsetTraditional) == L"繁",
         "traditional charset icon text is stable");
  Expect(options::DefaultCharsetChoiceIconText(fp::kCharsetSimplified) == L"简",
         "simplified charset icon text is stable");
  Expect(options::SyncAutoIntervalMinutesFromValue(L"360") == 360,
         "sync interval parser accepts numeric choices");
  Expect(options::SyncAutoIntervalMinutesFromValue(L"bad") ==
             fp::kDefaultSyncAutoIntervalMinutes,
         "sync interval parser falls back to the shared default");
}

}  // namespace

int main() {
  TestChoiceTables();
  TestIconTextAndIntervals();
  if (g_failures != 0) {
    std::cerr << g_failures << " config settings options failure(s)\n";
    return 1;
  }
  std::cout << "Config settings options tests passed\n";
  return 0;
}
