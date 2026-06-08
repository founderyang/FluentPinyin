#pragma once

#include <string>
#include <string_view>

namespace fp::config_winui {

std::wstring ReadCandidateLayoutSetting(
    std::wstring_view default_value);
bool WriteCandidateLayoutSetting(std::wstring_view value);

}  // namespace fp::config_winui
