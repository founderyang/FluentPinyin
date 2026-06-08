#include "config_winui/status_tip_blacklist.h"

#include "common/constants.h"
#include "common/encoding.h"
#include "config_winui/settings_binding.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace fp::config_winui {
namespace {

std::wstring TrimSettingToken(std::wstring_view value) {
  return fp::TrimWhitespace(value);
}

std::wstring ToLowerSettingToken(std::wstring value) {
  return fp::ToLowerInvariant(std::move(value));
}

}  // namespace

std::wstring NormalizeStatusTipBlacklistToken(std::wstring_view token) {
  std::wstring normalized = TrimSettingToken(token);
  if (normalized.size() >= 2 &&
      ((normalized.front() == L'"' && normalized.back() == L'"') ||
       (normalized.front() == L'\'' && normalized.back() == L'\''))) {
    normalized = TrimSettingToken(
        std::wstring_view(normalized).substr(1, normalized.size() - 2));
  }
  if (normalized.empty()) {
    return {};
  }
  if (normalized.find_first_of(L"*?") == std::wstring::npos &&
      normalized.find_first_of(L"\\/") != std::wstring::npos) {
    normalized = std::filesystem::path(normalized).filename().wstring();
  }
  return TrimSettingToken(normalized);
}

bool StatusTipBlacklistContains(const std::vector<std::wstring>& items,
                                std::wstring_view item) {
  const std::wstring normalized = ToLowerSettingToken(std::wstring(item));
  return std::any_of(items.begin(), items.end(), [&normalized](const auto& existing) {
    return ToLowerSettingToken(existing) == normalized;
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

std::vector<std::wstring> CurrentStatusTipBlacklistItems() {
  return ParseStatusTipBlacklistSetting(
      ReadStringSetting(fp::kStatusTipBlacklistSetting, fp::kDefaultStatusTipBlacklist));
}

int CurrentStatusTipBlacklistCount() {
  return static_cast<int>(CurrentStatusTipBlacklistItems().size());
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

}  // namespace fp::config_winui
