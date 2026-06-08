#include "config_winui/hotkeys_page.h"

#include "common/constants.h"
#include "config_winui/hotkey_recorder_controls.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <string>
#include <string_view>

namespace fp::config_winui {
namespace {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

Border HotkeyBindingRowWithIcon(std::wstring_view title,
                                std::wstring_view key,
                                std::wstring_view fallback,
                                UIElement const& icon_content) {
  StackPanel controls;
  controls.Orientation(Orientation::Horizontal);
  controls.Spacing(8);
  auto editor = HotkeyRecorderButton(key, fallback);
  controls.Children().Append(editor);

  auto clear = StableInlinePathActionButton(
      L"清除",
      kFluentIconDismissCircle20RegularPath,
      0.84,
      [editor, key = std::wstring(key)]() {
        WriteStringSetting(key, L"");
        SetHotkeyRecorderDisplay(editor, key, L"");
        RequestInputStateRefreshDeferred(80);
      });
  SetSettingsToolTip(clear, L"清除");
  controls.Children().Append(clear);

  auto reset = StableInlinePathActionButton(
      L"恢复默认",
      kFluentIconArrowClockwise20RegularPath,
      0.84,
      [editor, key = std::wstring(key), fallback = std::wstring(fallback)]() {
        WriteStringSetting(key, fallback);
        SetHotkeyRecorderDisplay(editor, key, fallback);
        RequestInputStateRefreshDeferred(80);
      });
  SetSettingsToolTip(reset, L"恢复默认");
  controls.Children().Append(reset);

  return SettingWideRowWithIcon(title,
                                L"点击后按下新的组合键，或恢复默认值。",
                                controls,
                                icon_content,
                                L"已联动");
}

}  // namespace

UIElement BuildHotkeysPage() {
  auto page = PageShell(L"热键", L"状态切换和候选导航。");
  page.Children().Append(SectionHeader(L"状态切换", true));
  page.Children().Append(HotkeyBindingRowWithIcon(
      L"中/英文模式",
      fp::kShortcutToolbarInputModeSetting,
      fp::kDefaultShortcutToolbarInputMode,
      TextIcon(L"中")));
  page.Children().Append(HotkeyBindingRowWithIcon(
      L"全/半角",
      fp::kShortcutToolbarShapeSetting,
      fp::kDefaultShortcutToolbarShape,
      ShapeStatusIcon(false)));
  page.Children().Append(HotkeyBindingRowWithIcon(
      L"中/英文标点",
      fp::kShortcutToolbarPunctuationSetting,
      fp::kDefaultShortcutToolbarPunctuation,
      PunctuationStatusIcon(true)));
  page.Children().Append(HotkeyBindingRowWithIcon(
      L"简体/繁体",
      fp::kShortcutToolbarCharsetSetting,
      fp::kDefaultShortcutToolbarCharset,
      TextIcon(L"简")));
  page.Children().Append(HotkeyBindingRowWithIcon(
      L"表情符号/符号",
      fp::kShortcutToolbarEmojiSetting,
      fp::kDefaultShortcutToolbarEmoji,
      EmojiStatusIcon()));
  page.Children().Append(SectionHeader(L"候选导航"));
  page.Children().Append(HotkeyBindingRowWithIcon(
      L"展开/收起候选框",
      fp::kShortcutCandidateExpandSetting,
      fp::kDefaultShortcutCandidateExpand,
      ChevronStatusIcon(kFluentChevronDown20Path)));
  page.Children().Append(HotkeyBindingRowWithIcon(
      L"上一页",
      fp::kShortcutCandidatePreviousPageSetting,
      fp::kDefaultShortcutCandidatePreviousPage,
      TriangleStatusIcon(false)));
  page.Children().Append(HotkeyBindingRowWithIcon(
      L"下一页",
      fp::kShortcutCandidateNextPageSetting,
      fp::kDefaultShortcutCandidateNextPage,
      TriangleStatusIcon(true)));
  return Scroll(page);
}

}  // namespace fp::config_winui
