#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace fp {

std::wstring NormalizeStatusTipBlacklistToken(std::wstring_view token);
bool StatusTipBlacklistContains(const std::vector<std::wstring>& items,
                                std::wstring_view item);
std::vector<std::wstring> ParseStatusTipBlacklistSetting(std::wstring_view value);
std::wstring JoinStatusTipBlacklistItems(const std::vector<std::wstring>& items);
bool StatusTipProcessNameMatchesPattern(std::wstring process_name, std::wstring pattern);
bool StatusTipProcessNameInList(std::wstring_view process_name, std::wstring_view list);

}  // namespace fp
