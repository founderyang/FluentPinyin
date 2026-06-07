#include "config_winui/fuzzy_pinyin_custom_rules.h"

#include <algorithm>

namespace fp::config_winui {

std::wstring NormalizeFuzzyPinyinCustomRuleToken(std::wstring_view token) {
  std::wstring normalized;
  bool has_separator = false;
  for (const wchar_t ch : token) {
    if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
      continue;
    }
    if (ch == L'=' || ch == L'/' || ch == L'／' || ch == L'＝' ||
        ch == L'?' || ch == L'-') {
      if (has_separator) {
        return {};
      }
      normalized.push_back(L'=');
      has_separator = true;
      continue;
    }
    if (ch >= L'A' && ch <= L'Z') {
      normalized.push_back(static_cast<wchar_t>(ch - L'A' + L'a'));
      continue;
    }
    if (ch >= L'a' && ch <= L'z') {
      normalized.push_back(ch);
      continue;
    }
    return {};
  }

  const size_t separator = normalized.find(L'=');
  if (separator == std::wstring::npos || separator == 0 ||
      separator + 1 >= normalized.size()) {
    return {};
  }
  return normalized;
}

std::vector<std::wstring> ParseFuzzyPinyinCustomRuleSetting(std::wstring_view value) {
  std::vector<std::wstring> result;
  std::wstring token;
  auto append_token = [&]() {
    const std::wstring normalized = NormalizeFuzzyPinyinCustomRuleToken(token);
    if (!normalized.empty() &&
        std::find(result.begin(), result.end(), normalized) == result.end()) {
      result.push_back(normalized);
    }
    token.clear();
  };

  for (const wchar_t ch : value) {
    if (ch == L',' || ch == L';' || ch == L'|' || ch == L'\r' || ch == L'\n') {
      append_token();
    } else {
      token.push_back(ch);
    }
  }
  append_token();
  return result;
}

std::wstring JoinFuzzyPinyinCustomRules(const std::vector<std::wstring>& rules,
                                        wchar_t separator) {
  std::wstring value;
  for (const auto& rule : rules) {
    if (rule.empty()) {
      continue;
    }
    if (!value.empty()) {
      value.push_back(separator);
    }
    value += rule;
  }
  return value;
}

std::wstring NormalizeFuzzyPinyinCustomRulesForStorage(std::wstring_view value) {
  return JoinFuzzyPinyinCustomRules(ParseFuzzyPinyinCustomRuleSetting(value), L',');
}

std::pair<std::wstring, std::wstring> SplitFuzzyPinyinCustomRule(std::wstring_view rule) {
  const size_t separator = rule.find(L'=');
  if (separator == std::wstring::npos) {
    return {std::wstring(rule), L""};
  }
  return {std::wstring(rule.substr(0, separator)),
          std::wstring(rule.substr(separator + 1))};
}

bool FuzzyPinyinRuleSelected(const std::vector<std::wstring>& rules,
                             std::wstring_view id) {
  return std::find(rules.begin(), rules.end(), std::wstring(id)) != rules.end();
}

}  // namespace fp::config_winui
