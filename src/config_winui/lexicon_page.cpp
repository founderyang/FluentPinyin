#include "config_winui/lexicon_page.h"

#include "common/constants.h"
#include "config_winui/lexicon_dialogs.h"
#include "config_winui/lexicon_store.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

namespace fp::config_winui {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

UIElement LexiconControls(ToggleSwitch const& toggle, Border const& edit_button) {
  StackPanel controls;
  controls.Orientation(Orientation::Horizontal);
  controls.Spacing(10);
  controls.HorizontalAlignment(HorizontalAlignment::Right);
  controls.VerticalAlignment(VerticalAlignment::Center);
  controls.Children().Append(edit_button);
  controls.Children().Append(toggle);
  return controls;
}

UIElement BuildLexiconPage(HWND owner, XamlRoot const& xaml_root) {
  auto page = PageShell(L"词库", L"用户词、短语和导入词库。");

  page.Children().Append(SectionHeader(L"用户词库", true));
  Grid user_icon;
  user_icon.Width(kSettingIconHostSize);
  user_icon.Height(kSettingIconHostSize);
  user_icon.HorizontalAlignment(HorizontalAlignment::Center);
  user_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(user_icon,
               UserLexiconStateIcon(
                   ReadBoolSetting(fp::kUserLexiconEnabledSetting, true)));
  ToggleSwitch user_switch;
  ApplySettingsUIFont(user_switch);
  user_switch.MinWidth(0);
  user_switch.HorizontalAlignment(HorizontalAlignment::Right);
  user_switch.IsOn(ReadBoolSetting(fp::kUserLexiconEnabledSetting, true));
  user_switch.Toggled([user_switch, user_icon](auto const&, auto const&) {
    WriteBoolSetting(fp::kUserLexiconEnabledSetting, user_switch.IsOn());
    WriteManagedDictionaryIntegrationFiles(ReadManagedDictionaryManifest());
    SetIconChild(user_icon, UserLexiconStateIcon(user_switch.IsOn()));
    RequestApplyInputConfigDeferred();
  });
  auto user_edit = StableIconToolButton(
      kFluentIconCalendarEdit20RegularPath,
      L"编辑用户词库",
      0.92,
      [xaml_root, user_icon]() {
        ShowPhraseLexiconDialog(xaml_root,
                                L"用户词库",
                                L"管理手动添加的词条。",
                                true,
                                user_icon);
      });
  page.Children().Append(SettingRowWithIcon(
      L"用户词库",
      L"维护常用词条。",
      LexiconControls(user_switch, user_edit),
      user_icon,
      L"已联动",
      176));

  page.Children().Append(SectionHeader(L"自定义短语"));
  Grid phrase_icon;
  phrase_icon.Width(kSettingIconHostSize);
  phrase_icon.Height(kSettingIconHostSize);
  phrase_icon.HorizontalAlignment(HorizontalAlignment::Center);
  phrase_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(phrase_icon,
               CustomPhrasesStateIcon(
                   ReadBoolSetting(fp::kCustomPhrasesEnabledSetting, true)));
  ToggleSwitch phrase_switch;
  ApplySettingsUIFont(phrase_switch);
  phrase_switch.MinWidth(0);
  phrase_switch.HorizontalAlignment(HorizontalAlignment::Right);
  phrase_switch.IsOn(ReadBoolSetting(fp::kCustomPhrasesEnabledSetting, true));
  phrase_switch.Toggled([phrase_switch, phrase_icon](auto const&, auto const&) {
    WriteBoolSetting(fp::kCustomPhrasesEnabledSetting, phrase_switch.IsOn());
    SetIconChild(phrase_icon, CustomPhrasesStateIcon(phrase_switch.IsOn()));
    RequestApplyInputConfigDeferred();
  });
  auto phrase_edit = StableIconToolButton(
      kFluentIconDocumentEdit20RegularPath,
      L"编辑自定义短语",
      0.92,
      [xaml_root, phrase_icon]() {
        ShowPhraseLexiconDialog(xaml_root,
                                L"自定义短语",
                                L"管理固定编码短语。",
                                false,
                                phrase_icon);
      });
  page.Children().Append(SettingRowWithIcon(
      L"自定义短语",
      L"维护固定编码短语。",
      LexiconControls(phrase_switch, phrase_edit),
      phrase_icon,
      L"已联动",
      176));

  page.Children().Append(SectionHeader(L"导入和管理"));
  Grid import_icon;
  import_icon.Width(kSettingIconHostSize);
  import_icon.Height(kSettingIconHostSize);
  import_icon.HorizontalAlignment(HorizontalAlignment::Center);
  import_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(import_icon,
               ImportedLexiconsStateIcon(
                   ReadBoolSetting(fp::kImportedLexiconsEnabledSetting, true)));
  ToggleSwitch import_switch;
  ApplySettingsUIFont(import_switch);
  import_switch.MinWidth(0);
  import_switch.HorizontalAlignment(HorizontalAlignment::Right);
  import_switch.IsOn(ReadBoolSetting(fp::kImportedLexiconsEnabledSetting, true));
  import_switch.Toggled([import_switch, import_icon](auto const&, auto const&) {
    WriteBoolSetting(fp::kImportedLexiconsEnabledSetting, import_switch.IsOn());
    WriteManagedDictionaryIntegrationFiles(ReadManagedDictionaryManifest());
    SetIconChild(import_icon, ImportedLexiconsStateIcon(import_switch.IsOn()));
    RequestApplyInputConfigDeferred();
  });
  auto import_edit = StableIconToolButton(
      kFluentIconEdit20RegularPath,
      L"管理导入词库",
      0.92,
      [owner, xaml_root, import_icon]() {
        ShowManagedDictionariesDialog(xaml_root, owner, import_icon);
      });
  page.Children().Append(SettingRowWithIcon(
      L"导入词库",
      L"导入、启用或删除词库。",
      LexiconControls(import_switch, import_edit),
      import_icon,
      L"已联动",
      176));
  return Scroll(page);
}

}  // namespace fp::config_winui
