#include "config_winui/settings_icons.h"

#include "common/constants.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/theme_helpers.h"

#include <windows.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.UI.Text.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace fp::config_winui {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
namespace Shapes = Microsoft::UI::Xaml::Shapes;
using Windows::UI::Text::FontWeights;

Grid ShapeStatusIcon(bool full_shape) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);

  root.Children().Append(full_shape
                             ? PathShape(kFluentCircle20FilledPath, kSettingIconVisualSize, 20, 0.86)
                             : PathShape(kFluentWeatherMoon24Path,
                                         kSettingIconVisualSize,
                                         24,
                                         0.90,
                                         -0.040,
                                         0.005,
                                         0.98));
  return root;
}

Grid PunctuationStatusIcon(bool chinese_punctuation) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);

  auto icon = ScaledIconCanvas(kSettingIconVisualSize, 52, 0.96);
  if (chinese_punctuation) {
    auto ring_outer =
        EllipseShape(20.2 - 8.0, 10.2 - 5.0, 14.2, 14.2, SettingsIconBrush());
    auto ring_inner = EllipseShape(23.5 - 8.0,
                                   13.6 - 5.0,
                                   7.7,
                                   7.7,
                                   Brush(CurrentSettingsPalette().icon_backdrop));
    auto comma = RawPathShape(L"M46.3 23.9C42.5 23.9 40.9 26.4 40.9 30.6C40.9 34.7 44.5 36.8 48.2 36C47.5 39 44.6 44 39.6 49H42C49.6 43.5 54 37.7 54 30.9C54 26.4 50.4 24 46.3 23.9Z");
    Canvas::SetLeft(comma, -8.0);
    Canvas::SetTop(comma, -5.0);
    icon.Children().Append(ring_outer);
    icon.Children().Append(ring_inner);
    icon.Children().Append(comma);
  } else {
    auto dot = EllipseShape(20.0 - 8.0, 10.5 - 5.0, 14.0, 13.5, SettingsIconBrush());
    auto comma = RawPathShape(L"M43 26.8H50.9L48.6 34L45.2 45.6H39.2L42.4 26.8H43Z");
    Canvas::SetLeft(comma, -8.0);
    Canvas::SetTop(comma, -5.0);
    icon.Children().Append(dot);
    icon.Children().Append(comma);
  }
  root.Children().Append(icon);
  return root;
}

Grid EmojiStatusIcon() {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(kFluentEmoji24Path, kSettingIconVisualSize, 24, 1.02));
  return root;
}

Grid FluentPathIcon(std::wstring_view path, double scale, double view_box_size) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(path, kSettingIconVisualSize, view_box_size, scale));
  return root;
}

Grid FluentButtonPathIcon(std::wstring_view path, double scale) {
  Grid root;
  root.Width(kInlineButtonIconHostSize);
  root.Height(kInlineButtonIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(path, kInlineButtonIconHostSize, 20, scale));
  return root;
}

Grid ActionButtonPathIcon(std::wstring_view path, double scale, double view_box_size) {
  Grid root;
  root.Width(kActionButtonIconHostSize);
  root.Height(kActionButtonIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(path, kActionButtonIconHostSize, view_box_size, scale));
  return root;
}

Grid PinyinAssistPathIcon(std::wstring_view path, double scale) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(path, kSettingIconVisualSize, 20, scale));
  return root;
}

Grid ChevronStatusIcon(std::wstring_view path, double scale) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(path, kSettingIconVisualSize, 20, scale));
  return root;
}

Grid TriangleStatusIcon(bool right) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(right ? kFluentTriangleRight12FilledPath
                                         : kFluentTriangleLeft12FilledPath,
                                   kSettingIconVisualSize,
                                   12,
                                   0.54,
                                   right ? -0.03 : 0.03,
                                   0.035,
                                   0.64));
  return root;
}

Grid CandidateLayoutIcon(std::wstring_view layout) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(layout == fp::kCandidateLayoutVertical
                                       ? kFluentReOrderDotsVertical20RegularPath
                                       : kFluentReOrderDotsHorizontal20RegularPath,
                                   kSettingIconVisualSize,
                                   20,
                                   1.08));
  return root;
}

Grid AutoPinyinCorrectionIcon(bool enabled) {
  return PinyinAssistPathIcon(enabled ? kFluentIconRename20RegularPath
                                      : kFluentIconRectangleLandscape20RegularPath,
                              0.94);
}

Grid SuperAbbrevIcon(bool enabled) {
  return PinyinAssistPathIcon(enabled ? kFluentIconFlash20RegularPath
                                      : kFluentIconFlashOff20RegularPath,
                              0.94);
}

Grid SmartFuzzyPinyinIcon(bool enabled) {
  return PinyinAssistPathIcon(enabled ? kFluentIconLightbulbFilament20RegularPath
                                      : kFluentIconLightbulb20RegularPath,
                              0.94);
}

Grid FuzzyPinyinRulesStateIcon(bool enabled) {
  return PinyinAssistPathIcon(enabled ? kFluentIconEdit20RegularPath
                                      : kFluentIconEditOff20RegularPath,
                              0.94);
}

Grid ImportedLexiconsStateIcon(bool enabled) {
  return FluentPathIcon(enabled ? kFluentIconLibrary20RegularPath
                                : kFluentIconLibrary20FilledPath,
                        0.92);
}

Grid UserLexiconStateIcon(bool) {
  return FluentPathIcon(kFluentIconCalendarEmpty20RegularPath, 0.96);
}

Grid CustomPhrasesStateIcon(bool enabled) {
  return FluentPathIcon(enabled ? kFluentIconDocument20RegularPath
                                : kFluentIconDocumentDismiss20RegularPath,
                        0.94);
}

Grid CandidateFontSizeIcon() {
  return FluentPathIcon(kFluentIconTextFont20RegularPath, 0.94);
}

Grid ToolbarVisibleIcon(bool enabled) {
  return FluentPathIcon(enabled ? kFluentIconToggleRight20RegularPath
                                : kFluentIconToggleLeft20RegularPath,
                        1.06);
}

Grid StatusTipEnabledIcon(bool enabled) {
  return FluentPathIcon(enabled ? kFluentIconComment20RegularPath
                                : kFluentIconCommentOff20RegularPath,
                        0.98);
}

Grid StatusTipBlacklistIcon() {
  return FluentPathIcon(kFluentIconCommentNote20RegularPath, 0.98);
}

Grid ThemeModeIcon(std::wstring_view mode) {
  if (mode == fp::kThemeModeCustom) {
    return FluentPathIcon(kFluentIconKeyboard20RegularPath, 1.02);
  }
  if (mode == fp::kThemeModeLight) {
    return FluentPathIcon(kFluentWeatherSunny20RegularPath, 1.02);
  }
  if (mode == fp::kThemeModeDark) {
    return FluentPathIcon(kFluentWeatherMoon20FilledPath, 0.98);
  }
  return FluentPathIcon(kFluentDarkTheme20RegularPath, 1.02);
}

Grid ThemePresetRowIcon() {
  return FluentPathIcon(kFluentIconKeyboard20RegularPath, 1.02);
}

TextBlock ThemePresetLabel(std::wstring_view preset) {
  return Text(fp::ThemePresetLabelText(preset), 13, FW_SEMIBOLD);
}

Grid ThemePreviewArtwork(std::wstring_view preset,
                         double width,
                         double height,
                         bool compact) {
  const auto preview = ThemePreview(preset);
  const double vertical_guard = compact ? 8.0 : 14.0;
  const double shape_height = compact ? 47.0 : 54.0;
  const double scale =
      std::min(width / (compact ? 118.0 : 146.0),
               std::max(1.0, height - vertical_guard) / shape_height);
  const double tile_size = std::floor((compact ? 43.0 : 50.0) * scale);
  const double circle_size = std::floor((compact ? 47.0 : 54.0) * scale);
  const double side_margin = std::max(10.0, (compact ? 14.0 : 18.0) * scale);
  Grid card;
  card.Width(width);
  card.Height(height);
  card.HorizontalAlignment(HorizontalAlignment::Center);
  card.VerticalAlignment(VerticalAlignment::Center);
  card.UseLayoutRounding(true);

  Border background;
  background.CornerRadius(Radius(compact ? 6 : 8));
  background.Background(Brush(preview.background));
  background.BorderBrush(Brush(preview.edge));
  background.BorderThickness(SettingsHairlineThickness());
  card.Children().Append(background);

  Border tile;
  tile.Width(tile_size);
  tile.Height(tile_size);
  tile.HorizontalAlignment(HorizontalAlignment::Left);
  tile.VerticalAlignment(VerticalAlignment::Center);
  tile.Margin(Thickness{side_margin, 0, 0, 0});
  tile.CornerRadius(Radius(compact ? 5 : 6));
  tile.Background(Brush(preview.tile));
  tile.BorderBrush(Brush(preview.edge));
  tile.BorderThickness(SettingsHairlineThickness());
  auto aa = Text(L"Aa", compact ? 20 : 22, FW_SEMIBOLD);
  aa.Foreground(Brush(preview.text));
  aa.HorizontalAlignment(HorizontalAlignment::Center);
  aa.VerticalAlignment(VerticalAlignment::Center);
  aa.TextWrapping(TextWrapping::NoWrap);
  tile.Child(aa);
  card.Children().Append(tile);

  Grid circle;
  circle.Width(circle_size);
  circle.Height(circle_size);
  circle.HorizontalAlignment(HorizontalAlignment::Right);
  circle.VerticalAlignment(VerticalAlignment::Center);
  circle.Margin(Thickness{0, 0, side_margin, 0});
  Shapes::Ellipse disk;
  disk.Fill(Brush(preview.circle));
  disk.Width(circle_size);
  disk.Height(circle_size);
  disk.HorizontalAlignment(HorizontalAlignment::Center);
  disk.VerticalAlignment(VerticalAlignment::Center);
  circle.Children().Append(disk);
  auto keyboard = PathShape(kFluentIconKeyboard20RegularPath,
                            compact ? 24 : 27,
                            20,
                            1.0);
  keyboard.Fill(Brush(preview.symbol));
  circle.Children().Append(keyboard);
  card.Children().Append(circle);
  return card;
}

}  // namespace fp::config_winui