#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <functional>

namespace fp::config_winui {

winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch ToolbarVisibleSwitch(
    std::function<void(bool)> on_change = {});
void RegisterToolbarVisibleSwitch(
    winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch const& toggle);
void ClearToolbarVisibleSwitches();
void SyncToolbarVisibleSwitchesFromSettings();

}  // namespace fp::config_winui
