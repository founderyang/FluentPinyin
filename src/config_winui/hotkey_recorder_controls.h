#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <string_view>

namespace fp::config_winui {

winrt::Microsoft::UI::Xaml::Controls::Button HotkeyRecorderButton(
    std::wstring_view key,
    std::wstring_view fallback);
void SetHotkeyRecorderDisplay(
    winrt::Microsoft::UI::Xaml::Controls::Button const& button,
    std::wstring_view key,
    std::wstring_view display);

}  // namespace fp::config_winui
