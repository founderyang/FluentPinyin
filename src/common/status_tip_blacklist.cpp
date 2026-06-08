#include "common/status_tip_blacklist.h"

#include "common/encoding.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace fp {
namespace {

std::wstring TrimToken(std::wstring_view value) {
  return fp::TrimWhitespace(value);
}

std::wstring LowerToken(std::wstring value) {
  return fp::ToLowerInvariant(std::move(value));
}

bool WildcardMatch(std::wstring_view text, std::wstring_view pattern) {
  size_t text_index = 0;
  size_t pattern_index = 0;
  size_t star_index = std::wstring_view::npos;
  size_t star_text_index = 0;

  while (text_index < text.size()) {
    if (pattern_index < pattern.size() &&
        (pattern[pattern_index] == L'?' || pattern[pattern_index] == text[text_index])) {
      ++text_index;
      ++pattern_index;
      continue;
    }
    if (pattern_index < pattern.size() && pattern[pattern_index] == L'*') {
      star_index = pattern_index++;
      star_text_index = text_index;
      continue;
    }
    if (star_index != std::wstring_view::npos) {
      pattern_index = star_index + 1;
      text_index = ++star_text_index;
      continue;
    }
    return false;
  }
  while (pattern_index < pattern.size() && pattern[pattern_index] == L'*') {
    ++pattern_index;
  }
  return pattern_index == pattern.size();
}

}  // namespace

std::wstring NormalizeStatusTipBlacklistToken(std::wstring_view token) {
  std::wstring normalized = TrimToken(token);
  if (normalized.size() >= 2 &&
      ((normalized.front() == L'"' && normalized.back() == L'"') ||
       (normalized.front() == L'\'' && normalized.back() == L'\''))) {
    normalized =
        TrimToken(std::wstring_view(normalized).substr(1, normalized.size() - 2));
  }
  if (normalized.empty()) {
    return {};
  }
  if (normalized.find_first_of(L"*?") == std::wstring::npos &&
      normalized.find_first_of(L"\\/") != std::wstring::npos) {
    normalized = std::filesystem::path(normalized).filename().wstring();
  }
  return TrimToken(normalized);
}

bool StatusTipBlacklistContains(const std::vector<std::wstring>& items,
                                std::wstring_view item) {
  const std::wstring normalized = LowerToken(std::wstring(item));
  return std::any_of(items.begin(), items.end(), [&normalized](const auto& existing) {
    return LowerToken(existing) == normalized;
  });
}

std::vector<std::wstring> ParseStatusTipBlacklistSetting(std::wstring_view value) {
  std::vector<std::wstring> result;
  std::wstring token;
  auto append_token = [&]() {
    const std::wstring normalized = NormalizeStatusTipBlacklistToken(token);
    if (!normalized.empty() && !StatusTipBlacklistContains(result, normalized)) {
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

std::wstring JoinStatusTipBlacklistItems(const std::vector<std::wstring>& items) {
  std::wstring value;
  for (const auto& item : items) {
    const std::wstring normalized = NormalizeStatusTipBlacklistToken(item);
    if (normalized.empty()) {
      continue;
    }
    if (!value.empty()) {
      value.push_back(L',');
    }
    value += normalized;
  }
  return value;
}

bool StatusTipProcessNameMatchesPattern(std::wstring process_name, std::wstring pattern) {
  process_name = LowerToken(TrimToken(process_name));
  pattern = LowerToken(TrimToken(pattern));
  if (process_name.empty() || pattern.empty()) {
    return false;
  }
  if (pattern.find_first_of(L"*?") != std::wstring::npos) {
    return WildcardMatch(process_name, pattern);
  }
  if (process_name == pattern) {
    return true;
  }
  if (pattern.find(L'.') == std::wstring::npos) {
    const std::wstring exe_pattern = pattern + L".exe";
    return process_name == exe_pattern;
  }
  return false;
}

bool StatusTipProcessNameInList(std::wstring_view process_name, std::wstring_view list) {
  for (const auto& pattern : ParseStatusTipBlacklistSetting(list)) {
    if (StatusTipProcessNameMatchesPattern(std::wstring(process_name), pattern)) {
      return true;
    }
  }
  return false;
}

}  // namespace fp
