#pragma once

#include <windef.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <string_view>

namespace fp::config_winui {

void ShowManagedDictionariesDialog(
    winrt::Microsoft::UI::Xaml::XamlRoot const& xaml_root,
    HWND owner,
    winrt::Microsoft::UI::Xaml::Controls::Grid const& state_icon);
void ShowPhraseLexiconDialog(
    winrt::Microsoft::UI::Xaml::XamlRoot const& xaml_root,
    std::wstring_view title_text,
    std::wstring_view description_text,
    bool user_lexicon,
    winrt::Microsoft::UI::Xaml::Controls::Grid const& state_icon);

}  // namespace fp::config_winui
