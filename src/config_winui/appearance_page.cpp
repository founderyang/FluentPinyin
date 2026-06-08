#include "config_winui/appearance_page.h"

#include "common/constants.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_options.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/settings_value_controls.h"
#include "config_winui/status_tip_blacklist_dialog.h"
#include "config_winui/theme_helpers.h"
#include "config_winui/toolbar_visibility_state.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace fp::config_winui {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

UIElement BuildAppearancePage(XamlRoot const& xaml_root,
                              std::function<void()> show_theme_preset,
                              std::function<void()> apply_theme_refresh) {
  auto page = PageShell(L"外观", L"候选窗口、状态提示、工具栏和主题。");
  page.Children().Append(SectionHeader(L"候选窗口", true));
  auto candidate_font_icon = TextIcon(
      CandidateFontIconText(ReadStringSetting(fp::kCandidateFontFamilySetting,
                                              fp::kDefaultCandidateFontFamily)));
  page.Children().Append(SettingRowWithIcon(
      L"候选项字体",
      L"切换候选窗口字体。",
      CandidateFontFamilyCombo(candidate_font_icon),
      candidate_font_icon,
      L"已联动"));
  page.Children().Append(SettingRowWithIcon(
      L"候选项字体大小",
      L"按小、中、大、特大档位调整。",
      CandidateFontSizeCombo(),
      CandidateFontSizeIcon(),
      L"已联动"));
  page.Children().Append(SectionHeader(L"桌面显示"));
  Grid toolbar_icon;
  toolbar_icon.Width(kSettingIconHostSize);
  toolbar_icon.Height(kSettingIconHostSize);
  toolbar_icon.HorizontalAlignment(HorizontalAlignment::Center);
  toolbar_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(toolbar_icon,
               ToolbarVisibleIcon(ReadBoolSettingMigrated(fp::kToolbarVisibleSetting,
                                                          false,
                                                          fp::kLegacyToolbarVisibleSetting)));
  auto toolbar_switch = ToolbarVisibleSwitch([toolbar_icon](bool visible) {
    SetIconChild(toolbar_icon, ToolbarVisibleIcon(visible));
    if (!visible) {
      RequestToolbarHostShutdown();
    }
    RequestToolbarHostRefresh();
  });
  toolbar_switch.Toggled([toolbar_switch, toolbar_icon](auto const&, auto const&) {
    SetIconChild(toolbar_icon, ToolbarVisibleIcon(toolbar_switch.IsOn()));
  });
  RegisterToolbarVisibleSwitch(toolbar_switch);
  page.Children().Append(SettingRowWithIcon(L"输入法工具栏",
                                            L"在桌面显示可拖动的输入法工具栏。",
                                            toolbar_switch,
                                            toolbar_icon,
                                            L"已联动"));
  Grid status_tip_icon;
  status_tip_icon.Width(kSettingIconHostSize);
  status_tip_icon.Height(kSettingIconHostSize);
  status_tip_icon.HorizontalAlignment(HorizontalAlignment::Center);
  status_tip_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(status_tip_icon,
               StatusTipEnabledIcon(ReadBoolSetting(fp::kStatusTipEnabledSetting, true)));
  page.Children().Append(SettingRowWithIcon(
      L"输入状态提示",
      L"切换输入状态时，在鼠标附近显示紧凑提示。",
      SettingSwitch(fp::kStatusTipEnabledSetting,
                    true,
                    [status_tip_icon](bool enabled) {
                      SetIconChild(status_tip_icon, StatusTipEnabledIcon(enabled));
                      RequestInputStateRefreshDeferred(80);
                    }),
      status_tip_icon,
      L"已联动"));
  auto status_tip_blacklist_edit = StableIconToolButton(
      kFluentIconCommentEdit20RegularPath,
      L"编辑黑名单",
      0.92,
      [xaml_root]() {
        TextBlock unused_count_icon;
        ShowStatusTipBlacklistDialog(xaml_root, unused_count_icon);
      });
  page.Children().Append(SettingRowWithIcon(
      L"状态提示黑名单",
      L"对指定进程关闭状态提示。",
      status_tip_blacklist_edit,
      StatusTipBlacklistIcon(),
      L"已联动"));
  page.Children().Append(SectionHeader(L"主题和材质"));
  Grid theme_icon;
  theme_icon.Width(kSettingIconHostSize);
  theme_icon.Height(kSettingIconHostSize);
  theme_icon.HorizontalAlignment(HorizontalAlignment::Center);
  theme_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(theme_icon, ThemeModeIcon(CurrentThemeModeSetting()));
  ComboBox theme_combo;
  ConfigureSettingCombo(theme_combo);
  const std::array<std::wstring_view, 4> theme_labels{L"跟随系统", L"浅色", L"深色", L"预设"};
  for (const auto label : theme_labels) {
    theme_combo.Items().Append(ThemeComboItem(label));
  }
  auto suppress_theme_selection = std::make_shared<bool>(false);
  theme_combo.SelectedIndex(ThemeModeIndex(CurrentThemeModeSetting()));
  theme_combo.SelectionChanged([theme_combo,
                                theme_icon,
                                suppress_theme_selection,
                                show_theme_preset,
                                apply_theme_refresh](auto const&, auto const&) {
    if (*suppress_theme_selection) {
      return;
    }
    const int selected = theme_combo.SelectedIndex();
    if (selected < 0) {
      return;
    }
    const std::wstring value = ThemeModeValueForIndex(selected);
    if (value == fp::kThemeModeCustom) {
      show_theme_preset();
      return;
    }
    WriteStringSetting(fp::kThemeModeSetting, value);
    WriteStringSetting(fp::kLegacyThemeSetting, value);
    WriteStringSetting(fp::kThemePresetSetting, DefaultPresetForThemeMode(value));
    SetIconChild(theme_icon, ThemeModeIcon(value));
    RequestInputStateRefreshDeferred(80);
    RequestToolbarHostRefresh();
    apply_theme_refresh();
  });
  auto theme_preset_button = StableIconToolButton(
      kFluentIconKeyboard20RegularPath,
      L"预设",
      1.02,
      [show_theme_preset]() { show_theme_preset(); });
  StackPanel theme_controls;
  theme_controls.Orientation(Orientation::Horizontal);
  theme_controls.Spacing(8);
  theme_controls.HorizontalAlignment(HorizontalAlignment::Right);
  theme_controls.VerticalAlignment(VerticalAlignment::Center);
  theme_controls.Children().Append(theme_combo);
  theme_controls.Children().Append(theme_preset_button);
  page.Children().Append(SettingRowWithIcon(
      L"主题",
      L"选择系统、浅色、深色或预设。",
      theme_controls,
      theme_icon,
      L"已联动",
      176.0));
  return Scroll(page);
}

}  // namespace fp::config_winui
