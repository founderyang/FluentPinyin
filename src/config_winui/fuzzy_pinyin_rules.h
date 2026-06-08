#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace fp::config_winui {

struct FuzzyPinyinRuleDefinition {
  std::wstring_view id;
  std::wstring_view name;
  std::wstring_view example;
};

const std::array<FuzzyPinyinRuleDefinition, 10>& FuzzyPinyinRuleDefinitions();
bool IsKnownFuzzyPinyinRule(std::wstring_view id);
std::vector<std::wstring> ParseFuzzyPinyinRuleSetting(std::wstring_view value,
                                                      bool default_to_all);
std::wstring JoinFuzzyPinyinRules(const std::vector<std::wstring>& rules);

}  // namespace fp::config_winui
