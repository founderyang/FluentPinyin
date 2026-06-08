#pragma once

#include <windef.h>
#include <winrt/Microsoft.UI.Xaml.h>

namespace fp::config_winui {

winrt::Microsoft::UI::Xaml::UIElement BuildLexiconPage(
    HWND owner,
    winrt::Microsoft::UI::Xaml::XamlRoot const& xaml_root);

}  // namespace fp::config_winui
