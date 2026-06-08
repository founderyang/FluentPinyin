#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace fp::config_winui {

winrt::Microsoft::UI::Xaml::Controls::Border Card(
    winrt::Microsoft::UI::Xaml::UIElement const& child);
winrt::Microsoft::UI::Xaml::Controls::Border StatusBadge(std::wstring_view status);
winrt::Microsoft::UI::Xaml::Controls::StackPanel PageShell(
    std::wstring_view title,
    std::wstring_view subtitle);
winrt::Microsoft::UI::Xaml::Controls::TextBlock SectionHeader(
    std::wstring_view title,
    bool first = false);
winrt::Microsoft::UI::Xaml::Controls::Border SettingIconBackdrop(
    winrt::Microsoft::UI::Xaml::UIElement const& icon_content);
winrt::Microsoft::UI::Xaml::Controls::Border SettingRowWithIcon(
    std::wstring_view title,
    std::wstring_view subtitle,
    winrt::Microsoft::UI::Xaml::UIElement const& control,
    winrt::Microsoft::UI::Xaml::UIElement const& icon_content,
    std::wstring_view status = L"",
    double control_width = 140.0);
winrt::Microsoft::UI::Xaml::Controls::Border SettingRow(
    std::wstring_view title,
    std::wstring_view subtitle,
    winrt::Microsoft::UI::Xaml::UIElement const& control,
    std::wstring_view glyph,
    std::wstring_view status = L"");
winrt::Microsoft::UI::Xaml::Controls::Border SettingWideRowWithIcon(
    std::wstring_view title,
    std::wstring_view subtitle,
    winrt::Microsoft::UI::Xaml::UIElement const& control,
    winrt::Microsoft::UI::Xaml::UIElement const& icon_content,
    std::wstring_view status = L"");
winrt::Microsoft::UI::Xaml::Controls::Border SettingWideRow(
    std::wstring_view title,
    std::wstring_view subtitle,
    winrt::Microsoft::UI::Xaml::UIElement const& control,
    std::wstring_view glyph,
    std::wstring_view status = L"");
void ConfigureSettingCombo(winrt::Microsoft::UI::Xaml::Controls::ComboBox const& combo);
winrt::Microsoft::UI::Xaml::Controls::ComboBoxItem ThemeComboItem(std::wstring_view label);
winrt::Microsoft::UI::Xaml::Controls::ComboBox ChoiceCombo(
    const std::vector<std::wstring>& labels,
    int current_index,
    std::function<void(int)> on_select);
winrt::Microsoft::UI::Xaml::Controls::Button ActionButton(
    std::wstring_view text,
    std::wstring_view glyph);
winrt::Microsoft::UI::Xaml::Controls::Button ActionPathButton(
    std::wstring_view text,
    std::wstring_view path,
    double scale = 0.88,
    double view_box_size = 20.0);
winrt::Microsoft::UI::Xaml::Controls::Button CompactActionButton(
    std::wstring_view text,
    std::wstring_view glyph);
winrt::Microsoft::UI::Xaml::Controls::Button CompactPathActionButton(
    std::wstring_view text,
    std::wstring_view path,
    double scale = 0.82);
winrt::Microsoft::UI::Xaml::Controls::Button InlinePathActionButton(
    std::wstring_view text,
    std::wstring_view path,
    double scale = 0.68);
winrt::Microsoft::UI::Xaml::Controls::Border StableInlinePathActionButton(
    std::wstring_view text,
    std::wstring_view path,
    double scale,
    std::function<void()> on_click);
winrt::Microsoft::UI::Xaml::Controls::Border StableIconToolButton(
    std::wstring_view path,
    std::wstring_view tooltip,
    double scale,
    std::function<void()> on_click,
    double width = 36.0,
    double height = 36.0);
winrt::Microsoft::UI::Xaml::Controls::Button IconToolButton(
    std::wstring_view path,
    std::wstring_view tooltip,
    double scale = 1.0);
winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem NavItem(
    std::wstring_view title,
    std::wstring_view tag,
    std::wstring_view glyph);

}  // namespace fp::config_winui
