#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include <functional>

namespace fp::config_winui {

void ShowThemePresetDialog(
    winrt::Microsoft::UI::Xaml::XamlRoot const& xaml_root,
    std::function<void()> on_applied);

}  // namespace fp::config_winui
