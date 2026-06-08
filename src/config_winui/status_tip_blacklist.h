#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace fp::config_winui {

std::wstring NormalizeStatusTipBlacklistToken(std::wstring_view token);
bool StatusTipBlacklistContains(const std::vector<std::wstring>& items,
                                std::wstring_view item);
std::vector<std::wstring> ParseStatusTipBlacklistSetting(std::wstring_view value);
std::vector<std::wstring> CurrentStatusTipBlacklistItems();
int CurrentStatusTipBlacklistCount();
std::wstring JoinStatusTipBlacklistItems(const std::vector<std::wstring>& items);

}  // namespace fp::config_winui
