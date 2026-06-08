#include "config_winui/status_tip_blacklist_dialog.h"

#include "common/constants.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/status_tip_blacklist.h"
#include "config_winui/theme_helpers.h"

#include <windows.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fp::config_winui {
namespace {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

struct StatusTipBlacklistRow {
  UIElement row{nullptr};
  TextBox process{nullptr};
  bool removed = false;
};

}  // namespace

void ShowStatusTipBlacklistDialog(XamlRoot const& xaml_root,
                                  TextBlock const& count_icon) {
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
  content.Children().Append(Text(L"状态提示黑名单", 20, FW_SEMIBOLD));

  auto description = Text(L"每行一个进程名或通配符。", 12);
  description.Foreground(SettingsSecondaryTextBrush());
  content.Children().Append(description);

  Grid header;
  header.ColumnSpacing(12);
  header.MinHeight(32);
  ColumnDefinition title_column;
  title_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  ColumnDefinition actions_column;
  actions_column.Width(GridLengthHelper::Auto());
  header.ColumnDefinitions().Append(title_column);
  header.ColumnDefinitions().Append(actions_column);

  auto title = Text(L"进程列表", 13, FW_SEMIBOLD);
  title.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(title, 0);
  header.Children().Append(title);

  StackPanel actions;
  actions.Orientation(Orientation::Horizontal);
  actions.Spacing(4);
  actions.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(actions, 1);
  header.Children().Append(actions);

  constexpr double kBlacklistRowsHeight = kSettingsDialogListHeight;
  Grid list_content;
  list_content.RowSpacing(6);
  RowDefinition list_header_row;
  list_header_row.Height(GridLengthHelper::Auto());
  RowDefinition list_body_row;
  list_body_row.Height(GridLengthHelper::FromPixels(kBlacklistRowsHeight));
  list_content.RowDefinitions().Append(list_header_row);
  list_content.RowDefinitions().Append(list_body_row);
  Grid::SetRow(header, 0);
  list_content.Children().Append(header);

  Grid list_host;
  list_host.Height(kBlacklistRowsHeight);
  list_host.HorizontalAlignment(HorizontalAlignment::Stretch);

  StackPanel rows_panel;
  rows_panel.Spacing(6);
  rows_panel.Margin(Thickness{0, 0, 16, 0});

  Grid empty_state;
  empty_state.Height(kBlacklistRowsHeight);
  empty_state.HorizontalAlignment(HorizontalAlignment::Stretch);
  empty_state.VerticalAlignment(VerticalAlignment::Stretch);
  StackPanel empty_text;
  empty_text.Spacing(4);
  empty_text.HorizontalAlignment(HorizontalAlignment::Center);
  empty_text.VerticalAlignment(VerticalAlignment::Center);
  auto empty_title = Text(L"暂无黑名单项目", 14, FW_SEMIBOLD);
  empty_title.HorizontalTextAlignment(TextAlignment::Center);
  empty_text.Children().Append(empty_title);
  auto empty_note = Text(L"点击“添加”创建一条进程规则。", 12);
  empty_note.Foreground(SettingsSecondaryTextBrush());
  empty_note.HorizontalTextAlignment(TextAlignment::Center);
  empty_text.Children().Append(empty_note);
  empty_state.Children().Append(empty_text);

  auto rows = std::make_shared<std::vector<std::shared_ptr<StatusTipBlacklistRow>>>();
  auto update_empty_state = std::make_shared<std::function<void()>>();
  *update_empty_state = [rows, empty_state]() {
    const bool has_visible_row =
        std::any_of(rows->begin(), rows->end(), [](const auto& row) {
          return row && !row->removed;
        });
    empty_state.Visibility(has_visible_row ? Visibility::Collapsed : Visibility::Visible);
  };

  auto add_blacklist_row = std::make_shared<std::function<void(std::wstring)>>();
  *add_blacklist_row =
      [rows_panel, rows, update_empty_state](std::wstring process_value) {
    auto row_state = std::make_shared<StatusTipBlacklistRow>();
    Grid row;
    row.MinHeight(32);
    row.ColumnSpacing(8);
    row.HorizontalAlignment(HorizontalAlignment::Stretch);
    ColumnDefinition value_column;
    value_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition remove_column;
    remove_column.Width(GridLengthHelper::FromPixels(36));
    row.ColumnDefinitions().Append(value_column);
    row.ColumnDefinitions().Append(remove_column);

    TextBox process;
    ApplySettingsUIFont(process);
    process.MinWidth(220);
    process.MinHeight(32);
    process.Height(32);
    process.HorizontalAlignment(HorizontalAlignment::Stretch);
    process.PlaceholderText(L"process.exe 或 *.exe");
    process.Text(process_value);
    SetSettingsToolTip(process, L"例如 explorer.exe 或 *.exe");
    Grid::SetColumn(process, 0);
    row.Children().Append(process);

    row_state->row = row;
    row_state->process = process;
    auto remove = StableIconToolButton(
        kFluentIconDelete20RegularPath,
        L"删除",
        0.82,
        [rows_panel, row_state, update_empty_state]() {
      row_state->removed = true;
      const auto children = rows_panel.Children();
      for (uint32_t index = 0; index < children.Size(); ++index) {
        if (children.GetAt(index) == row_state->row) {
          children.RemoveAt(index);
          break;
        }
      }
      (*update_empty_state)();
        },
        34,
        32);
    Grid::SetColumn(remove, 1);
    row.Children().Append(remove);

    rows->push_back(row_state);
    rows_panel.Children().Append(row);
    (*update_empty_state)();
      };

  auto clear_rows = std::make_shared<std::function<void()>>();
  *clear_rows = [rows_panel, rows, update_empty_state]() {
    for (const auto& row : *rows) {
      if (row) {
        row->removed = true;
      }
    }
    rows_panel.Children().Clear();
    (*update_empty_state)();
  };

  actions.Children().Append(StableInlinePathActionButton(
      L"默认",
      kFluentIconArrowClockwise20RegularPath,
      0.66,
      [clear_rows, add_blacklist_row]() {
        (*clear_rows)();
        for (const auto& item : ParseStatusTipBlacklistSetting(fp::kDefaultStatusTipBlacklist)) {
          (*add_blacklist_row)(item);
        }
      }));
  actions.Children().Append(StableInlinePathActionButton(
      L"清空",
      kFluentIconDismissCircle20RegularPath,
      0.66,
      [clear_rows]() {
        (*clear_rows)();
      }));
  actions.Children().Append(StableInlinePathActionButton(
      L"添加",
      kFluentIconAddCircle20RegularPath,
      0.66,
      [add_blacklist_row]() {
        (*add_blacklist_row)(L"");
      }));

  for (const auto& item : CurrentStatusTipBlacklistItems()) {
    (*add_blacklist_row)(item);
  }
  (*update_empty_state)();

  ScrollViewer scroller;
  scroller.Content(rows_panel);
  scroller.Height(kBlacklistRowsHeight);
  scroller.MinHeight(kBlacklistRowsHeight);
  scroller.MaxHeight(kBlacklistRowsHeight);
  scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
  scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
  scroller.VerticalScrollMode(ScrollMode::Enabled);
  scroller.HorizontalScrollMode(ScrollMode::Disabled);
  scroller.ZoomMode(ZoomMode::Disabled);
  scroller.BringIntoViewOnFocusChange(false);
  list_host.Children().Append(scroller);
  list_host.Children().Append(empty_state);
  Grid::SetRow(list_host, 1);
  list_content.Children().Append(list_host);

  Border list_frame = SettingsFrame(list_content,
                                    CurrentSettingsPalette().card,
                                    Radius(8),
                                    Thickness{10, 8, 10, 8});
  list_frame.HorizontalAlignment(HorizontalAlignment::Stretch);
  content.Children().Append(list_frame);

  dialog.Content(content);
  auto operation = dialog.ShowAsync();
  operation.Completed([dialog, rows, count_icon](auto const& async, auto const& status) {
    if (status != Windows::Foundation::AsyncStatus::Completed ||
        async.GetResults() != ContentDialogResult::Primary) {
      return;
    }

    std::vector<std::wstring> items;
    for (const auto& row : *rows) {
      if (!row || row->removed) {
        continue;
      }
      const std::wstring normalized =
          NormalizeStatusTipBlacklistToken(std::wstring(row->process.Text()));
      if (!normalized.empty() && !StatusTipBlacklistContains(items, normalized)) {
        items.push_back(normalized);
      }
    }

    WriteStringSetting(fp::kStatusTipBlacklistSetting, JoinStatusTipBlacklistItems(items));
    count_icon.Text(std::to_wstring(items.size()));
    RequestInputStateRefreshDeferred();
  });
}

}  // namespace fp::config_winui
