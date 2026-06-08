#include "config_winui/settings_ui_helpers.h"

#include "common/bundled_fonts.h"
#include "config_winui/theme_helpers.h"

#include <windows.h>

#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.Text.h>

#include <algorithm>
#include <string>

namespace fp::config_winui {
namespace {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace Markup = Microsoft::UI::Xaml::Markup;
namespace Shapes = Microsoft::UI::Xaml::Shapes;
using Windows::Foundation::IInspectable;
using Windows::Foundation::IReference;
using Windows::UI::Color;
using Windows::UI::Text::FontWeights;

HWND g_settings_window_hwnd = nullptr;

}  // namespace

void SetSettingsWindowHandle(HWND hwnd) {
  g_settings_window_hwnd = hwnd;
}

HWND SettingsWindowHandle() {
  return g_settings_window_hwnd;
}

Color ColorFromArgb(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) {
  return Color{alpha, red, green, blue};
}

Color ColorFromRgb(uint8_t red, uint8_t green, uint8_t blue) {
  return ColorFromArgb(255, red, green, blue);
}

SolidColorBrush Brush(uint8_t red, uint8_t green, uint8_t blue) {
  return SolidColorBrush(ColorFromRgb(red, green, blue));
}

SolidColorBrush Brush(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) {
  return SolidColorBrush(ColorFromArgb(alpha, red, green, blue));
}

SolidColorBrush Brush(fp::ThemeColor color) {
  return Brush(color.red, color.green, color.blue);
}

SolidColorBrush TransparentBrush() {
  return SolidColorBrush(Windows::UI::Colors::Transparent());
}

FontFamily SettingsUiFontFamily() {
  return FontFamily(fp::kSettingsUiFontFamily);
}

void ApplySettingsUIFont(Control const& control) {
  control.FontFamily(SettingsUiFontFamily());
}

void ApplySettingsUIFont(TextBlock const& text) {
  text.FontFamily(SettingsUiFontFamily());
}

void ApplySettingsUIFont(CheckBox const& check) {
  check.FontFamily(SettingsUiFontFamily());
}

IReference<Color> ColorReference(uint8_t red, uint8_t green, uint8_t blue) {
  return box_value(ColorFromRgb(red, green, blue)).as<IReference<Color>>();
}

IReference<Color> ColorReference(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) {
  return box_value(ColorFromArgb(alpha, red, green, blue)).as<IReference<Color>>();
}

IReference<Color> ColorReference(fp::ThemeColor color) {
  return ColorReference(color.red, color.green, color.blue);
}

CornerRadius Radius(double value) {
  return CornerRadius{value, value, value, value};
}

Thickness UniformThickness(double value) {
  return Thickness{value, value, value, value};
}

double SettingsHairlineDip() {
  UINT dpi = g_settings_window_hwnd != nullptr ? GetDpiForWindow(g_settings_window_hwnd) : 0;
  if (dpi == 0) {
    dpi = GetDpiForSystem();
  }
  dpi = std::max<UINT>(dpi, 96);
  return 96.0 / static_cast<double>(dpi);
}

Thickness SettingsHairlineThickness() {
  return UniformThickness(SettingsHairlineDip());
}

SolidColorBrush SettingsSurfaceBrush() {
  return Brush(CurrentSettingsPalette().background);
}

SolidColorBrush SettingsCardBrush() {
  return Brush(CurrentSettingsPalette().card);
}

fp::ThemeColor SettingsBorderColor() {
  return CurrentSettingsPalette().border;
}

fp::ThemeColor SettingsBorderHoverColor() {
  return CurrentSettingsPalette().edge;
}

SolidColorBrush SettingsBorderBrush() {
  return Brush(SettingsBorderColor());
}

SolidColorBrush SettingsBorderHoverBrush() {
  return Brush(SettingsBorderHoverColor());
}

SolidColorBrush SettingsIconBrush() {
  return Brush(CurrentSettingsPalette().icon);
}

SolidColorBrush SettingsTextBrush() {
  return Brush(CurrentSettingsPalette().text);
}

SolidColorBrush SettingsSecondaryTextBrush() {
  return Brush(CurrentSettingsPalette().secondary_text);
}

fp::ThemeColor SettingsEdgeColor() {
  return CurrentSettingsPalette().border;
}

SolidColorBrush SettingsEdgeBrush() {
  return Brush(SettingsEdgeColor());
}

COLORREF SettingsEdgeColorRef() {
  const fp::ThemeColor color = SettingsEdgeColor();
  return RGB(color.red, color.green, color.blue);
}

Border SettingsFrame(UIElement const& child,
                     fp::ThemeColor surface,
                     CornerRadius radius,
                     Thickness content_padding,
                     Thickness margin) {
  Border frame;
  frame.Background(Brush(surface));
  frame.BorderBrush(SettingsBorderBrush());
  frame.BorderThickness(SettingsHairlineThickness());
  frame.CornerRadius(radius);
  frame.Padding(content_padding);
  frame.Margin(margin);
  frame.HorizontalAlignment(HorizontalAlignment::Stretch);
  frame.VerticalAlignment(VerticalAlignment::Stretch);
  frame.UseLayoutRounding(true);
  frame.Child(child);
  return frame;
}

ElementTheme CurrentSettingsElementTheme() {
  return CurrentSettingsPalette().light ? ElementTheme::Light : ElementTheme::Dark;
}

ApplicationTheme CurrentSettingsApplicationTheme() {
  return CurrentSettingsPalette().light ? ApplicationTheme::Light : ApplicationTheme::Dark;
}

void ApplySettingsResources(ResourceDictionary const& resources) {
  const auto palette = CurrentSettingsPalette();
  const Thickness hairline = SettingsHairlineThickness();
  const auto settings_font = SettingsUiFontFamily().as<IInspectable>();
  const auto border = SettingsBorderBrush().as<IInspectable>();
  const auto border_hover = SettingsBorderHoverBrush().as<IInspectable>();
  const auto surface = Brush(palette.background).as<IInspectable>();
  const auto card = Brush(palette.card).as<IInspectable>();
  const auto text = Brush(palette.text).as<IInspectable>();
  const auto secondary_text = Brush(palette.secondary_text).as<IInspectable>();
  const auto button = Brush(palette.button).as<IInspectable>();
  const auto button_hover = Brush(palette.button_hover).as<IInspectable>();
  const auto button_pressed = Brush(palette.button_pressed).as<IInspectable>();
  const auto accent = Brush(palette.accent).as<IInspectable>();
  const auto transparent = TransparentBrush().as<IInspectable>();
  const auto knob_on =
      Brush(palette.light ? fp::ThemeColor{255, 255, 255} : fp::ThemeColor{0, 0, 0})
          .as<IInspectable>();
  const auto knob_off = Brush(palette.secondary_text).as<IInspectable>();
  resources.Insert(box_value(L"ContentControlThemeFontFamily"), settings_font);
  resources.Insert(box_value(L"TextBlockFontFamily"), settings_font);
  resources.Insert(box_value(L"ButtonFontFamily"), settings_font);
  resources.Insert(box_value(L"CheckBoxFontFamily"), settings_font);
  resources.Insert(box_value(L"ComboBoxFontFamily"), settings_font);
  resources.Insert(box_value(L"ComboBoxItemFontFamily"), settings_font);
  resources.Insert(box_value(L"ContentDialogFontFamily"), settings_font);
  resources.Insert(box_value(L"ContentDialogButtonFontFamily"), settings_font);
  resources.Insert(box_value(L"MTCMediaFontFamily"), settings_font);
  resources.Insert(box_value(L"NavigationViewFontFamily"), settings_font);
  resources.Insert(box_value(L"NavigationViewItemFontFamily"), settings_font);
  resources.Insert(box_value(L"PasswordBoxFontFamily"), settings_font);
  resources.Insert(box_value(L"PhoneFontFamily"), settings_font);
  resources.Insert(box_value(L"PhoneFontFamilyNormal"), settings_font);
  resources.Insert(box_value(L"PhoneFontFamilySemiLight"), settings_font);
  resources.Insert(box_value(L"PivotHeaderItemFontFamily"), settings_font);
  resources.Insert(box_value(L"PivotTitleFontFamily"), settings_font);
  resources.Insert(box_value(L"TextBoxFontFamily"), settings_font);
  resources.Insert(box_value(L"TextControlFontFamily"), settings_font);
  resources.Insert(box_value(L"ToggleSwitchFontFamily"), settings_font);
  resources.Insert(box_value(L"ToolTipFontFamily"), settings_font);
  resources.Insert(box_value(L"ToolTipContentThemeFontFamily"), settings_font);
  resources.Insert(box_value(L"KeyTipFontFamily"), settings_font);
  resources.Insert(box_value(L"ApplicationPageBackgroundThemeBrush"), surface);
  resources.Insert(box_value(L"SolidBackgroundFillColorBaseBrush"), surface);
  resources.Insert(box_value(L"SolidBackgroundFillColorSecondaryBrush"), surface);
  resources.Insert(box_value(L"LayerFillColorDefaultBrush"), surface);
  resources.Insert(box_value(L"LayerFillColorAltBrush"), surface);
  resources.Insert(box_value(L"ControlStrokeColorDefaultBrush"), border);
  resources.Insert(box_value(L"SurfaceStrokeColorDefaultBrush"), border);
  resources.Insert(box_value(L"ButtonBorderThemeThickness"), box_value(hairline));
  resources.Insert(box_value(L"ComboBoxBorderThemeThickness"), box_value(hairline));
  resources.Insert(box_value(L"ComboBoxDropdownBorderThickness"), box_value(hairline));
  resources.Insert(box_value(L"ComboBoxPopupBorderThemeThickness"), box_value(hairline));
  resources.Insert(box_value(L"ContentDialogMaxWidth"), box_value(kSettingsDialogOuterWidth));
  resources.Insert(box_value(L"ContentDialogMinWidth"), box_value(kSettingsDialogOuterWidth));
  resources.Insert(box_value(L"ContentDialogBackground"), card);
  resources.Insert(box_value(L"ContentDialogForeground"), text);
  resources.Insert(box_value(L"ContentDialogBorderBrush"), border);
  resources.Insert(box_value(L"ContentDialogBorderWidth"), box_value(hairline));
  resources.Insert(box_value(L"ContentDialogButtonBackground"), button);
  resources.Insert(box_value(L"ContentDialogButtonBackgroundPointerOver"), button_hover);
  resources.Insert(box_value(L"ContentDialogButtonBackgroundPressed"), button_pressed);
  resources.Insert(box_value(L"ContentDialogButtonForeground"), text);
  resources.Insert(box_value(L"ContentDialogButtonBorderBrush"), border);
  resources.Insert(box_value(L"ContentDialogButtonBorderBrushPointerOver"), border_hover);
  resources.Insert(box_value(L"ContentDialogButtonBorderBrushPressed"), border_hover);
  resources.Insert(box_value(L"ContentDialogPrimaryButtonBackground"), accent);
  resources.Insert(box_value(L"ContentDialogPrimaryButtonBackgroundPointerOver"), accent);
  resources.Insert(box_value(L"ContentDialogPrimaryButtonBackgroundPressed"), accent);
  resources.Insert(
      box_value(L"ContentDialogPrimaryButtonForeground"),
      Brush(palette.light ? fp::ThemeColor{255, 255, 255} : fp::ThemeColor{0, 0, 0})
          .as<IInspectable>());
  resources.Insert(box_value(L"ComboBoxBackground"), button);
  resources.Insert(box_value(L"ComboBoxBackgroundPointerOver"), button_hover);
  resources.Insert(box_value(L"ComboBoxBackgroundPressed"), button_pressed);
  resources.Insert(box_value(L"ComboBoxBackgroundDropDownOpen"), button_pressed);
  resources.Insert(box_value(L"ComboBoxForeground"), text);
  resources.Insert(box_value(L"ComboBoxForegroundPointerOver"), text);
  resources.Insert(box_value(L"ComboBoxForegroundPressed"), text);
  resources.Insert(box_value(L"ComboBoxForegroundDropDownOpen"), text);
  resources.Insert(box_value(L"ComboBoxBorderBrush"), border);
  resources.Insert(box_value(L"ComboBoxBorderBrushPointerOver"), border_hover);
  resources.Insert(box_value(L"ComboBoxBorderBrushPressed"), border_hover);
  resources.Insert(box_value(L"ComboBoxBorderBrushDropDownOpen"), border);
  resources.Insert(box_value(L"ComboBoxDropDownBackground"), card);
  resources.Insert(box_value(L"ComboBoxDropDownBorderBrush"), border);
  resources.Insert(box_value(L"ComboBoxItemBackground"), card);
  resources.Insert(box_value(L"ComboBoxItemBackgroundPointerOver"), button_hover);
  resources.Insert(box_value(L"ComboBoxItemBackgroundPressed"), button_pressed);
  resources.Insert(box_value(L"ComboBoxItemBackgroundSelected"), button_pressed);
  resources.Insert(box_value(L"ComboBoxItemBackgroundSelectedPointerOver"), button_pressed);
  resources.Insert(box_value(L"ComboBoxItemForeground"), text);
  resources.Insert(box_value(L"ComboBoxItemForegroundPointerOver"), text);
  resources.Insert(box_value(L"ComboBoxItemForegroundPressed"), text);
  resources.Insert(box_value(L"ComboBoxItemForegroundSelected"), text);
  resources.Insert(box_value(L"ComboBoxItemPillFillBrush"), transparent);
  resources.Insert(box_value(L"NavigationViewForeground"), text);
  resources.Insert(box_value(L"NavigationViewItemForeground"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundPointerOver"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundPressed"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundDisabled"), secondary_text);
  resources.Insert(box_value(L"NavigationViewItemForegroundChecked"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundCheckedPointerOver"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundCheckedPressed"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundSelected"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundSelectedPointerOver"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundSelectedPressed"), text);
  resources.Insert(box_value(L"NavigationViewItemIconForeground"), text);
  resources.Insert(box_value(L"NavigationViewItemIconForegroundPointerOver"), text);
  resources.Insert(box_value(L"NavigationViewItemIconForegroundPressed"), text);
  resources.Insert(box_value(L"NavigationViewItemIconForegroundSelected"), text);
  resources.Insert(box_value(L"NavigationViewItemIconForegroundSelectedPointerOver"), text);
  resources.Insert(box_value(L"NavigationViewItemBackground"), transparent);
  resources.Insert(box_value(L"NavigationViewItemBackgroundPointerOver"), button_hover);
  resources.Insert(box_value(L"NavigationViewItemBackgroundPressed"), button_pressed);
  resources.Insert(box_value(L"NavigationViewItemBackgroundChecked"), button);
  resources.Insert(box_value(L"NavigationViewItemBackgroundCheckedPointerOver"), button_hover);
  resources.Insert(box_value(L"NavigationViewItemBackgroundCheckedPressed"), button_pressed);
  resources.Insert(box_value(L"NavigationViewItemBackgroundSelected"), button);
  resources.Insert(box_value(L"NavigationViewItemBackgroundSelectedPointerOver"), button_hover);
  resources.Insert(box_value(L"NavigationViewItemBackgroundSelectedPressed"), button_pressed);
  resources.Insert(box_value(L"NavigationViewItemBorderBrush"), transparent);
  resources.Insert(box_value(L"NavigationViewItemBorderBrushPointerOver"), transparent);
  resources.Insert(box_value(L"NavigationViewItemBorderBrushPressed"), transparent);
  resources.Insert(box_value(L"NavigationViewItemBorderBrushChecked"), transparent);
  resources.Insert(box_value(L"NavigationViewItemBorderBrushSelected"), transparent);
  resources.Insert(box_value(L"NavigationViewSelectionIndicatorForeground"), accent);
  resources.Insert(box_value(L"ToggleSwitchContentForeground"), text);
  resources.Insert(box_value(L"ToggleSwitchContentForegroundDisabled"), secondary_text);
  resources.Insert(box_value(L"ToggleSwitchHeaderForeground"), text);
  resources.Insert(box_value(L"ToggleSwitchHeaderForegroundDisabled"), secondary_text);
  resources.Insert(box_value(L"ToggleSwitchContainerBackground"), transparent);
  resources.Insert(box_value(L"ToggleSwitchContainerBackgroundPointerOver"),
                   ColorReference(0, 0, 0, 0));
  resources.Insert(box_value(L"ToggleSwitchContainerBackgroundPressed"),
                   ColorReference(0, 0, 0, 0));
  resources.Insert(box_value(L"ToggleSwitchContainerBackgroundDisabled"),
                   ColorReference(0, 0, 0, 0));
  resources.Insert(box_value(L"ToggleSwitchFillOff"), button);
  resources.Insert(box_value(L"ToggleSwitchFillOffPointerOver"),
                   ColorReference(palette.button_hover));
  resources.Insert(box_value(L"ToggleSwitchFillOffPressed"),
                   ColorReference(palette.button_pressed));
  resources.Insert(box_value(L"ToggleSwitchFillOffDisabled"),
                   ColorReference(palette.button));
  resources.Insert(box_value(L"ToggleSwitchStrokeOff"), border);
  resources.Insert(box_value(L"ToggleSwitchStrokeOffPointerOver"),
                   ColorReference(SettingsBorderHoverColor()));
  resources.Insert(box_value(L"ToggleSwitchStrokeOffPressed"),
                   ColorReference(SettingsBorderHoverColor()));
  resources.Insert(box_value(L"ToggleSwitchStrokeOffDisabled"),
                   ColorReference(SettingsBorderColor()));
  resources.Insert(box_value(L"ToggleSwitchOuterBorderStrokeThickness"),
                   box_value(SettingsHairlineDip()));
  resources.Insert(box_value(L"ToggleSwitchFillOn"), accent);
  resources.Insert(box_value(L"ToggleSwitchFillOnPointerOver"), accent);
  resources.Insert(box_value(L"ToggleSwitchFillOnPressed"), accent);
  resources.Insert(box_value(L"ToggleSwitchFillOnDisabled"), button_pressed);
  resources.Insert(box_value(L"ToggleSwitchStrokeOn"), accent);
  resources.Insert(box_value(L"ToggleSwitchStrokeOnPointerOver"), accent);
  resources.Insert(box_value(L"ToggleSwitchStrokeOnPressed"), accent);
  resources.Insert(box_value(L"ToggleSwitchStrokeOnDisabled"), button_pressed);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOff"), knob_off);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOffPointerOver"), knob_off);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOffPressed"), knob_off);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOffDisabled"), secondary_text);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOn"), knob_on);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOnPointerOver"), knob_on);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOnPressed"), knob_on);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOnDisabled"), secondary_text);
  resources.Insert(box_value(L"TextControlForeground"), text);
  resources.Insert(box_value(L"TextControlForegroundPointerOver"), text);
  resources.Insert(box_value(L"TextControlForegroundFocused"), text);
  resources.Insert(box_value(L"TextControlPlaceholderForeground"), secondary_text);
  resources.Insert(box_value(L"TextControlPlaceholderForegroundPointerOver"), secondary_text);
  resources.Insert(box_value(L"TextControlPlaceholderForegroundFocused"), secondary_text);
  resources.Insert(box_value(L"TextControlHeaderForeground"), text);
  resources.Insert(box_value(L"TextControlBackground"), button);
  resources.Insert(box_value(L"TextControlBackgroundPointerOver"), button_hover);
  resources.Insert(box_value(L"TextControlBackgroundFocused"), button_pressed);
  resources.Insert(box_value(L"TextControlBorderBrush"), border);
  resources.Insert(box_value(L"TextControlBorderBrushPointerOver"), border_hover);
  resources.Insert(box_value(L"TextControlBorderBrushFocused"), border_hover);
  resources.Insert(box_value(L"TextControlBorderBrushDisabled"), border);
  resources.Insert(box_value(L"TextControlBorderThemeThickness"), box_value(hairline));
  resources.Insert(box_value(L"TextControlBorderThemeThicknessFocused"), box_value(hairline));
  resources.Insert(box_value(L"TextControlButtonForeground"), secondary_text);
  resources.Insert(box_value(L"TextControlSelectionHighlightColor"), accent);
  resources.Insert(box_value(L"NavigationViewDefaultPaneBackground"), surface);
  resources.Insert(box_value(L"NavigationViewContentBackground"), surface);
}

TextBlock Text(std::wstring_view value, double size, int weight) {
  TextBlock text;
  text.Text(value);
  ApplySettingsUIFont(text);
  text.FontSize(size);
  text.Foreground(SettingsTextBrush());
  text.TextWrapping(TextWrapping::Wrap);
  if (weight >= FW_SEMIBOLD) {
    text.FontWeight(FontWeights::SemiBold());
  }
  return text;
}

ToolTip SettingsToolTip(std::wstring_view value) {
  ToolTip tooltip;
  ApplySettingsUIFont(tooltip);
  tooltip.RequestedTheme(CurrentSettingsElementTheme());
  tooltip.Content(Text(value, 12));
  ApplySettingsResources(tooltip.Resources());
  return tooltip;
}

void SetSettingsToolTip(DependencyObject const& target, std::wstring_view value) {
  ToolTipService::SetToolTip(target, SettingsToolTip(value));
}

void ApplySettingsDialogBase(ContentDialog const& dialog, XamlRoot const& xaml_root) {
  dialog.XamlRoot(xaml_root);
  dialog.RequestedTheme(CurrentSettingsElementTheme());
  dialog.Title(nullptr);
  dialog.FontFamily(SettingsUiFontFamily());
  ApplySettingsResources(dialog.Resources());
}

void PrepareIconElement(FrameworkElement const& element, double host_size) {
  element.Width(host_size);
  element.Height(host_size);
  element.MinWidth(host_size);
  element.MinHeight(host_size);
  element.HorizontalAlignment(HorizontalAlignment::Center);
  element.VerticalAlignment(VerticalAlignment::Center);
  element.UseLayoutRounding(true);
}

FontIcon Icon(std::wstring_view glyph, double size) {
  FontIcon icon;
  icon.Glyph(glyph);
  icon.FontFamily(FontFamily(L"Segoe Fluent Icons"));
  icon.FontSize(size);
  icon.Foreground(SettingsIconBrush());
  icon.FontWeight(FontWeights::Normal());
  PrepareIconElement(icon, kSettingIconHostSize);
  return icon;
}

TextBlock TextIcon(std::wstring_view value, double size) {
  auto icon = Text(value, size, FW_SEMIBOLD);
  icon.Foreground(SettingsIconBrush());
  PrepareIconElement(icon, kSettingIconHostSize);
  icon.TextAlignment(TextAlignment::Center);
  icon.TextWrapping(TextWrapping::NoWrap);
  icon.LineHeight(kSettingIconHostSize);
  icon.LineStackingStrategy(LineStackingStrategy::BlockLineHeight);
  return icon;
}

Grid IconHost(UIElement const& icon_content) {
  Grid host;
  PrepareIconElement(host, kSettingIconHostSize);
  host.Children().Append(icon_content);
  return host;
}

Shapes::Path PathShape(std::wstring_view data,
                       double size,
                       double view_box_size,
                       double scale,
                       double dx,
                       double dy,
                       double scale_y) {
  auto xaml = L"<Path xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
              L"Data=\"" +
              std::wstring(data) +
              L"\" Fill=\"#FFF2F2F2\" Stretch=\"None\" Width=\"" +
              std::to_wstring(view_box_size) + L"\" Height=\"" +
              std::to_wstring(view_box_size) + L"\"/>";
  auto path = Markup::XamlReader::Load(xaml).as<Shapes::Path>();
  path.Fill(SettingsIconBrush());
  path.HorizontalAlignment(HorizontalAlignment::Center);
  path.VerticalAlignment(VerticalAlignment::Center);
  path.UseLayoutRounding(true);
  path.RenderTransformOrigin(Windows::Foundation::Point{0.5f, 0.5f});
  CompositeTransform transform;
  const double normalized_scale = scale * size / view_box_size;
  transform.ScaleX(normalized_scale);
  transform.ScaleY((scale_y > 0.0 ? scale_y : scale) * size / view_box_size);
  transform.TranslateX(size * dx);
  transform.TranslateY(size * dy);
  path.RenderTransform(transform);
  return path;
}

Shapes::Path RawPathShape(std::wstring_view data) {
  auto xaml = L"<Path xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
              L"Data=\"" +
              std::wstring(data) + L"\" Fill=\"#FFF2F2F2\" Stretch=\"None\"/>";
  auto path = Markup::XamlReader::Load(xaml).as<Shapes::Path>();
  path.Fill(SettingsIconBrush());
  path.UseLayoutRounding(true);
  return path;
}

Canvas ScaledIconCanvas(double size, double view_box_size, double scale) {
  Canvas canvas;
  canvas.Width(view_box_size);
  canvas.Height(view_box_size);
  canvas.HorizontalAlignment(HorizontalAlignment::Center);
  canvas.VerticalAlignment(VerticalAlignment::Center);
  canvas.UseLayoutRounding(true);
  canvas.RenderTransformOrigin(Windows::Foundation::Point{0.5f, 0.5f});
  CompositeTransform transform;
  const double normalized_scale = scale * size / view_box_size;
  transform.ScaleX(normalized_scale);
  transform.ScaleY(normalized_scale);
  canvas.RenderTransform(transform);
  return canvas;
}

void SetIconChild(Grid const& root, UIElement const& child) {
  root.Children().Clear();
  root.Children().Append(child);
}

Shapes::Ellipse EllipseShape(double left,
                             double top,
                             double width,
                             double height,
                             SolidColorBrush const& fill) {
  Shapes::Ellipse ellipse;
  ellipse.Width(width);
  ellipse.Height(height);
  ellipse.Fill(fill);
  ellipse.UseLayoutRounding(true);
  Canvas::SetLeft(ellipse, left);
  Canvas::SetTop(ellipse, top);
  return ellipse;
}

}  // namespace fp::config_winui
