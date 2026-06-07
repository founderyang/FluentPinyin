#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fp::config_winui {

std::wstring NormalizeFuzzyPinyinCustomRuleToken(std::wstring_view token);
std::vector<std::wstring> ParseFuzzyPinyinCustomRuleSetting(std::wstring_view value);
std::wstring JoinFuzzyPinyinCustomRules(const std::vector<std::wstring>& rules,
                                        wchar_t separator);
std::wstring NormalizeFuzzyPinyinCustomRulesForStorage(std::wstring_view value);
std::pair<std::wstring, std::wstring> SplitFuzzyPinyinCustomRule(std::wstring_view rule);
bool FuzzyPinyinRuleSelected(const std::vector<std::wstring>& rules,
                             std::wstring_view id);

}  // namespace fp::config_winui
