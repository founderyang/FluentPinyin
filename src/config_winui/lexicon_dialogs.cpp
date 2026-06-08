#include "config_winui/lexicon_dialogs.h"

#include "common/constants.h"
#include "common/encoding.h"
#include "config_winui/file_dialogs.h"
#include "config_winui/lexicon_store.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"
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
#include <string_view>
#include <utility>
#include <vector>

namespace fp::config_winui {
namespace {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

struct LexiconPhraseRow {
  UIElement row{nullptr};
  TextBox phrase{nullptr};
  TextBox code{nullptr};
  TextBox weight{nullptr};
  bool removed = false;
};

struct ManagedDictionaryRow {
  UIElement row{nullptr};
  ToggleSwitch toggle{nullptr};
  std::string name;
  bool removed = false;
};

}  // namespace

void ShowManagedDictionariesDialog(XamlRoot const& xaml_root,
                                   HWND owner,
                                   Grid const& state_icon) {
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
  content.Children().Append(Text(L"导入词库", 20, FW_SEMIBOLD));

  auto description = Text(L"管理已导入的词库文件。", 12);
  description.Foreground(SettingsSecondaryTextBrush());
  content.Children().Append(description);

  auto rows = std::make_shared<std::vector<std::shared_ptr<ManagedDictionaryRow>>>();
  StackPanel rows_panel;
  rows_panel.Spacing(6);
  rows_panel.Margin(Thickness{0, 0, 16, 0});

  Grid empty_state;
  empty_state.Height(kSettingsDialogListHeight);
  empty_state.HorizontalAlignment(HorizontalAlignment::Stretch);
  empty_state.VerticalAlignment(VerticalAlignment::Stretch);
  StackPanel empty_text;
  empty_text.Spacing(4);
  empty_text.HorizontalAlignment(HorizontalAlignment::Center);
  empty_text.VerticalAlignment(VerticalAlignment::Center);
  auto empty_title = Text(L"暂无导入词库", 14, FW_SEMIBOLD);
  empty_title.HorizontalTextAlignment(TextAlignment::Center);
  empty_text.Children().Append(empty_title);
  auto empty_note = Text(L"点击“导入”选择 .dict.yaml 文件。", 12);
  empty_note.Foreground(SettingsSecondaryTextBrush());
  empty_note.HorizontalTextAlignment(TextAlignment::Center);
  empty_text.Children().Append(empty_note);
  empty_state.Children().Append(empty_text);

  auto update_empty_state = std::make_shared<std::function<void()>>();
  *update_empty_state = [rows, empty_state]() {
    const bool has_visible_row =
        std::any_of(rows->begin(), rows->end(), [](const auto& row) {
          return row && !row->removed;
        });
    empty_state.Visibility(has_visible_row ? Visibility::Collapsed : Visibility::Visible);
  };

  auto add_dictionary_row =
      std::make_shared<std::function<void(std::string, bool)>>();
  *add_dictionary_row = [rows_panel, rows, update_empty_state](
                            std::string name,
                            bool enabled) {
    auto row_state = std::make_shared<ManagedDictionaryRow>();
    row_state->name = name;

    Grid row;
    row.MinHeight(36);
    row.ColumnSpacing(8);
    row.HorizontalAlignment(HorizontalAlignment::Stretch);
    ColumnDefinition name_column;
    name_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition switch_column;
    switch_column.Width(GridLengthHelper::FromPixels(70));
    ColumnDefinition remove_column;
    remove_column.Width(GridLengthHelper::FromPixels(36));
    row.ColumnDefinitions().Append(name_column);
    row.ColumnDefinitions().Append(switch_column);
    row.ColumnDefinitions().Append(remove_column);

    auto label = Text(fp::Utf8ToWide(name), 13, FW_SEMIBOLD);
    label.TextWrapping(TextWrapping::NoWrap);
    label.TextTrimming(TextTrimming::CharacterEllipsis);
    label.VerticalAlignment(VerticalAlignment::Center);
    SetSettingsToolTip(label, fp::Utf8ToWide(name));
    Grid::SetColumn(label, 0);
    row.Children().Append(label);

    ToggleSwitch toggle;
    ApplySettingsUIFont(toggle);
    toggle.MinWidth(0);
    toggle.IsOn(enabled);
    toggle.HorizontalAlignment(HorizontalAlignment::Right);
    toggle.VerticalAlignment(VerticalAlignment::Center);
    row_state->toggle = toggle;
    Grid::SetColumn(toggle, 1);
    row.Children().Append(toggle);

    row_state->row = row;
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
    Grid::SetColumn(remove, 2);
    row.Children().Append(remove);

    rows->push_back(row_state);
    rows_panel.Children().Append(row);
    (*update_empty_state)();
  };

  for (const auto& entry : ReadManagedDictionaryManifest()) {
    (*add_dictionary_row)(entry.name, entry.enabled);
  }
  (*update_empty_state)();

  Grid header;
  header.ColumnSpacing(12);
  header.MinHeight(34);
  ColumnDefinition title_column;
  title_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  ColumnDefinition add_column;
  add_column.Width(GridLengthHelper::Auto());
  header.ColumnDefinitions().Append(title_column);
  header.ColumnDefinitions().Append(add_column);

  auto title = Text(L"已导入词库", 13, FW_SEMIBOLD);
  title.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(title, 0);
  header.Children().Append(title);

  auto import_button = StableInlinePathActionButton(
      L"导入",
      kFluentIconArrowImport20RegularPath,
      0.66,
      [owner, rows, rows_panel, add_dictionary_row, update_empty_state, state_icon]() {
    const auto selected = PickRimeDictionaryFile(owner);
    if (!selected) {
      return;
    }

    std::wstring error;
    if (!ImportManagedDictionary(*selected, &error)) {
      MessageBoxW(owner,
                  error.empty() ? L"导入词库失败。" : error.c_str(),
                  L"流畅拼音 设置",
                  MB_ICONWARNING);
      return;
    }

    rows->clear();
    rows_panel.Children().Clear();
    for (const auto& entry : ReadManagedDictionaryManifest()) {
      (*add_dictionary_row)(entry.name, entry.enabled);
    }
    (*update_empty_state)();
    SetIconChild(state_icon,
                 ImportedLexiconsStateIcon(
                     ReadBoolSetting(fp::kImportedLexiconsEnabledSetting, true)));
    RequestApplyInputConfigDeferred();
      });
  Grid::SetColumn(import_button, 1);
  header.Children().Append(import_button);

  Grid list_host;
  list_host.Height(kSettingsDialogListHeight);
  ScrollViewer scroller;
  scroller.Content(rows_panel);
  scroller.Height(kSettingsDialogListHeight);
  scroller.MinHeight(kSettingsDialogListHeight);
  scroller.MaxHeight(kSettingsDialogListHeight);
  scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
  scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
  scroller.VerticalScrollMode(ScrollMode::Enabled);
  scroller.HorizontalScrollMode(ScrollMode::Disabled);
  scroller.ZoomMode(ZoomMode::Disabled);
  scroller.BringIntoViewOnFocusChange(false);
  list_host.Children().Append(scroller);
  list_host.Children().Append(empty_state);

  Grid table;
  table.RowSpacing(6);
  RowDefinition header_row;
  header_row.Height(GridLengthHelper::Auto());
  RowDefinition list_row;
  list_row.Height(GridLengthHelper::Auto());
  table.RowDefinitions().Append(header_row);
  table.RowDefinitions().Append(list_row);
  Grid::SetRow(header, 0);
  table.Children().Append(header);
  Grid::SetRow(list_host, 1);
  table.Children().Append(list_host);

  Border table_frame = SettingsFrame(table,
                                     CurrentSettingsPalette().card,
                                     Radius(8),
                                     Thickness{10, 8, 10, 8});
  table_frame.HorizontalAlignment(HorizontalAlignment::Stretch);
  content.Children().Append(table_frame);

  dialog.Content(content);
  auto operation = dialog.ShowAsync();
  operation.Completed([rows, state_icon](auto const& async, auto const& status) {
    if (status != Windows::Foundation::AsyncStatus::Completed ||
        async.GetResults() != ContentDialogResult::Primary) {
      return;
    }

    std::vector<ManagedDictionaryEntry> entries;
    for (const auto& row : *rows) {
      if (!row) {
        continue;
      }
      if (row->removed) {
        RemoveManagedDictionaryFile(row->name);
        continue;
      }
      if (IsSafeRimeDictName(row->name)) {
        entries.push_back({row->name, row->toggle.IsOn()});
      }
    }
    WriteManagedDictionaryIntegrationFiles(entries);
    SetIconChild(state_icon,
                 ImportedLexiconsStateIcon(
                     ReadBoolSetting(fp::kImportedLexiconsEnabledSetting, true)));
    RequestApplyInputConfigDeferred();
  });
}

void ShowPhraseLexiconDialog(XamlRoot const& xaml_root,
                            std::wstring_view title_text,
                             std::wstring_view description_text,
                             bool user_lexicon,
                             Grid const& state_icon) {
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
  content.Children().Append(Text(title_text, 20, FW_SEMIBOLD));
  auto description = Text(description_text, 12);
  description.Foreground(SettingsSecondaryTextBrush());
  content.Children().Append(description);

  StackPanel rows_panel;
  rows_panel.Spacing(6);
  rows_panel.Margin(Thickness{0, 0, 16, 0});
  auto rows = std::make_shared<std::vector<std::shared_ptr<LexiconPhraseRow>>>();
  auto update_empty_state = std::make_shared<std::function<void()>>();
  Grid empty_state;
  empty_state.Height(kSettingsDialogListHeight);
  empty_state.HorizontalAlignment(HorizontalAlignment::Stretch);
  empty_state.VerticalAlignment(VerticalAlignment::Stretch);
  StackPanel empty_text;
  empty_text.Spacing(4);
  empty_text.HorizontalAlignment(HorizontalAlignment::Center);
  empty_text.VerticalAlignment(VerticalAlignment::Center);
  auto empty_title = Text(L"暂无内容", 14, FW_SEMIBOLD);
  empty_title.HorizontalTextAlignment(TextAlignment::Center);
  empty_text.Children().Append(empty_title);
  auto empty_note = Text(L"点击“添加”创建一条记录。", 12);
  empty_note.Foreground(SettingsSecondaryTextBrush());
  empty_note.HorizontalTextAlignment(TextAlignment::Center);
  empty_text.Children().Append(empty_note);
  empty_state.Children().Append(empty_text);

  *update_empty_state = [rows, empty_state]() {
    const bool has_visible_row =
        std::any_of(rows->begin(), rows->end(), [](const auto& row) {
          return row && !row->removed;
        });
    empty_state.Visibility(has_visible_row ? Visibility::Collapsed : Visibility::Visible);
  };

  auto add_row = std::make_shared<std::function<void(PhraseEntry)>>();
  *add_row = [rows_panel, rows, update_empty_state](PhraseEntry entry) {
    auto row_state = std::make_shared<LexiconPhraseRow>();
    Grid row;
    row.MinHeight(32);
    row.ColumnSpacing(8);
    row.HorizontalAlignment(HorizontalAlignment::Stretch);
    const double weights[] = {1.4, 1.0};
    for (int index = 0; index < 4; ++index) {
      ColumnDefinition column;
      if (index < 2) {
        column.Width(GridLengthHelper::FromValueAndType(weights[index], GridUnitType::Star));
      } else {
        column.Width(GridLengthHelper::FromPixels(index == 2 ? 70 : 36));
      }
      row.ColumnDefinitions().Append(column);
    }

    TextBox phrase;
    ApplySettingsUIFont(phrase);
    phrase.MinWidth(130);
    phrase.MinHeight(32);
    phrase.Height(32);
    phrase.PlaceholderText(L"词条");
    phrase.Text(entry.phrase);
    phrase.HorizontalAlignment(HorizontalAlignment::Stretch);
    Grid::SetColumn(phrase, 0);
    row.Children().Append(phrase);

    TextBox code;
    ApplySettingsUIFont(code);
    code.MinWidth(96);
    code.MinHeight(32);
    code.Height(32);
    code.PlaceholderText(L"编码");
    code.Text(entry.code);
    code.HorizontalAlignment(HorizontalAlignment::Stretch);
    Grid::SetColumn(code, 1);
    row.Children().Append(code);

    TextBox weight;
    ApplySettingsUIFont(weight);
    weight.MinWidth(58);
    weight.MinHeight(32);
    weight.Height(32);
    weight.PlaceholderText(L"权重");
    weight.Text(NormalizePhraseWeight(entry.weight));
    weight.HorizontalAlignment(HorizontalAlignment::Stretch);
    Grid::SetColumn(weight, 2);
    row.Children().Append(weight);

    row_state->row = row;
    row_state->phrase = phrase;
    row_state->code = code;
    row_state->weight = weight;
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
    Grid::SetColumn(remove, 3);
    row.Children().Append(remove);

    rows->push_back(row_state);
    rows_panel.Children().Append(row);
    (*update_empty_state)();
  };

  auto add_button = StableInlinePathActionButton(
      L"添加",
      kFluentIconAddCircle20RegularPath,
      0.66,
      [add_row]() {
    if (*add_row) {
      (*add_row)(PhraseEntry{});
    }
      });

  Grid header;
  header.ColumnSpacing(12);
  header.MinHeight(34);
  ColumnDefinition header_title;
  header_title.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  ColumnDefinition header_action;
  header_action.Width(GridLengthHelper::Auto());
  header.ColumnDefinitions().Append(header_title);
  header.ColumnDefinitions().Append(header_action);
  auto table_title = Text(user_lexicon ? L"用户词条" : L"自定义短语", 13, FW_SEMIBOLD);
  table_title.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(table_title, 0);
  header.Children().Append(table_title);
  Grid::SetColumn(add_button, 1);
  header.Children().Append(add_button);

  for (const auto& entry : (user_lexicon ? ReadUserLexiconEntries()
                                         : ReadCustomPhraseEntries())) {
    (*add_row)(entry);
  }
  (*update_empty_state)();

  Grid list_host;
  list_host.Height(kSettingsDialogListHeight);
  ScrollViewer scroller;
  scroller.Content(rows_panel);
  scroller.Height(kSettingsDialogListHeight);
  scroller.MinHeight(kSettingsDialogListHeight);
  scroller.MaxHeight(kSettingsDialogListHeight);
  scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
  scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
  scroller.VerticalScrollMode(ScrollMode::Enabled);
  scroller.HorizontalScrollMode(ScrollMode::Disabled);
  scroller.ZoomMode(ZoomMode::Disabled);
  scroller.BringIntoViewOnFocusChange(false);
  list_host.Children().Append(scroller);
  list_host.Children().Append(empty_state);

  Grid table;
  table.RowSpacing(6);
  for (int index = 0; index < 2; ++index) {
    RowDefinition row;
    row.Height(GridLengthHelper::Auto());
    table.RowDefinitions().Append(row);
  }
  Grid::SetRow(header, 0);
  table.Children().Append(header);
  Grid::SetRow(list_host, 1);
  table.Children().Append(list_host);
  content.Children().Append(SettingsFrame(table,
                                         CurrentSettingsPalette().card,
                                         Radius(8),
                                         Thickness{10, 8, 10, 8}));

  dialog.Content(content);
  auto operation = dialog.ShowAsync();
  operation.Completed([rows, user_lexicon, state_icon](auto const& async, auto const& status) {
    if (status != Windows::Foundation::AsyncStatus::Completed ||
        async.GetResults() != ContentDialogResult::Primary) {
      return;
    }
    std::vector<PhraseEntry> entries;
    for (const auto& row : *rows) {
      if (!row || row->removed) {
        continue;
      }
      PhraseEntry entry;
      entry.phrase = TrimLexiconText(row->phrase.Text().c_str());
      entry.code = TrimLexiconText(row->code.Text().c_str());
      entry.weight = NormalizePhraseWeight(row->weight.Text().c_str());
      if (!entry.phrase.empty() && !entry.code.empty()) {
        entries.push_back(std::move(entry));
      }
    }
    if (user_lexicon) {
      WriteUserLexiconEntries(entries);
      WriteManagedDictionaryIntegrationFiles(ReadManagedDictionaryManifest());
      SetIconChild(state_icon,
                   UserLexiconStateIcon(
                       ReadBoolSetting(fp::kUserLexiconEnabledSetting, true)));
    } else {
      WriteCustomPhraseEntries(entries);
      SetIconChild(state_icon,
                   CustomPhrasesStateIcon(
                       ReadBoolSetting(fp::kCustomPhrasesEnabledSetting, true)));
    }
    RequestApplyInputConfigDeferred();
  });
}

}  // namespace fp::config_winui
