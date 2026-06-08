#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include <functional>

namespace fp::config_winui {

winrt::Microsoft::UI::Xaml::UIElement BuildGeneralPage(
    winrt::Microsoft::UI::Xaml::XamlRoot const& xaml_root,
    std::function<void()> reset_defaults);

}  // namespace fp::config_winui
