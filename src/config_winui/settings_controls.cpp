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

Button ActionButton(std::wstring_view text, std::wstring_view glyph) {
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(108);
  button.HorizontalAlignment(HorizontalAlignment::Right);
  StackPanel content;
  content.Orientation(Orientation::Horizontal);
  content.Spacing(8);
  content.Children().Append(Icon(glyph, kInlineButtonIconHostSize));
  content.Children().Append(Text(text, 13, FW_SEMIBOLD));
  button.Content(content);
  return button;
}

Button ActionPathButton(std::wstring_view text,
                        std::wstring_view path,
                        double scale,
                        double view_box_size) {
  const auto palette = CurrentSettingsPalette();
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(108);
  button.HorizontalAlignment(HorizontalAlignment::Right);
  button.Background(Brush(palette.button));
  button.BorderBrush(SettingsBorderBrush());
  button.BorderThickness(SettingsHairlineThickness());
  button.CornerRadius(Radius(8));
  const auto background = Brush(palette.button).as<IInspectable>();
  const auto hover = Brush(palette.button_hover).as<IInspectable>();
  const auto pressed = Brush(palette.button_pressed).as<IInspectable>();
  const auto border = SettingsBorderBrush().as<IInspectable>();
  const auto border_hover = SettingsBorderHoverBrush().as<IInspectable>();
  const auto foreground = Brush(palette.text).as<IInspectable>();
  button.Resources().Insert(box_value(L"ButtonBackground"), background);
  button.Resources().Insert(box_value(L"ButtonBackgroundPointerOver"), hover);
  button.Resources().Insert(box_value(L"ButtonBackgroundPressed"), pressed);
  button.Resources().Insert(box_value(L"ButtonBorderBrush"), border);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPointerOver"), border_hover);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPressed"), border_hover);
  button.Resources().Insert(box_value(L"ButtonForeground"), foreground);
  button.Resources().Insert(box_value(L"ButtonForegroundPointerOver"), foreground);
  button.Resources().Insert(box_value(L"ButtonForegroundPressed"), foreground);
  Grid content;
  content.ColumnSpacing(8);
  content.VerticalAlignment(VerticalAlignment::Center);
  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::FromPixels(kActionButtonIconHostSize));
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::Auto());
  content.ColumnDefinitions().Append(icon_column);
  content.ColumnDefinitions().Append(text_column);

  auto icon = ActionButtonPathIcon(path, scale, view_box_size);
  Grid::SetColumn(icon, 0);
  content.Children().Append(icon);
  auto label = Text(text, 13, FW_SEMIBOLD);
  label.TextWrapping(TextWrapping::NoWrap);
  label.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(label, 1);
  content.Children().Append(label);
  button.Content(content);
  return button;
}

Button CompactActionButton(std::wstring_view text, std::wstring_view glyph) {
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(86);
  button.Padding(Thickness{10, 6, 10, 7});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  StackPanel content;
  content.Orientation(Orientation::Horizontal);
  content.Spacing(6);
  content.Children().Append(Icon(glyph, kInlineButtonIconHostSize));
  content.Children().Append(Text(text, 12, FW_SEMIBOLD));
  button.Content(content);
  return button;
}

Button CompactPathActionButton(std::wstring_view text,
                               std::wstring_view path,
                               double scale) {
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(74);
  button.Padding(Thickness{8, 5, 9, 6});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  Grid content;
  content.ColumnSpacing(6);
  content.VerticalAlignment(VerticalAlignment::Center);
  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::FromPixels(kInlineButtonIconHostSize));
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::Auto());
  content.ColumnDefinitions().Append(icon_column);
  content.ColumnDefinitions().Append(text_column);

  auto icon = FluentButtonPathIcon(path, scale);
  Grid::SetColumn(icon, 0);
  content.Children().Append(icon);
  auto label = Text(text, 12, FW_SEMIBOLD);
  label.TextWrapping(TextWrapping::NoWrap);
  label.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(label, 1);
  content.Children().Append(label);
  button.Content(content);
  return button;
}

Button InlinePathActionButton(std::wstring_view text,
                              std::wstring_view path,
                              double scale) {
  const auto palette = CurrentSettingsPalette();
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(50);
  button.Height(28);
  button.MinHeight(28);
  button.Padding(Thickness{5, 2, 6, 3});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  button.BorderThickness(UniformThickness(0));
  auto normal = TransparentBrush().as<IInspectable>();
  auto hover = Brush(palette.button_hover).as<IInspectable>();
  auto pressed = Brush(palette.button_pressed).as<IInspectable>();
  button.Resources().Insert(box_value(L"ButtonBackground"), normal);
  button.Resources().Insert(box_value(L"ButtonBackgroundPointerOver"), hover);
  button.Resources().Insert(box_value(L"ButtonBackgroundPressed"), pressed);
  button.Resources().Insert(box_value(L"ButtonBorderBrush"), normal);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPointerOver"), normal);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPressed"), normal);
  const auto foreground = Brush(palette.text).as<IInspectable>();
  button.Resources().Insert(box_value(L"ButtonForeground"), foreground);
  button.Resources().Insert(box_value(L"ButtonForegroundPointerOver"), foreground);
  button.Resources().Insert(box_value(L"ButtonForegroundPressed"), foreground);

  Grid content;
  content.ColumnSpacing(4);
  content.VerticalAlignment(VerticalAlignment::Center);
  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::FromPixels(kInlineButtonIconHostSize));
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::Auto());
  content.ColumnDefinitions().Append(icon_column);
  content.ColumnDefinitions().Append(text_column);

  auto icon = FluentButtonPathIcon(path, scale);
  Grid::SetColumn(icon, 0);
  content.Children().Append(icon);
  auto label = Text(text, 13, FW_SEMIBOLD);
  label.TextWrapping(TextWrapping::NoWrap);
  label.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(label, 1);
  content.Children().Append(label);
  button.Content(content);
  return button;
}

Border StableInlinePathActionButton(std::wstring_view text,
                                    std::wstring_view path,
                                    double scale,
                                    std::function<void()> on_click) {
  const auto palette = CurrentSettingsPalette();
  Border button;
  button.MinWidth(60);
  button.Height(28);
  button.Padding(Thickness{8, 2, 9, 3});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  button.VerticalAlignment(VerticalAlignment::Center);
  button.Background(Brush(palette.button));
  button.BorderBrush(SettingsBorderBrush());
  button.BorderThickness(SettingsHairlineThickness());
  button.CornerRadius(Radius(8));
  button.UseLayoutRounding(true);

  Grid content;
  content.ColumnSpacing(5);
  content.VerticalAlignment(VerticalAlignment::Center);
  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::FromPixels(kInlineButtonIconHostSize));
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::Auto());
  content.ColumnDefinitions().Append(icon_column);
  content.ColumnDefinitions().Append(text_column);

  auto icon = FluentButtonPathIcon(path, scale);
  Grid::SetColumn(icon, 0);
  content.Children().Append(icon);
  auto label = Text(text, 12, FW_SEMIBOLD);
  label.TextWrapping(TextWrapping::NoWrap);
  label.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(label, 1);
  content.Children().Append(label);
  button.Child(content);

  auto hovered = std::make_shared<bool>(false);
  auto pressed = std::make_shared<bool>(false);
  auto apply_visual = std::make_shared<std::function<void()>>();
  *apply_visual = [button, label, hovered, pressed, palette]() {
    if (*pressed) {
      button.Background(Brush(palette.button_pressed));
      button.BorderBrush(SettingsBorderHoverBrush());
    } else if (*hovered) {
      button.Background(Brush(palette.button_hover));
      button.BorderBrush(SettingsBorderHoverBrush());
    } else {
      button.Background(Brush(palette.button));
      button.BorderBrush(SettingsBorderBrush());
    }
    label.Foreground(Brush(palette.text));
  };

  button.PointerEntered([hovered, apply_visual](auto const&, auto const&) {
    *hovered = true;
    (*apply_visual)();
  });
  button.PointerExited([hovered, pressed, apply_visual](auto const&, auto const&) {
    *hovered = false;
    *pressed = false;
    (*apply_visual)();
  });
  button.PointerPressed([pressed, apply_visual](auto const&, auto const&) {
    *pressed = true;
    (*apply_visual)();
  });
  button.PointerReleased([pressed, apply_visual](auto const&, auto const&) {
    *pressed = false;
    (*apply_visual)();
  });
  button.Tapped([pressed, apply_visual, on_click = std::move(on_click)](auto const&, auto const&) {
    *pressed = false;
    (*apply_visual)();
    if (on_click) {
      on_click();
    }
  });
  (*apply_visual)();
  return button;
}

Border StableIconToolButton(std::wstring_view path,
                            std::wstring_view tooltip,
                            double scale,
                            std::function<void()> on_click,
                            double width,
                            double height) {
  const auto palette = CurrentSettingsPalette();
  Border button;
  button.Width(width);
  button.Height(height);
  button.MinWidth(width);
  button.Padding(Thickness{0, 0, 0, 1});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  button.VerticalAlignment(VerticalAlignment::Center);
  button.Background(Brush(palette.button));
  button.BorderBrush(SettingsBorderBrush());
  button.BorderThickness(SettingsHairlineThickness());
  button.CornerRadius(Radius(8));
  button.UseLayoutRounding(true);
  button.Child(ActionButtonPathIcon(path, scale));
  SetSettingsToolTip(button, tooltip);

  auto hovered = std::make_shared<bool>(false);
  auto pressed = std::make_shared<bool>(false);
  auto apply_visual = std::make_shared<std::function<void()>>();
  *apply_visual = [button, hovered, pressed, palette]() {
    if (*pressed) {
      button.Background(Brush(palette.button_pressed));
      button.BorderBrush(SettingsBorderHoverBrush());
    } else if (*hovered) {
      button.Background(Brush(palette.button_hover));
      button.BorderBrush(SettingsBorderHoverBrush());
    } else {
      button.Background(Brush(palette.button));
      button.BorderBrush(SettingsBorderBrush());
    }
  };

  button.PointerEntered([hovered, apply_visual](auto const&, auto const&) {
    *hovered = true;
    (*apply_visual)();
  });
  button.PointerExited([hovered, pressed, apply_visual](auto const&, auto const&) {
    *hovered = false;
    *pressed = false;
    (*apply_visual)();
  });
  button.PointerPressed([pressed, apply_visual](auto const&, auto const&) {
    *pressed = true;
    (*apply_visual)();
  });
  button.PointerReleased([pressed, apply_visual](auto const&, auto const&) {
    *pressed = false;
    (*apply_visual)();
  });
  button.Tapped([pressed, apply_visual, on_click = std::move(on_click)](auto const&, auto const&) {
    *pressed = false;
    (*apply_visual)();
    if (on_click) {
      on_click();
    }
  });
  (*apply_visual)();
  return button;
}

Button IconToolButton(std::wstring_view path, std::wstring_view tooltip, double scale) {
  const auto palette = CurrentSettingsPalette();
  Button button;
  ApplySettingsUIFont(button);
  button.Width(36);
  button.Height(36);
  button.MinWidth(36);
  button.Padding(Thickness{0, 0, 0, 1});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  button.VerticalAlignment(VerticalAlignment::Center);
  button.Background(Brush(palette.button));
  button.BorderBrush(SettingsBorderBrush());
  button.BorderThickness(SettingsHairlineThickness());
  button.CornerRadius(Radius(8));
  const auto background = Brush(palette.button).as<IInspectable>();
  const auto hover = Brush(palette.button_hover).as<IInspectable>();
  const auto pressed = Brush(palette.button_pressed).as<IInspectable>();
  const auto border = SettingsBorderBrush().as<IInspectable>();
  const auto border_hover = SettingsBorderHoverBrush().as<IInspectable>();
  button.Resources().Insert(box_value(L"ButtonBackground"), background);
  button.Resources().Insert(box_value(L"ButtonBackgroundPointerOver"), hover);
  button.Resources().Insert(box_value(L"ButtonBackgroundPressed"), pressed);
  button.Resources().Insert(box_value(L"ButtonBorderBrush"), border);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPointerOver"), border_hover);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPressed"), border_hover);
  button.Content(ActionButtonPathIcon(path, scale));
  SetSettingsToolTip(button, tooltip);
  return button;
}

}  // namespace fp::config_winui
