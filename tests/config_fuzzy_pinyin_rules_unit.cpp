#include "common/constants.h"
#include "config_winui/fuzzy_pinyin_rules.h"

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

void TestDefinitions() {
  namespace fuzzy = fp::config_winui;

  const auto& definitions = fuzzy::FuzzyPinyinRuleDefinitions();
  Expect(definitions.size() == 10, "exposes all built-in fuzzy pinyin rules");
  Expect(definitions.front().id == std::wstring_view(L"nl"),
         "keeps nl as the first rule");
  Expect(definitions.back().id == std::wstring_view(L"s_sh"),
         "keeps s_sh as the last rule");
  Expect(fuzzy::IsKnownFuzzyPinyinRule(L"c_ch"), "accepts known rule ids");
  Expect(!fuzzy::IsKnownFuzzyPinyinRule(L"bad_rule"), "rejects unknown rule ids");
}

void TestParseJoin() {
  namespace fuzzy = fp::config_winui;

  const auto selected = fuzzy::ParseFuzzyPinyinRuleSetting(
      L"nl;bad|c_ch c_ch\tz_zh", false);
  Expect(selected.size() == 3, "parses valid unique rule ids");
  Expect(selected[0] == L"nl", "preserves first valid rule order");
  Expect(selected[1] == L"c_ch", "filters duplicate valid rules");
  Expect(selected[2] == L"z_zh", "parses whitespace-delimited rules");
  Expect(fuzzy::JoinFuzzyPinyinRules(selected) == L"nl,c_ch,z_zh",
         "joins selected rule ids for storage");

  const auto all = fuzzy::ParseFuzzyPinyinRuleSetting(L"all", false);
  Expect(all.size() == fuzzy::FuzzyPinyinRuleDefinitions().size(),
         "expands all marker to every rule");

  const auto default_all = fuzzy::ParseFuzzyPinyinRuleSetting(L"unknown", true);
  Expect(default_all.size() == fuzzy::FuzzyPinyinRuleDefinitions().size(),
         "defaults to all when requested and no valid rules remain");

  const auto common =
      fuzzy::ParseFuzzyPinyinRuleSetting(fp::kDefaultCommonFuzzyPinyinRules, false);
  Expect(std::wstring_view(fuzzy::JoinFuzzyPinyinRules(common)) ==
             fp::kDefaultCommonFuzzyPinyinRules,
         "keeps common fuzzy pinyin defaults centralized");
}

}  // namespace

int main() {
  TestDefinitions();
  TestParseJoin();
  if (g_failures != 0) {
    std::cerr << g_failures << " fuzzy pinyin rule failure(s)\n";
    return 1;
  }
  std::cout << "Fuzzy pinyin rule tests passed\n";
  return 0;
}
