#include "config_winui/status_tip_blacklist.h"

#include "common/constants.h"
#include "common/status_tip_blacklist.h"
#include "config_winui/settings_binding.h"

namespace fp::config_winui {

std::wstring NormalizeStatusTipBlacklistToken(std::wstring_view token) {
  return fp::NormalizeStatusTipBlacklistToken(token);
}

bool StatusTipBlacklistContains(const std::vector<std::wstring>& items,
                                std::wstring_view item) {
  return fp::StatusTipBlacklistContains(items, item);
}

std::vector<std::wstring> ParseStatusTipBlacklistSetting(std::wstring_view value) {
  return fp::ParseStatusTipBlacklistSetting(value);
}

std::vector<std::wstring> CurrentStatusTipBlacklistItems() {
  return ParseStatusTipBlacklistSetting(
      ReadStringSetting(fp::kStatusTipBlacklistSetting, fp::kDefaultStatusTipBlacklist));
}

int CurrentStatusTipBlacklistCount() {
  return static_cast<int>(CurrentStatusTipBlacklistItems().size());
}

std::wstring JoinStatusTipBlacklistItems(const std::vector<std::wstring>& items) {
  return fp::JoinStatusTipBlacklistItems(items);
}

}  // namespace fp::config_winui
