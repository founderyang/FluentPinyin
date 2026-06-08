#pragma once

#include <array>
#include <string_view>

namespace fp::config_winui {

struct WanxiangModeDefinition {
  std::wstring_view title;
  std::wstring_view subtitle;
  std::wstring_view setting_key;
  bool default_value;
  std::wstring_view icon;
  std::wstring_view legacy_key;
};

const std::array<WanxiangModeDefinition, 14>& WanxiangModeDefinitions();

}  // namespace fp::config_winui
