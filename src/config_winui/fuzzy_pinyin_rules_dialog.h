#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <memory>

namespace fp::config_winui {

void ShowFuzzyPinyinRulesDialog(
    winrt::Microsoft::UI::Xaml::XamlRoot const& xaml_root,
    winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch const& fuzzy_switch,
    winrt::Microsoft::UI::Xaml::Controls::Grid const& state_icon,
    bool require_enabled,
    std::shared_ptr<bool> suppress_toggle_dialog);

}  // namespace fp::config_winui
