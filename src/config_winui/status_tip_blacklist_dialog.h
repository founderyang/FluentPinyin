#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

namespace fp::config_winui {

void ShowStatusTipBlacklistDialog(
    winrt::Microsoft::UI::Xaml::XamlRoot const& xaml_root,
    winrt::Microsoft::UI::Xaml::Controls::TextBlock const& count_icon);

}  // namespace fp::config_winui
