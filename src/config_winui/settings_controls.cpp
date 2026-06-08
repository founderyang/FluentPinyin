#include "config_winui/settings_controls.h"

#include "config_winui/settings_icons.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/theme_helpers.h"

#include <windows.h>

#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Text.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace fp::config_winui {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using Windows::Foundation::IInspectable;
using Windows::UI::Text::FontWeights;

Border Card(UIElement const& child) {
  return SettingsFrame(child,
                       CurrentSettingsPalette().card,
                       Radius(8),
                       UniformThickness(16),
                       Thickness{0, 0, 0, 10});
}

Border StatusBadge(std::wstring_view status) {
  Border badge;
  badge.CornerRadius(Radius(999));
  badge.Padding(Thickness{8, 2, 8, 3});
  badge.VerticalAlignment(VerticalAlignment::Center);
  badge.UseLayoutRounding(true);

  const auto palette = CurrentSettingsPalette();
  SolidColorBrush background = Brush(palette.button);
  SolidColorBrush foreground = Brush(palette.secondary_text);
  if (status == L"已联动") {
    background = palette.light ? Brush(224, 245, 233) : Brush(42, 72, 55);
    foreground = palette.light ? Brush(31, 112, 66) : Brush(145, 225, 175);
  } else if (status == L"部分联动") {
    background = palette.light ? Brush(247, 240, 218) : Brush(70, 64, 42);
    foreground = palette.light ? Brush(136, 101, 24) : Brush(230, 205, 145);
  } else if (status == L"入口已接通") {
    background = palette.light ? Brush(224, 241, 250) : Brush(45, 65, 82);
    foreground = palette.light ? Brush(38, 103, 138) : Brush(150, 205, 235);
  } else if (status == L"已保存") {
    background = Brush(palette.button);
    foreground = Brush(palette.secondary_text);
  } else if (status == L"占位") {
    background = palette.light ? Brush(250, 229, 229) : Brush(72, 48, 48);
    foreground = palette.light ? Brush(146, 56, 56) : Brush(230, 160, 160);
  }
  badge.Background(background);

  auto label = Text(status, 11, FW_SEMIBOLD);
  label.Foreground(foreground);
  label.TextWrapping(TextWrapping::NoWrap);
  badge.Child(label);
  return badge;
}

StackPanel PageShell(std::wstring_view title, std::wstring_view subtitle) {
  StackPanel page;
  page.Spacing(12);
  page.Padding(Thickness{32, 12, 32, 32});
  page.HorizontalAlignment(HorizontalAlignment::Stretch);
  page.Children().Append(Text(title, 28, FW_SEMIBOLD));
  auto description = Text(subtitle, 14);
  description.Foreground(SettingsSecondaryTextBrush());
  description.Margin(Thickness{0, 0, 0, 8});
  page.Children().Append(description);
  return page;
}

TextBlock SectionHeader(std::wstring_view title, bool first) {
  auto header = Text(title, 16, FW_SEMIBOLD);
  header.Foreground(SettingsTextBrush());
  header.Margin(first ? Thickness{2, 2, 0, 0} : Thickness{2, 16, 0, 0});
  header.TextWrapping(TextWrapping::NoWrap);
  return header;
}

Border SettingIconBackdrop(UIElement const& icon_content) {
  Border icon_backdrop;
  icon_backdrop.Width(kSettingIconBackdropSize);
  icon_backdrop.Height(kSettingIconBackdropSize);
  icon_backdrop.CornerRadius(Radius(kSettingIconBackdropRadius));
  icon_backdrop.Background(Brush(CurrentSettingsPalette().icon_backdrop));
  icon_backdrop.VerticalAlignment(VerticalAlignment::Center);
  icon_backdrop.HorizontalAlignment(HorizontalAlignment::Center);
  icon_backdrop.UseLayoutRounding(true);
  icon_backdrop.Child(IconHost(icon_content));
  return icon_backdrop;
}

Border SettingRowWithIcon(std::wstring_view title,
                          std::wstring_view subtitle,
                          UIElement const& control,
                          UIElement const& icon_content,
                          std::wstring_view status,
                          double control_width) {
  (void)status;
  Grid grid;
  grid.HorizontalAlignment(HorizontalAlignment::Stretch);
  grid.ColumnSpacing(16);
  grid.RowSpacing(12);

  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::Auto());
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  ColumnDefinition control_column;
  control_column.Width(GridLengthHelper::FromPixels(control_width));
  grid.ColumnDefinitions().Append(icon_column);
  grid.ColumnDefinitions().Append(text_column);
  grid.ColumnDefinitions().Append(control_column);

  Border icon_backdrop = SettingIconBackdrop(icon_content);
  Grid::SetColumn(icon_backdrop, 0);
  grid.Children().Append(icon_backdrop);

  StackPanel text_area;
  text_area.Spacing(2);
  text_area.HorizontalAlignment(HorizontalAlignment::Stretch);

  StackPanel title_line;
  title_line.Orientation(Orientation::Horizontal);
  title_line.Spacing(8);
  title_line.Children().Append(Text(title, 15, FW_SEMIBOLD));
  text_area.Children().Append(title_line);

  auto sub = Text(subtitle, 12);
  sub.Foreground(SettingsSecondaryTextBrush());
  sub.TextWrapping(TextWrapping::Wrap);
  text_area.Children().Append(sub);
  Grid::SetColumn(text_area, 1);
  grid.Children().Append(text_area);

  auto control_element = control.as<FrameworkElement>();
  control_element.VerticalAlignment(VerticalAlignment::Center);
  control_element.HorizontalAlignment(HorizontalAlignment::Right);
  Grid::SetColumn(control_element, 2);
  grid.Children().Append(control);
  return Card(grid);
}

Border SettingRow(std::wstring_view title,
                  std::wstring_view subtitle,
                  UIElement const& control,
                  std::wstring_view glyph,
                  std::wstring_view status) {
  return SettingRowWithIcon(title, subtitle, control, Icon(glyph, 16), status);
}

Border SettingWideRowWithIcon(std::wstring_view title,
                              std::wstring_view subtitle,
                              UIElement const& control,
                              UIElement const& icon_content,
                              std::wstring_view status) {
  (void)status;
  Grid grid;
  grid.HorizontalAlignment(HorizontalAlignment::Stretch);
  grid.ColumnSpacing(16);
  grid.RowSpacing(12);

  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::Auto());
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  grid.ColumnDefinitions().Append(icon_column);
  grid.ColumnDefinitions().Append(text_column);

  RowDefinition text_row;
  text_row.Height(GridLengthHelper::Auto());
  RowDefinition control_row;
  control_row.Height(GridLengthHelper::Auto());
  grid.RowDefinitions().Append(text_row);
  grid.RowDefinitions().Append(control_row);

  Border icon_backdrop = SettingIconBackdrop(icon_content);
  Grid::SetColumn(icon_backdrop, 0);
  Grid::SetRow(icon_backdrop, 0);
  grid.Children().Append(icon_backdrop);

  StackPanel text_area;
  text_area.Spacing(2);
  text_area.HorizontalAlignment(HorizontalAlignment::Stretch);
  text_area.Children().Append(Text(title, 15, FW_SEMIBOLD));
  auto sub = Text(subtitle, 12);
  sub.Foreground(SettingsSecondaryTextBrush());
  sub.TextWrapping(TextWrapping::Wrap);
  text_area.Children().Append(sub);
  Grid::SetColumn(text_area, 1);
  Grid::SetRow(text_area, 0);
  grid.Children().Append(text_area);

  auto control_element = control.as<FrameworkElement>();
  control_element.VerticalAlignment(VerticalAlignment::Center);
  control_element.HorizontalAlignment(HorizontalAlignment::Stretch);
  Grid::SetColumn(control_element, 1);
  Grid::SetRow(control_element, 1);
  grid.Children().Append(control);
  return Card(grid);
}

Border SettingWideRow(std::wstring_view title,
                      std::wstring_view subtitle,
                      UIElement const& control,
                      std::wstring_view glyph,
                      std::wstring_view status) {
  auto icon = Icon(glyph, 16);
  icon.HorizontalAlignment(HorizontalAlignment::Center);
  icon.VerticalAlignment(VerticalAlignment::Center);
  return SettingWideRowWithIcon(title, subtitle, control, icon, status);
}

void ConfigureSettingCombo(ComboBox const& combo) {
  ApplySettingsUIFont(combo);
  combo.Width(120);
  combo.MinWidth(120);
  combo.HorizontalAlignment(HorizontalAlignment::Right);
  combo.HorizontalContentAlignment(HorizontalAlignment::Center);
  const auto palette = CurrentSettingsPalette();
  combo.Background(Brush(palette.button));
  combo.Foreground(SettingsTextBrush());
  combo.BorderBrush(SettingsBorderBrush());
}

ComboBoxItem ThemeComboItem(std::wstring_view label) {
  const auto palette = CurrentSettingsPalette();
  ComboBoxItem item;
  ApplySettingsUIFont(item);
  auto text = Text(label, 13, FW_SEMIBOLD);
  text.Foreground(SettingsTextBrush());
  text.TextWrapping(TextWrapping::NoWrap);
  item.Content(text);
  item.Background(Brush(palette.card));
  item.Foreground(SettingsTextBrush());
  item.BorderBrush(TransparentBrush());
  item.BorderThickness(UniformThickness(0));
  item.RequestedTheme(CurrentSettingsElementTheme());
  item.HorizontalContentAlignment(HorizontalAlignment::Center);
  return item;
}

ComboBox ChoiceCombo(const std::vector<std::wstring>& labels,
                     int current_index,
                     std::function<void(int)> on_select) {
  ComboBox combo;
  ConfigureSettingCombo(combo);
  for (const auto& label : labels) {
    combo.Items().Append(ThemeComboItem(label));
  }
  if (labels.empty()) {
    return combo;
  }

  combo.SelectedIndex(std::clamp(current_index, 0, static_cast<int>(labels.size()) - 1));
  combo.SelectionChanged([combo, on_select](auto const&, auto const&) {
    const int selected = combo.SelectedIndex();
    if (selected < 0) {
      return;
    }
    if (on_select) {
      on_select(selected);
    }
  });
  return combo;
}

}  // namespace fp::config_winui
