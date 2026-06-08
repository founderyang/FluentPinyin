#include "config_winui/fuzzy_pinyin_rules_dialog.h"

#include "common/constants.h"
#include "config_winui/fuzzy_pinyin_custom_rules.h"
#include "config_winui/fuzzy_pinyin_rules.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/theme_helpers.h"

#include <windows.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fp::config_winui {
namespace {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

IReference<bool> NullableBool(bool value) {
  return box_value(value).as<IReference<bool>>();
}

bool IsChecked(CheckBox const& check) {
  const auto checked = check.IsChecked();
  return checked && checked.Value();
}

struct CustomFuzzyPinyinRuleRow {
  UIElement row{nullptr};
  TextBox left{nullptr};
  TextBox right{nullptr};
  bool removed = false;
};

void SetFuzzyPinyinRuleChecks(
    const std::vector<std::pair<std::wstring, CheckBox>>& checks,
    const std::vector<std::wstring>& enabled_rules) {
  for (const auto& [id, check] : checks) {
    check.IsChecked(NullableBool(FuzzyPinyinRuleSelected(enabled_rules, id)));
  }
}

}  // namespace

void ShowFuzzyPinyinRulesDialog(XamlRoot const& xaml_root,
                                ToggleSwitch const& fuzzy_switch,
                                Grid const& state_icon,
                                bool require_enabled,
                                std::shared_ptr<bool> suppress_toggle_dialog) {
  ContentDialog dialog;
  ApplySettingsDialogBase(dialog, xaml_root);
  dialog.PrimaryButtonText(L"完成");
  dialog.CloseButtonText(L"取消");
  dialog.DefaultButton(ContentDialogButton::Primary);
  ApplySettingsResources(dialog.Resources());
  dialog.Resources().Insert(box_value(L"ContentDialogPadding"),
                            box_value(Thickness{24, 16, 24, 8}));
  dialog.Resources().Insert(box_value(L"ContentDialogCommandSpaceMargin"),
                            box_value(Thickness{0, 6, 0, 0}));
  dialog.Resources().Insert(box_value(L"ContentDialogTitleMargin"),
                            box_value(Thickness{0, 0, 0, 10}));

  StackPanel content;
  content.Width(kSettingsDialogContentWidth);
  content.MaxWidth(kSettingsDialogContentWidth);
  content.Spacing(7);
  content.HorizontalAlignment(HorizontalAlignment::Stretch);
  content.Children().Append(Text(L"模糊拼音规则", 20, FW_SEMIBOLD));

  auto description = Text(L"勾选或添加要启用的规则。", 12);
  description.Foreground(SettingsSecondaryTextBrush());
  content.Children().Append(description);

  Grid mode_row;
  mode_row.ColumnSpacing(12);
  mode_row.HorizontalAlignment(HorizontalAlignment::Stretch);
  ColumnDefinition mode_title_column;
  mode_title_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  ColumnDefinition mode_combo_column;
  mode_combo_column.Width(GridLengthHelper::Auto());
  mode_row.ColumnDefinitions().Append(mode_title_column);
  mode_row.ColumnDefinitions().Append(mode_combo_column);

  auto mode_title = Text(L"编辑内容", 13, FW_SEMIBOLD);
  mode_title.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(mode_title, 0);
  mode_row.Children().Append(mode_title);

  const std::array<std::wstring_view, 2> mode_labels{L"预设规则", L"自定义规则"};
  auto mode_index =
      std::make_shared<int>(CurrentFuzzyPinyinCustomRuleCount() > 0 ? 1 : 0);
  auto update_visible_panel = std::make_shared<std::function<void()>>();

  Border mode_selector_frame;
  mode_selector_frame.Width(244);
  mode_selector_frame.Height(38);
  mode_selector_frame.HorizontalAlignment(HorizontalAlignment::Right);
  mode_selector_frame.VerticalAlignment(VerticalAlignment::Center);

  Grid mode_selector_grid;
  mode_selector_grid.ColumnSpacing(3);
  for (int index = 0; index < 2; ++index) {
    ColumnDefinition column;
    column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    mode_selector_grid.ColumnDefinitions().Append(column);
  }

  auto mode_item_frames = std::make_shared<std::vector<Border>>();
  auto mode_item_labels = std::make_shared<std::vector<TextBlock>>();
  auto update_mode_item_visuals = std::make_shared<std::function<void()>>();
  auto hovered_mode_item = std::make_shared<int>(-1);
  auto pressed_mode_item = std::make_shared<int>(-1);
  auto apply_mode_item_visual = [mode_index, hovered_mode_item, pressed_mode_item](
                                     Border const& item,
                                     TextBlock const& label,
                                     int index) {
    const auto palette = CurrentSettingsPalette();
    const bool selected = index == *mode_index;
    const bool hovered = index == *hovered_mode_item;
    const bool pressed = index == *pressed_mode_item;
    if (pressed) {
      item.Background(selected ? Brush(palette.button_pressed) : Brush(palette.button_hover));
    } else if (hovered) {
      item.Background(selected ? Brush(palette.button_hover) : Brush(palette.icon_backdrop));
    } else {
      item.Background(selected ? Brush(palette.button) : TransparentBrush());
    }
    label.Foreground(selected ? Brush(palette.text) : Brush(palette.secondary_text));
  };
  auto add_mode_item = [&](int index) {
    Border item;
    item.Height(32);
    item.Padding(Thickness{8, 0, 8, 1});
    item.HorizontalAlignment(HorizontalAlignment::Stretch);
    item.CornerRadius(Radius(6));
    item.BorderThickness(UniformThickness(0));

    auto label = Text(mode_labels[static_cast<size_t>(index)], 13, FW_SEMIBOLD);
    label.TextWrapping(TextWrapping::NoWrap);
    label.HorizontalAlignment(HorizontalAlignment::Center);
    label.VerticalAlignment(VerticalAlignment::Center);
    label.HorizontalTextAlignment(TextAlignment::Center);
    item.Child(label);
    apply_mode_item_visual(item, label, index);

    item.PointerEntered([hovered_mode_item,
                         mode_item_frames,
                         mode_item_labels,
                         update_mode_item_visuals,
                         index](auto const&, auto const&) {
      *hovered_mode_item = index;
      if (*update_mode_item_visuals) {
        (*update_mode_item_visuals)();
      }
    });
    item.PointerExited([hovered_mode_item,
                        pressed_mode_item,
                        mode_item_frames,
                        mode_item_labels,
                        update_mode_item_visuals,
                        index](auto const&, auto const&) {
      if (*hovered_mode_item == index) {
        *hovered_mode_item = -1;
      }
      if (*pressed_mode_item == index) {
        *pressed_mode_item = -1;
      }
      if (*update_mode_item_visuals) {
        (*update_mode_item_visuals)();
      }
    });
    item.PointerPressed([pressed_mode_item,
                         update_mode_item_visuals,
                         index](auto const&, auto const&) {
      *pressed_mode_item = index;
      if (*update_mode_item_visuals) {
        (*update_mode_item_visuals)();
      }
    });
    item.PointerReleased([pressed_mode_item, update_mode_item_visuals](auto const&,
                                                                      auto const&) {
      *pressed_mode_item = -1;
      if (*update_mode_item_visuals) {
        (*update_mode_item_visuals)();
      }
    });
    item.Tapped([mode_index,
                pressed_mode_item,
                mode_item_frames,
                mode_item_labels,
                update_visible_panel,
                update_mode_item_visuals,
                index](auto const&, auto const&) {
      *mode_index = index;
      *pressed_mode_item = -1;
      if (*update_mode_item_visuals) {
        (*update_mode_item_visuals)();
      } else {
        for (size_t item_index = 0; item_index < mode_item_frames->size(); ++item_index) {
          const auto palette = CurrentSettingsPalette();
          mode_item_frames->at(item_index).Background(
              static_cast<int>(item_index) == *mode_index
                  ? Brush(palette.button)
                  : TransparentBrush());
          mode_item_labels->at(item_index).Foreground(
              static_cast<int>(item_index) == *mode_index ? Brush(palette.text)
                                                          : Brush(palette.secondary_text));
        }
      }
      if (*update_visible_panel) {
        (*update_visible_panel)();
      }
    });
    mode_item_frames->push_back(item);
    mode_item_labels->push_back(label);
    Grid::SetColumn(item, index);
    mode_selector_grid.Children().Append(item);
  };
  add_mode_item(0);
  add_mode_item(1);
  *update_mode_item_visuals = [mode_index,
                               mode_item_frames,
                               mode_item_labels,
                               apply_mode_item_visual]() {
    for (size_t index = 0; index < mode_item_frames->size(); ++index) {
      apply_mode_item_visual(mode_item_frames->at(index),
                             mode_item_labels->at(index),
                             static_cast<int>(index));
    }
  };
  mode_selector_frame = SettingsFrame(mode_selector_grid,
                                      CurrentSettingsPalette().card,
                                      Radius(8),
                                      UniformThickness(3));
  mode_selector_frame.Width(244);
  mode_selector_frame.Height(38);
  mode_selector_frame.HorizontalAlignment(HorizontalAlignment::Right);
  mode_selector_frame.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(mode_selector_frame, 1);
  mode_row.Children().Append(mode_selector_frame);
  content.Children().Append(mode_row);

  constexpr double kRulesListHeight = kSettingsDialogListHeight;
  constexpr double kBuiltInRuleRowHeight = kRulesListHeight / 5.0;
  constexpr double kBuiltInRulesHeight = kRulesListHeight;
  constexpr double kCustomRulesHeight = kRulesListHeight;

  Grid built_in_content;
  built_in_content.RowSpacing(6);
  RowDefinition built_in_header_row;
  built_in_header_row.Height(GridLengthHelper::Auto());
  RowDefinition built_in_rules_row;
  built_in_rules_row.Height(GridLengthHelper::FromPixels(kBuiltInRulesHeight));
  built_in_content.RowDefinitions().Append(built_in_header_row);
  built_in_content.RowDefinitions().Append(built_in_rules_row);

  Grid built_in_header;
  built_in_header.ColumnSpacing(8);
  built_in_header.MinHeight(32);
  ColumnDefinition built_in_title_column;
  built_in_title_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  ColumnDefinition built_in_actions_column;
  built_in_actions_column.Width(GridLengthHelper::Auto());
  built_in_header.ColumnDefinitions().Append(built_in_title_column);
  built_in_header.ColumnDefinitions().Append(built_in_actions_column);

  auto built_in_title = Text(L"预设规则", 13, FW_SEMIBOLD);
  built_in_title.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(built_in_title, 0);
  built_in_header.Children().Append(built_in_title);

  StackPanel actions;
  actions.Orientation(Orientation::Horizontal);
  actions.Spacing(4);
  actions.VerticalAlignment(VerticalAlignment::Center);

  auto checks = std::make_shared<std::vector<std::pair<std::wstring, CheckBox>>>();
  const auto selected_rules = CurrentFuzzyPinyinRules(true);
  auto all = StableInlinePathActionButton(
      L"全选",
      kFluentIconCheckmarkCircle20RegularPath,
      0.66,
      [checks]() {
        SetFuzzyPinyinRuleChecks(
            *checks,
            ParseFuzzyPinyinRuleSetting(fp::kDefaultFuzzyPinyinRules, true));
      });
  actions.Children().Append(all);
  auto common = StableInlinePathActionButton(
      L"常用",
      kFluentIconSparkleCircle20RegularPath,
      0.64,
      [checks]() {
        SetFuzzyPinyinRuleChecks(
            *checks,
            ParseFuzzyPinyinRuleSetting(fp::kDefaultCommonFuzzyPinyinRules, false));
      });
  actions.Children().Append(common);
  auto clear = StableInlinePathActionButton(
      L"清空",
      kFluentIconDismissCircle20RegularPath,
      0.66,
      [checks]() {
        SetFuzzyPinyinRuleChecks(*checks, {});
      });
  actions.Children().Append(clear);
  Grid::SetColumn(actions, 1);
  built_in_header.Children().Append(actions);
  Grid::SetRow(built_in_header, 0);
  built_in_content.Children().Append(built_in_header);

  Grid rules_grid;
  rules_grid.ColumnSpacing(8);
  rules_grid.RowSpacing(0);
  ColumnDefinition left_rules_column;
  left_rules_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  ColumnDefinition right_rules_column;
  right_rules_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  rules_grid.ColumnDefinitions().Append(left_rules_column);
  rules_grid.ColumnDefinitions().Append(right_rules_column);
  for (int row_index = 0; row_index < 5; ++row_index) {
    RowDefinition row_definition;
    row_definition.Height(GridLengthHelper::FromPixels(kBuiltInRuleRowHeight));
    rules_grid.RowDefinitions().Append(row_definition);
  }

  for (size_t index = 0; index < FuzzyPinyinRuleDefinitions().size(); ++index) {
    const auto& rule = FuzzyPinyinRuleDefinitions()[index];
    Grid rule_cell;
    rule_cell.Height(kBuiltInRuleRowHeight);
    rule_cell.ColumnSpacing(6);
    rule_cell.HorizontalAlignment(HorizontalAlignment::Stretch);
    ColumnDefinition check_column;
    check_column.Width(GridLengthHelper::FromPixels(28));
    ColumnDefinition name_column;
    name_column.Width(GridLengthHelper::FromPixels(62));
    ColumnDefinition example_column;
    example_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    rule_cell.ColumnDefinitions().Append(check_column);
    rule_cell.ColumnDefinitions().Append(name_column);
    rule_cell.ColumnDefinitions().Append(example_column);

    CheckBox check;
    ApplySettingsUIFont(check);
    check.MinWidth(0);
    check.MinHeight(0);
    check.Width(24);
    check.Height(24);
    check.Margin(Thickness{0, 0, 0, 0});
    check.VerticalAlignment(VerticalAlignment::Center);
    check.IsChecked(NullableBool(FuzzyPinyinRuleSelected(selected_rules, rule.id)));
    Grid::SetColumn(check, 0);
    rule_cell.Children().Append(check);
    checks->emplace_back(std::wstring(rule.id), check);

    auto name = Text(rule.name, 12, FW_SEMIBOLD);
    name.TextWrapping(TextWrapping::NoWrap);
    name.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(name, 1);
    rule_cell.Children().Append(name);

    auto example = Text(rule.example, 11);
    example.Foreground(Brush(CurrentSettingsPalette().muted_text));
    example.TextWrapping(TextWrapping::NoWrap);
    example.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(example, 2);
    rule_cell.Children().Append(example);

    Grid::SetColumn(rule_cell, static_cast<int>(index % 2));
    Grid::SetRow(rule_cell, static_cast<int>(index / 2));
    rules_grid.Children().Append(rule_cell);
  }
  rules_grid.Height(kBuiltInRulesHeight);
  Grid::SetRow(rules_grid, 1);
  built_in_content.Children().Append(rules_grid);

  Border table_frame = SettingsFrame(built_in_content,
                                     CurrentSettingsPalette().card,
                                     Radius(8),
                                     Thickness{10, 8, 10, 8});
  table_frame.HorizontalAlignment(HorizontalAlignment::Stretch);
  content.Children().Append(table_frame);

  Grid custom_content;
  custom_content.RowSpacing(6);
  RowDefinition custom_header_row;
  custom_header_row.Height(GridLengthHelper::Auto());
  RowDefinition custom_list_row;
  custom_list_row.Height(GridLengthHelper::Auto());
  custom_content.RowDefinitions().Append(custom_header_row);
  custom_content.RowDefinitions().Append(custom_list_row);

  Grid custom_header;
  custom_header.ColumnSpacing(12);
  custom_header.MinHeight(32);
  ColumnDefinition custom_title_column;
  custom_title_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  ColumnDefinition custom_add_column;
  custom_add_column.Width(GridLengthHelper::Auto());
  custom_header.ColumnDefinitions().Append(custom_title_column);
  custom_header.ColumnDefinitions().Append(custom_add_column);

  auto custom_title = Text(L"自定义规则", 13, FW_SEMIBOLD);
  custom_title.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(custom_title, 0);
  custom_header.Children().Append(custom_title);

  auto add_custom_row = std::make_shared<std::function<void(std::wstring, std::wstring)>>();
  auto add_custom = StableInlinePathActionButton(
      L"添加",
      kFluentIconAddCircle20RegularPath,
      0.66,
      [add_custom_row]() {
        if (*add_custom_row) {
          (*add_custom_row)(L"", L"");
        }
      });
  Grid::SetColumn(add_custom, 1);
  custom_header.Children().Append(add_custom);
  Grid::SetRow(custom_header, 0);
  custom_content.Children().Append(custom_header);

  StackPanel custom_rows_panel;
  custom_rows_panel.Spacing(6);
  custom_rows_panel.Margin(Thickness{0, 0, 16, 0});
  Grid custom_empty_state;
  custom_empty_state.Height(kCustomRulesHeight);
  custom_empty_state.HorizontalAlignment(HorizontalAlignment::Stretch);
  custom_empty_state.VerticalAlignment(VerticalAlignment::Stretch);
  StackPanel custom_empty_text;
  custom_empty_text.Spacing(4);
  custom_empty_text.HorizontalAlignment(HorizontalAlignment::Center);
  custom_empty_text.VerticalAlignment(VerticalAlignment::Center);
  auto custom_empty_title = Text(L"暂无自定义规则", 14, FW_SEMIBOLD);
  custom_empty_title.HorizontalTextAlignment(TextAlignment::Center);
  custom_empty_text.Children().Append(custom_empty_title);
  auto custom_empty_note = Text(L"点击“添加”创建一条规则。", 12);
  custom_empty_note.Foreground(SettingsSecondaryTextBrush());
  custom_empty_note.HorizontalTextAlignment(TextAlignment::Center);
  custom_empty_text.Children().Append(custom_empty_note);
  custom_empty_state.Children().Append(custom_empty_text);
  auto custom_rows = std::make_shared<std::vector<std::shared_ptr<CustomFuzzyPinyinRuleRow>>>();
  auto update_custom_empty_state = std::make_shared<std::function<void()>>();
  *update_custom_empty_state = [custom_rows, custom_empty_state]() {
    const bool has_visible_row =
        std::any_of(custom_rows->begin(), custom_rows->end(), [](const auto& row) {
          return row && !row->removed;
        });
    custom_empty_state.Visibility(has_visible_row ? Visibility::Collapsed : Visibility::Visible);
  };
  *add_custom_row = [custom_rows_panel, custom_rows, add_custom_row, update_custom_empty_state](
                        std::wstring left_value,
                        std::wstring right_value) {
    auto row_state = std::make_shared<CustomFuzzyPinyinRuleRow>();
    Grid row;
    row.MinHeight(32);
    row.ColumnSpacing(8);
    row.HorizontalAlignment(HorizontalAlignment::Stretch);
    ColumnDefinition left_column;
    left_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition separator_column;
    separator_column.Width(GridLengthHelper::FromPixels(18));
    ColumnDefinition right_column;
    right_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition remove_column;
    remove_column.Width(GridLengthHelper::FromPixels(36));
    row.ColumnDefinitions().Append(left_column);
    row.ColumnDefinitions().Append(separator_column);
    row.ColumnDefinitions().Append(right_column);
    row.ColumnDefinitions().Append(remove_column);

    TextBox left;
    ApplySettingsUIFont(left);
    left.MinWidth(88);
    left.MinHeight(32);
    left.Height(32);
    left.HorizontalAlignment(HorizontalAlignment::Stretch);
    left.PlaceholderText(L"拼音 A");
    left.Text(left_value);
    SetSettingsToolTip(left, L"例如 n");
    Grid::SetColumn(left, 0);
    row.Children().Append(left);

    auto separator = Text(L"=", 14, FW_SEMIBOLD);
    separator.Width(18);
    separator.HorizontalTextAlignment(TextAlignment::Center);
    separator.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(separator, 1);
    row.Children().Append(separator);

    TextBox right;
    ApplySettingsUIFont(right);
    right.MinWidth(88);
    right.MinHeight(32);
    right.Height(32);
    right.HorizontalAlignment(HorizontalAlignment::Stretch);
    right.PlaceholderText(L"拼音 B");
    right.Text(right_value);
    SetSettingsToolTip(right, L"例如 l");
    Grid::SetColumn(right, 2);
    row.Children().Append(right);

    row_state->row = row;
    row_state->left = left;
    row_state->right = right;
    auto remove = StableIconToolButton(
        kFluentIconDelete20RegularPath,
        L"删除",
        0.82,
        [custom_rows_panel, row_state, update_custom_empty_state]() {
      row_state->removed = true;
      const auto children = custom_rows_panel.Children();
      for (uint32_t index = 0; index < children.Size(); ++index) {
        if (children.GetAt(index) == row_state->row) {
          children.RemoveAt(index);
          break;
        }
      }
      (*update_custom_empty_state)();
        },
        34,
        32);
    Grid::SetColumn(remove, 3);
    row.Children().Append(remove);

    custom_rows->push_back(row_state);
    custom_rows_panel.Children().Append(row);
    (*update_custom_empty_state)();
  };

  for (const auto& rule : ParseFuzzyPinyinCustomRuleSetting(
           ReadStringSetting(fp::kFuzzyPinyinCustomRulesSetting, L""))) {
    const auto [left, right] = SplitFuzzyPinyinCustomRule(rule);
    (*add_custom_row)(left, right);
  }
  (*update_custom_empty_state)();

  Grid custom_list_host;
  custom_list_host.Height(kCustomRulesHeight);
  ScrollViewer custom_scroller;
  custom_scroller.Content(custom_rows_panel);
  custom_scroller.Height(kCustomRulesHeight);
  custom_scroller.MinHeight(kCustomRulesHeight);
  custom_scroller.MaxHeight(kCustomRulesHeight);
  custom_scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
  custom_scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
  custom_scroller.VerticalScrollMode(ScrollMode::Enabled);
  custom_scroller.HorizontalScrollMode(ScrollMode::Disabled);
  custom_scroller.ZoomMode(ZoomMode::Disabled);
  custom_scroller.BringIntoViewOnFocusChange(false);
  custom_list_host.Children().Append(custom_scroller);
  custom_list_host.Children().Append(custom_empty_state);
  Grid::SetRow(custom_list_host, 1);
  custom_content.Children().Append(custom_list_host);
  Border custom_frame = SettingsFrame(custom_content,
                                      CurrentSettingsPalette().card,
                                      Radius(8),
                                      Thickness{10, 8, 10, 8});
  custom_frame.HorizontalAlignment(HorizontalAlignment::Stretch);
  content.Children().Append(custom_frame);

  *update_visible_panel = [table_frame, custom_frame, mode_index]() {
    const bool show_custom = *mode_index == 1;
    table_frame.Visibility(show_custom ? Visibility::Collapsed : Visibility::Visible);
    custom_frame.Visibility(show_custom ? Visibility::Visible : Visibility::Collapsed);
  };
  (*update_visible_panel)();

  dialog.Content(content);
  auto operation = dialog.ShowAsync();
  operation.Completed([dialog,
                       checks,
                       custom_rows,
                       fuzzy_switch,
                       state_icon,
                       require_enabled,
                       suppress_toggle_dialog](auto const& async, auto const& status) {
    if (status != Windows::Foundation::AsyncStatus::Completed ||
        async.GetResults() != ContentDialogResult::Primary) {
      if (require_enabled) {
        if (suppress_toggle_dialog) {
          *suppress_toggle_dialog = true;
        }
        fuzzy_switch.IsOn(false);
        SetIconChild(state_icon, FuzzyPinyinRulesStateIcon(false));
        if (suppress_toggle_dialog) {
          *suppress_toggle_dialog = false;
        }
        WriteBoolSetting(fp::kFuzzyPinyinSetting, false);
      }
      return;
    }

    std::vector<std::wstring> selected;
    for (const auto& [id, check] : *checks) {
      if (IsChecked(check)) {
        selected.push_back(id);
      }
    }

    std::vector<std::wstring> custom_rules;
    for (const auto& row : *custom_rows) {
      if (!row || row->removed) {
        continue;
      }
      std::wstring pair_text = std::wstring(row->left.Text()) + L"=" +
                               std::wstring(row->right.Text());
      std::wstring normalized = NormalizeFuzzyPinyinCustomRuleToken(pair_text);
      if (!normalized.empty() &&
          std::find(custom_rules.begin(), custom_rules.end(), normalized) ==
              custom_rules.end()) {
        custom_rules.push_back(normalized);
      }
    }

    if (selected.empty() && custom_rules.empty()) {
      WriteStringSetting(fp::kFuzzyPinyinRulesSetting, L"");
      WriteStringSetting(fp::kFuzzyPinyinCustomRulesSetting, L"");
      if (suppress_toggle_dialog) {
        *suppress_toggle_dialog = true;
      }
      fuzzy_switch.IsOn(false);
      SetIconChild(state_icon, FuzzyPinyinRulesStateIcon(false));
      if (suppress_toggle_dialog) {
        *suppress_toggle_dialog = false;
      }
      WriteBoolSetting(fp::kFuzzyPinyinSetting, false);
      RequestApplyInputConfigDeferred();
      return;
    }

    WriteStringSetting(fp::kFuzzyPinyinRulesSetting,
                       JoinFuzzyPinyinRules(selected));
    WriteStringSetting(fp::kFuzzyPinyinCustomRulesSetting,
                       JoinFuzzyPinyinCustomRules(custom_rules, L','));
    if (require_enabled || fuzzy_switch.IsOn()) {
      if (suppress_toggle_dialog) {
        *suppress_toggle_dialog = true;
      }
      fuzzy_switch.IsOn(true);
      SetIconChild(state_icon, FuzzyPinyinRulesStateIcon(true));
      if (suppress_toggle_dialog) {
        *suppress_toggle_dialog = false;
      }
      WriteBoolSetting(fp::kFuzzyPinyinSetting, true);
      RequestApplyInputConfigDeferred();
    }
  });
}

}  // namespace fp::config_winui
