#pragma once

#include "config_winui/settings_options.h"
#include "config_winui/wanxiang_modes.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fp::config_winui {

winrt::Microsoft::UI::Xaml::Controls::ComboBox CandidateCountCombo();
winrt::Microsoft::UI::Xaml::Controls::ComboBox CandidateCountCombo(
    winrt::Microsoft::UI::Xaml::Controls::TextBlock const& icon_label);
winrt::Microsoft::UI::Xaml::Controls::ComboBox CandidateFontSizeCombo();
winrt::Microsoft::UI::Xaml::Controls::ComboBox CandidateFontFamilyCombo(
    winrt::Microsoft::UI::Xaml::Controls::TextBlock const& icon_label);
winrt::Microsoft::UI::Xaml::Controls::ComboBox IntChoiceCombo(
    std::wstring_view key,
    const std::vector<std::wstring>& labels,
    int default_value,
    int min_value,
    int max_value);
winrt::Microsoft::UI::Xaml::Controls::ComboBox IntChoiceCombo(
    std::wstring_view key,
    const std::vector<std::wstring>& labels,
    int default_value,
    int min_value,
    int max_value,
    winrt::Microsoft::UI::Xaml::Controls::TextBlock const& icon_label);
winrt::Microsoft::UI::Xaml::Controls::ComboBox StringChoiceCombo(
    std::wstring_view key,
    const std::vector<std::pair<std::wstring, std::wstring>>& choices,
    std::wstring_view default_value,
    std::wstring_view initial_value = L"",
    bool restart_input_core_on_change = false,
    std::function<void(std::wstring_view)> on_change = {});
winrt::Microsoft::UI::Xaml::Controls::ComboBox StringChoiceComboWithIcon(
    std::wstring_view key,
    const std::vector<std::pair<std::wstring, std::wstring>>& choices,
    std::wstring_view default_value,
    winrt::Microsoft::UI::Xaml::Controls::TextBlock const& icon_label,
    std::wstring_view initial_value = L"",
    bool restart_input_core_on_change = false,
    std::function<std::wstring(std::wstring_view)> icon_text = {},
    std::function<void(std::wstring_view)> on_change = {});
winrt::Microsoft::UI::Xaml::Controls::ComboBox BoolChoiceCombo(
    std::wstring_view key,
    const std::vector<std::pair<std::wstring, bool>>& choices,
    bool default_value,
    std::function<void(bool)> on_change = {});
winrt::Microsoft::UI::Xaml::Controls::ComboBox InputSchemeCombo(
    winrt::Microsoft::UI::Xaml::Controls::ComboBox const& double_scheme_combo,
    std::function<void(std::wstring_view)> on_change = {});
winrt::Microsoft::UI::Xaml::Controls::ComboBox InputSchemeCombo(
    winrt::Microsoft::UI::Xaml::Controls::ComboBox const& double_scheme_combo,
    winrt::Microsoft::UI::Xaml::Controls::TextBlock const& icon_label,
    std::function<void(std::wstring_view)> on_change = {});
winrt::Microsoft::UI::Xaml::Controls::TextBox SettingTextBox(
    std::wstring_view key,
    std::wstring_view placeholder = L"",
    double min_width = 220,
    std::wstring_view default_value = L"");
winrt::Microsoft::UI::Xaml::Controls::TextBox SettingTextBox(
    std::wstring_view key,
    std::wstring_view placeholder,
    double min_width,
    std::wstring_view default_value,
    winrt::Microsoft::UI::Xaml::Controls::TextBlock const& icon_label);
winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch SettingSwitch(
    std::wstring_view key,
    bool default_value,
    std::function<void(bool)> on_change = {});
winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch SettingSwitch(
    std::wstring_view key,
    bool default_value,
    winrt::Microsoft::UI::Xaml::Controls::TextBlock const& icon_label,
    std::function<std::wstring(bool)> icon_text = {},
    std::function<void(bool)> on_change = {});
void RequestPinyinConfigRestart(bool enabled);
void RequestRimeOptionRefresh(bool enabled);
winrt::Microsoft::UI::Xaml::UIElement WanxiangModeIcon(std::wstring_view value);
winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch RimeConfigSwitch(
    std::wstring_view key,
    bool default_value);
winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch WanxiangModeSwitch(
    const WanxiangModeDefinition& mode);

}  // namespace fp::config_winui
