#include "config_winui/fuzzy_pinyin_custom_rules.h"

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

void TestNormalizeCustomRule() {
  namespace fuzzy = fp::config_winui;

  Expect(fuzzy::NormalizeFuzzyPinyinCustomRuleToken(L" SH / S ") == L"sh=s",
         "normalizes spaces, case, and slash separators");
  Expect(fuzzy::NormalizeFuzzyPinyinCustomRuleToken(L"zh＝z") == L"zh=z",
         "normalizes full-width equals separator");
  Expect(fuzzy::NormalizeFuzzyPinyinCustomRuleToken(L"ch／c") == L"ch=c",
         "normalizes full-width slash separator");
  Expect(fuzzy::NormalizeFuzzyPinyinCustomRuleToken(L"s-sh") == L"s=sh",
         "normalizes dash separator");
  Expect(fuzzy::NormalizeFuzzyPinyinCustomRuleToken(L"=sh").empty(),
         "rejects missing left side");
  Expect(fuzzy::NormalizeFuzzyPinyinCustomRuleToken(L"sh=").empty(),
         "rejects missing right side");
  Expect(fuzzy::NormalizeFuzzyPinyinCustomRuleToken(L"sh=s=z").empty(),
         "rejects multiple separators");
  Expect(fuzzy::NormalizeFuzzyPinyinCustomRuleToken(L"sh=1").empty(),
         "rejects non-letter tokens");
}

void TestParseJoinSplit() {
  namespace fuzzy = fp::config_winui;

  const auto rules = fuzzy::ParseFuzzyPinyinCustomRuleSetting(
      L" sh/s;zh=z|sh=s\r\nbad=1,ZH=Z ");
  Expect(rules.size() == 2, "parses valid unique custom rules");
  Expect(rules[0] == L"sh=s", "keeps first normalized rule");
  Expect(rules[1] == L"zh=z", "keeps second normalized rule");
  Expect(fuzzy::JoinFuzzyPinyinCustomRules(rules, L',') == L"sh=s,zh=z",
         "joins custom rules with requested separator");
  Expect(fuzzy::NormalizeFuzzyPinyinCustomRulesForStorage(L"SH/S\nZH/Z") ==
             L"sh=s,zh=z",
         "normalizes custom rules for storage");

  const auto [left, right] = fuzzy::SplitFuzzyPinyinCustomRule(L"sh=s");
  Expect(left == L"sh" && right == L"s", "splits custom rule pairs");
  Expect(fuzzy::SplitFuzzyPinyinCustomRule(L"sh").second.empty(),
         "splits rule without separator");

  const std::vector<std::wstring> enabled{L"nl", L"c_ch"};
  Expect(fuzzy::FuzzyPinyinRuleSelected(enabled, L"c_ch"),
         "detects selected fuzzy pinyin rules");
  Expect(!fuzzy::FuzzyPinyinRuleSelected(enabled, L"z_zh"),
         "detects unselected fuzzy pinyin rules");
}

}  // namespace

int main() {
  TestNormalizeCustomRule();
  TestParseJoinSplit();
  if (g_failures != 0) {
    std::cerr << g_failures << " fuzzy pinyin custom rule failure(s)\n";
    return 1;
  }
  std::cout << "Fuzzy pinyin custom rule tests passed\n";
  return 0;
}
