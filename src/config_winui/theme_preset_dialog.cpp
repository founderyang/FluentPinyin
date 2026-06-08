#include "config_winui/theme_preset_dialog.h"

#include "common/constants.h"
#include "common/theme.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/theme_helpers.h"

#include <windows.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

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

}  // namespace

void ShowThemePresetDialog(XamlRoot const& xaml_root,
                           std::function<void()> on_applied) {
  constexpr double kThemePresetDialogWidth = 500.0;
  constexpr double kThemePresetPreviewWidth = 176.0;
  constexpr double kThemePresetPreviewHeight = 78.0;
  constexpr double kThemePresetCardHeight = 96.0;
  constexpr double kThemePresetListHeight = 232.0;

  ContentDialog dialog;
  ApplySettingsDialogBase(dialog, xaml_root);
  dialog.PrimaryButtonText(L"完成");
  dialog.CloseButtonText(L"取消");
  dialog.DefaultButton(ContentDialogButton::Primary);
  ApplySettingsResources(dialog.Resources());
  dialog.Resources().Insert(box_value(L"ContentDialogPadding"),
                            box_value(Thickness{22, 18, 22, 10}));
  dialog.Resources().Insert(box_value(L"ContentDialogCommandSpaceMargin"),
                            box_value(Thickness{0, 8, 0, 0}));
  dialog.Resources().Insert(box_value(L"ContentDialogTitleMargin"),
                            box_value(Thickness{0, 0, 0, 0}));

  StackPanel content;
  content.Width(kThemePresetDialogWidth);
  content.MaxWidth(kThemePresetDialogWidth);
  content.Spacing(12);
  content.HorizontalAlignment(HorizontalAlignment::Stretch);
  content.Children().Append(Text(L"输入法预设", 22, FW_SEMIBOLD));

  auto selected = std::make_shared<std::wstring>(CurrentThemePresetSetting());
  auto frames = std::make_shared<std::vector<std::pair<std::wstring, Border>>>();
  auto update_visuals = std::make_shared<std::function<void()>>();
  auto apply_card_visual = [selected](std::wstring_view id, Border const& frame) {
    const auto palette = CurrentSettingsPalette();
    const bool is_selected = *selected == id;
    frame.BorderBrush(SettingsBorderBrush());
    frame.Background(is_selected ? Brush(30,
                                         palette.accent.red,
                                         palette.accent.green,
                                         palette.accent.blue)
                                 : Brush(palette.card));
  };

  Grid grid;
  grid.ColumnSpacing(10);
  grid.RowSpacing(10);
  grid.Margin(Thickness{0, 0, 16, 0});
  for (int column = 0; column < 2; ++column) {
    ColumnDefinition definition;
    definition.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    grid.ColumnDefinitions().Append(definition);
  }
  for (int row = 0; row < 4; ++row) {
    RowDefinition definition;
    definition.Height(GridLengthHelper::Auto());
    grid.RowDefinitions().Append(definition);
  }

  for (size_t index = 0; index < fp::kThemePresetDefinitions.size(); ++index) {
    const auto preset = fp::kThemePresetDefinitions[index];
    Border frame = SettingsFrame(ThemePreviewArtwork(preset.id,
                                                     kThemePresetPreviewWidth,
                                                     kThemePresetPreviewHeight,
                                                     true),
                                 CurrentSettingsPalette().card,
                                 Radius(8),
                                 UniformThickness(5));
    frame.HorizontalAlignment(HorizontalAlignment::Stretch);
    frame.Height(kThemePresetCardHeight);
    frame.UseLayoutRounding(true);
    SetSettingsToolTip(frame, preset.label);
    apply_card_visual(preset.id, frame);

    frame.Tapped([selected,
                  frames,
                  update_visuals,
                  id = std::wstring(preset.id)](auto const&, auto const&) {
      *selected = id;
      if (*update_visuals) {
        (*update_visuals)();
      }
    });
    frame.PointerEntered([frame](auto const&, auto const&) {
      frame.Opacity(0.94);
    });
    frame.PointerExited([frame](auto const&, auto const&) {
      frame.Opacity(1.0);
    });

    frames->push_back({std::wstring(preset.id), frame});
    Grid::SetColumn(frame, static_cast<int>(index % 2));
    Grid::SetRow(frame, static_cast<int>(index / 2));
    grid.Children().Append(frame);
  }

  *update_visuals = [frames, apply_card_visual]() {
    for (const auto& [id, frame] : *frames) {
      apply_card_visual(id, frame);
    }
  };

  ScrollViewer scroller;
  scroller.Content(grid);
  scroller.Height(kThemePresetListHeight);
  scroller.MaxHeight(kThemePresetListHeight);
  scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Visible);
  scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
  scroller.VerticalScrollMode(ScrollMode::Enabled);
  scroller.HorizontalScrollMode(ScrollMode::Disabled);
  content.Children().Append(scroller);
  dialog.Content(content);
  auto operation = dialog.ShowAsync();
  operation.Completed([dialog, selected, on_applied = std::move(on_applied)](
                          auto const& async, auto const& status) {
    if (status != Windows::Foundation::AsyncStatus::Completed ||
        async.GetResults() != ContentDialogResult::Primary) {
      return;
    }
    WriteStringSetting(fp::kThemePresetSetting, *selected);
    WriteStringSetting(fp::kThemeModeSetting, fp::kThemeModeCustom);
    WriteStringSetting(fp::kLegacyThemeSetting, fp::kThemeModeCustom);
    RequestInputStateRefreshDeferred(80);
    RequestToolbarHostRefresh();
    if (on_applied) {
      on_applied();
    }
  });
}

}  // namespace fp::config_winui
