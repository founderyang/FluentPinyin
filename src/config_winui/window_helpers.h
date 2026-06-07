#pragma once

#undef GetCurrentTime

#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Graphics.h>

#include <string_view>

namespace fp::config_winui {

HWND GetWindowHandle(winrt::Microsoft::UI::Xaml::Window const& window);
bool ActivateWindowByTitle(std::wstring_view title, int attempts = 1);
winrt::Windows::Graphics::SizeInt32 DefaultWindowSize(
    winrt::Microsoft::UI::Xaml::Window const& window);
winrt::Windows::Graphics::SizeInt32 ScaleSizeForDpi(
    winrt::Windows::Graphics::SizeInt32 const& size,
    UINT dpi);
void CenterWindowOnMonitor(winrt::Microsoft::UI::Xaml::Window const& window);

}  // namespace fp::config_winui
