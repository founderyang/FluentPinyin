#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include <functional>

namespace fp::config_winui {

winrt::Microsoft::UI::Xaml::UIElement BuildAppearancePage(
    winrt::Microsoft::UI::Xaml::XamlRoot const& xaml_root,
    std::function<void()> show_theme_preset,
    std::function<void()> apply_theme_refresh);

}  // namespace fp::config_winui
