#pragma once

#include "common/theme.h"

#include <windef.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>

#include <cstdint>
#include <string_view>

namespace fp::config_winui {

inline constexpr double kSettingsDialogContentWidth = 500.0;
inline constexpr double kSettingsDialogOuterWidth = 548.0;
inline constexpr double kSettingsDialogListHeight = 160.0;
inline constexpr double kSyncProviderDialogFormViewportHeight = 220.0;
inline constexpr double kSettingIconBackdropSize = 36.0;
inline constexpr double kSettingIconBackdropRadius = 8.0;
inline constexpr double kSettingIconHostSize = 22.0;
inline constexpr double kSettingIconVisualSize = 18.0;
inline constexpr double kInlineButtonIconHostSize = 16.0;
inline constexpr double kActionButtonIconHostSize = 18.0;

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

winrt::Microsoft::UI::Xaml::Controls::TextBlock Text(std::wstring_view value,
                                                      double size,
                                                      int weight = 400);
winrt::Microsoft::UI::Xaml::Controls::ToolTip SettingsToolTip(
    std::wstring_view value);
void SetSettingsToolTip(
    winrt::Microsoft::UI::Xaml::DependencyObject const& target,
    std::wstring_view value);
void ApplySettingsDialogBase(
    winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& dialog,
    winrt::Microsoft::UI::Xaml::XamlRoot const& xaml_root);
void PrepareIconElement(
    winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
    double host_size = kSettingIconHostSize);
winrt::Microsoft::UI::Xaml::Controls::FontIcon Icon(
    std::wstring_view glyph,
    double size = kSettingIconVisualSize);
winrt::Microsoft::UI::Xaml::Controls::TextBlock TextIcon(
    std::wstring_view value,
    double size = 15.0);
winrt::Microsoft::UI::Xaml::Controls::Grid IconHost(
    winrt::Microsoft::UI::Xaml::UIElement const& icon_content);
winrt::Microsoft::UI::Xaml::Shapes::Path PathShape(std::wstring_view data,
                                                    double size,
                                                    double view_box_size,
                                                    double scale = 1.0,
                                                    double dx = 0.0,
                                                    double dy = 0.0,
                                                    double scale_y = 0.0);
winrt::Microsoft::UI::Xaml::Shapes::Path RawPathShape(std::wstring_view data);
winrt::Microsoft::UI::Xaml::Controls::Canvas ScaledIconCanvas(
    double size,
    double view_box_size,
    double scale = 1.0);
void SetIconChild(
    winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
    winrt::Microsoft::UI::Xaml::UIElement const& child);
winrt::Microsoft::UI::Xaml::Shapes::Ellipse EllipseShape(
    double left,
    double top,
    double width,
    double height,
    winrt::Microsoft::UI::Xaml::Media::SolidColorBrush const& fill);

}  // namespace fp::config_winui
