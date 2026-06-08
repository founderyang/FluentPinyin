#pragma once

#include "common/theme.h"

#include <windef.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>

#include <cstdint>

namespace fp::config_winui {

inline constexpr double kSettingsDialogContentWidth = 500.0;
inline constexpr double kSettingsDialogOuterWidth = 548.0;
inline constexpr double kSettingsDialogListHeight = 160.0;
inline constexpr double kSyncProviderDialogFormViewportHeight = 220.0;

void SetSettingsWindowHandle(HWND hwnd);
HWND SettingsWindowHandle();

winrt::Windows::UI::Color ColorFromArgb(uint8_t alpha,
                                        uint8_t red,
                                        uint8_t green,
                                        uint8_t blue);
winrt::Windows::UI::Color ColorFromRgb(uint8_t red, uint8_t green, uint8_t blue);

winrt::Microsoft::UI::Xaml::Media::SolidColorBrush Brush(uint8_t red,
                                                         uint8_t green,
                                                         uint8_t blue);
winrt::Microsoft::UI::Xaml::Media::SolidColorBrush Brush(uint8_t alpha,
                                                         uint8_t red,
                                                         uint8_t green,
                                                         uint8_t blue);
winrt::Microsoft::UI::Xaml::Media::SolidColorBrush Brush(fp::ThemeColor color);
winrt::Microsoft::UI::Xaml::Media::SolidColorBrush TransparentBrush();

winrt::Microsoft::UI::Xaml::Media::FontFamily SettingsUiFontFamily();
void ApplySettingsUIFont(winrt::Microsoft::UI::Xaml::Controls::Control const& control);
void ApplySettingsUIFont(winrt::Microsoft::UI::Xaml::Controls::TextBlock const& text);
void ApplySettingsUIFont(winrt::Microsoft::UI::Xaml::Controls::CheckBox const& check);

winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> ColorReference(
    uint8_t red,
    uint8_t green,
    uint8_t blue);
winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> ColorReference(
    uint8_t alpha,
    uint8_t red,
    uint8_t green,
    uint8_t blue);
winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> ColorReference(
    fp::ThemeColor color);

winrt::Microsoft::UI::Xaml::CornerRadius Radius(double value);
winrt::Microsoft::UI::Xaml::Thickness UniformThickness(double value);
double SettingsHairlineDip();
winrt::Microsoft::UI::Xaml::Thickness SettingsHairlineThickness();

winrt::Microsoft::UI::Xaml::Media::SolidColorBrush SettingsSurfaceBrush();
winrt::Microsoft::UI::Xaml::Media::SolidColorBrush SettingsCardBrush();
fp::ThemeColor SettingsBorderColor();
fp::ThemeColor SettingsBorderHoverColor();
winrt::Microsoft::UI::Xaml::Media::SolidColorBrush SettingsBorderBrush();
winrt::Microsoft::UI::Xaml::Media::SolidColorBrush SettingsBorderHoverBrush();
winrt::Microsoft::UI::Xaml::Media::SolidColorBrush SettingsIconBrush();
winrt::Microsoft::UI::Xaml::Media::SolidColorBrush SettingsTextBrush();
winrt::Microsoft::UI::Xaml::Media::SolidColorBrush SettingsSecondaryTextBrush();
fp::ThemeColor SettingsEdgeColor();
winrt::Microsoft::UI::Xaml::Media::SolidColorBrush SettingsEdgeBrush();
COLORREF SettingsEdgeColorRef();

winrt::Microsoft::UI::Xaml::Controls::Border SettingsFrame(
    winrt::Microsoft::UI::Xaml::UIElement const& child,
    fp::ThemeColor surface,
    winrt::Microsoft::UI::Xaml::CornerRadius radius,
    winrt::Microsoft::UI::Xaml::Thickness content_padding = UniformThickness(0),
    winrt::Microsoft::UI::Xaml::Thickness margin = UniformThickness(0));

winrt::Microsoft::UI::Xaml::ElementTheme CurrentSettingsElementTheme();
winrt::Microsoft::UI::Xaml::ApplicationTheme CurrentSettingsApplicationTheme();
void ApplySettingsResources(
    winrt::Microsoft::UI::Xaml::ResourceDictionary const& resources);

}  // namespace fp::config_winui
