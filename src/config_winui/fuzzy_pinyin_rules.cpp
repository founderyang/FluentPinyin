#include "config_winui/fuzzy_pinyin_rules.h"

#include "common/fuzzy_pinyin.h"

#include <algorithm>

namespace fp::config_winui {

const std::array<FuzzyPinyinRuleDefinition, 10>& FuzzyPinyinRuleDefinitions() {
  static constexpr std::array<FuzzyPinyinRuleDefinition, 10> rules{{
      {fp::kFuzzyPinyinRuleIds[0], L"n / l", L"nan ? lan"},
      {fp::kFuzzyPinyinRuleIds[1], L"r / y", L"ran ? yan"},
      {fp::kFuzzyPinyinRuleIds[2], L"h / f", L"hu ? fu"},
      {fp::kFuzzyPinyinRuleIds[3], L"r / l", L"ru ? lu"},
      {fp::kFuzzyPinyinRuleIds[4], L"k / g", L"ka ? ga"},
      {fp::kFuzzyPinyinRuleIds[5], L"en / eng", L"shen ? sheng"},
      {fp::kFuzzyPinyinRuleIds[6], L"in / ing", L"xin ? xing"},
      {fp::kFuzzyPinyinRuleIds[7], L"c / ch", L"cao ? chao"},
      {fp::kFuzzyPinyinRuleIds[8], L"z / zh", L"zai ? zhai"},
      {fp::kFuzzyPinyinRuleIds[9], L"s / sh", L"san ? shan"},
  }};
  return rules;
}

bool IsKnownFuzzyPinyinRule(std::wstring_view id) {
  return fp::IsKnownFuzzyPinyinRuleId(id);
}

std::vector<std::wstring> ParseFuzzyPinyinRuleSetting(std::wstring_view value,
                                                      bool default_to_all) {
  std::vector<std::wstring> result;
  std::wstring token;
  auto append_token = [&]() {
    if (token.empty()) {
      return;
    }
    if (token == L"all") {
      for (const auto& rule : FuzzyPinyinRuleDefinitions()) {
        const std::wstring id(rule.id);
        if (std::find(result.begin(), result.end(), id) == result.end()) {
          result.push_back(id);
        }
      }
    } else if (IsKnownFuzzyPinyinRule(token) &&
               std::find(result.begin(), result.end(), token) == result.end()) {
      result.push_back(token);
    }
    token.clear();
  };

  for (const wchar_t ch : value) {
    if (ch == L',' || ch == L';' || ch == L'|' || ch == L' ' || ch == L'\t') {
      append_token();
    } else {
      token.push_back(ch);
    }
  }
  append_token();

  if (result.empty() && default_to_all) {
    for (const auto& rule : FuzzyPinyinRuleDefinitions()) {
      result.emplace_back(rule.id);
    }
  }
  return result;
}

std::wstring JoinFuzzyPinyinRules(const std::vector<std::wstring>& rules) {
  std::wstring value;
  for (const auto& rule : rules) {
    if (!IsKnownFuzzyPinyinRule(rule)) {
      continue;
    }
    if (!value.empty()) {
      value += L',';
    }
    value += rule;
  }
  return value;
}

}  // namespace fp::config_winui
