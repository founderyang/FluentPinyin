#include "config_winui/general_page.h"

#include "common/candidate_font.h"
#include "common/constants.h"
#include "config_winui/candidate_layout_settings.h"
#include "config_winui/default_settings.h"
#include "config_winui/fuzzy_pinyin_rules_dialog.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_options.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/settings_value_controls.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace fp::config_winui {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

UIElement BuildGeneralPage(XamlRoot const& xaml_root,
                           std::function<void()> reset_defaults) {
  auto page = PageShell(L"常规", L"输入、候选、拼音辅助和默认设置。");
  auto input_scheme_icon =
      TextIcon(InputSchemeIconText(
          ReadStringSetting(fp::kInputSchemeSetting, fp::kDefaultInputScheme)));
  auto double_pinyin_scheme_icon =
      TextIcon(ChoiceIconText(
          DoublePinyinSchemeChoices(),
          ReadStringSetting(fp::kDoublePinyinSchemeSetting,
                            fp::kDefaultDoublePinyinScheme)));
  auto double_pinyin_scheme = StringChoiceComboWithIcon(
      fp::kDoublePinyinSchemeSetting,
      DoublePinyinSchemeChoices(),
      fp::kDefaultDoublePinyinScheme,
      double_pinyin_scheme_icon,
      L"",
      true);
  auto double_pinyin_scheme_row = SettingRowWithIcon(
      L"双拼方案",
      L"仅在双拼下生效。",
      double_pinyin_scheme,
      double_pinyin_scheme_icon,
      L"已联动");
  double_pinyin_scheme_row.Visibility(
      ReadStringSetting(fp::kInputSchemeSetting,
                        fp::kDefaultInputScheme) ==
              std::wstring(fp::kInputSchemeDoublePinyin)
          ? Visibility::Visible
          : Visibility::Collapsed);
  page.Children().Append(SectionHeader(L"拼音设置", true));
  page.Children().Append(SettingRowWithIcon(
      L"输入方案",
      L"选择全拼或双拼。",
      InputSchemeCombo(double_pinyin_scheme,
                       [input_scheme_icon, double_pinyin_scheme_row](std::wstring_view value) {
                         input_scheme_icon.Text(InputSchemeIconText(value));
                         double_pinyin_scheme_row.Visibility(
                             value == fp::kInputSchemeDoublePinyin ? Visibility::Visible
                                                                   : Visibility::Collapsed);
                       }),
      input_scheme_icon,
      L"已联动"));
  page.Children().Append(double_pinyin_scheme_row);
  page.Children().Append(SectionHeader(L"默认模式"));
  auto input_mode_icon = TextIcon(DefaultInputModeIconText());
  page.Children().Append(SettingRowWithIcon(
      L"输入模式",
      L"设置默认中文或英文状态。",
      StringChoiceCombo(fp::kDefaultInputModeSetting,
                        DefaultInputModeChoices(),
                        fp::kDefaultInputMode,
                        L"",
                        false,
                        [input_mode_icon](std::wstring_view value) {
                          input_mode_icon.Text(DefaultInputModeChoiceIconText(value));
                          RequestInputStateRefresh();
                          RequestToolbarHostRefresh();
                        }),
      input_mode_icon,
      L"已联动"));
  auto charset_icon = TextIcon(DefaultCharsetIconText());
  page.Children().Append(SettingRowWithIcon(
      L"输入字符",
      L"设置默认输出简体或繁体。",
      StringChoiceCombo(fp::kDefaultCharsetSetting,
                        DefaultCharsetChoices(),
                        fp::kDefaultCharset,
                        L"",
                        false,
                        [charset_icon](std::wstring_view value) {
                          charset_icon.Text(DefaultCharsetChoiceIconText(value));
                          RequestInputStateRefresh();
                          RequestToolbarHostRefresh();
                        }),
      charset_icon,
      L"已联动"));
  Grid shape_icon;
  shape_icon.Width(kSettingIconHostSize);
  shape_icon.Height(kSettingIconHostSize);
  shape_icon.HorizontalAlignment(HorizontalAlignment::Center);
  shape_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(shape_icon,
               ShapeStatusIcon(!ReadBoolSetting(fp::kDefaultShapeHalfSetting, true)));
  page.Children().Append(SettingRowWithIcon(
      L"全角/半角",
      L"设置默认全角或半角状态。",
      BoolChoiceCombo(fp::kDefaultShapeHalfSetting,
                      DefaultShapeChoices(),
                      true,
                      [shape_icon](bool half_shape) {
                        SetIconChild(shape_icon, ShapeStatusIcon(!half_shape));
                        RequestInputStateRefresh();
                        RequestToolbarHostRefresh();
                      }),
      shape_icon,
      L"已联动"));
  Grid punctuation_icon;
  punctuation_icon.Width(kSettingIconHostSize);
  punctuation_icon.Height(kSettingIconHostSize);
  punctuation_icon.HorizontalAlignment(HorizontalAlignment::Center);
  punctuation_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(punctuation_icon,
               PunctuationStatusIcon(
                   ReadBoolSetting(fp::kDefaultChinesePunctuationSetting, true)));
  page.Children().Append(SettingRowWithIcon(
      L"标点符号",
      L"设置默认中文或英文标点。",
      BoolChoiceCombo(fp::kDefaultChinesePunctuationSetting,
                      DefaultPunctuationChoices(),
                      true,
                      [punctuation_icon](bool chinese_punctuation) {
                        SetIconChild(punctuation_icon, PunctuationStatusIcon(chinese_punctuation));
                        RequestInputStateRefresh();
                        RequestToolbarHostRefresh();
                      }),
      punctuation_icon,
      L"已联动"));
  page.Children().Append(SectionHeader(L"候选窗口"));
  auto candidate_count_icon =
      TextIcon(std::to_wstring(ReadIntSetting(fp::kCandidateCountSetting,
                                              fp::kDefaultCandidateCount,
                                              fp::kMinCandidateCount,
                                              fp::kMaxCandidateCount)));
  page.Children().Append(SettingRowWithIcon(L"候选词数",
                                            L"设置每页候选数量，范围 3 到 9。",
                                            CandidateCountCombo(candidate_count_icon),
                                            candidate_count_icon,
                                            L"已联动"));
  Grid candidate_layout_icon;
  candidate_layout_icon.Width(kSettingIconHostSize);
  candidate_layout_icon.Height(kSettingIconHostSize);
  candidate_layout_icon.HorizontalAlignment(HorizontalAlignment::Center);
  candidate_layout_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(candidate_layout_icon,
               CandidateLayoutIcon(ReadCandidateLayoutSetting(fp::kDefaultCandidateLayout)));
  page.Children().Append(SettingRowWithIcon(
      L"候选排列",
      L"选择横排或竖排候选窗口。",
      StringChoiceCombo(fp::kCandidateLayoutSetting,
                        CandidateLayoutChoices(),
                        fp::kDefaultCandidateLayout,
                        L"",
                        false,
                        [candidate_layout_icon](std::wstring_view value) {
                          SetIconChild(candidate_layout_icon, CandidateLayoutIcon(value));
                          RequestCandidateWindowVisualRefresh();
                          RequestCandidateWindowVisualRefreshDeferred(120);
                        }),
      candidate_layout_icon,
      L"已联动"));
  page.Children().Append(SectionHeader(L"拼音辅助"));
  Grid auto_correction_icon;
  auto_correction_icon.Width(kSettingIconHostSize);
  auto_correction_icon.Height(kSettingIconHostSize);
  auto_correction_icon.HorizontalAlignment(HorizontalAlignment::Center);
  auto_correction_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(auto_correction_icon,
               AutoPinyinCorrectionIcon(
                   ReadBoolSetting(fp::kAutoPinyinCorrectionSetting, true)));
  page.Children().Append(SettingRowWithIcon(
      L"自动拼音纠错",
      L"修正常见拼写偏差。",
      SettingSwitch(fp::kAutoPinyinCorrectionSetting,
                    true,
                    [auto_correction_icon](bool enabled) {
                      SetIconChild(auto_correction_icon, AutoPinyinCorrectionIcon(enabled));
                      RequestPinyinConfigRestart(enabled);
                    }),
      auto_correction_icon,
      L"已联动"));
  Grid super_abbrev_icon;
  super_abbrev_icon.Width(kSettingIconHostSize);
  super_abbrev_icon.Height(kSettingIconHostSize);
  super_abbrev_icon.HorizontalAlignment(HorizontalAlignment::Center);
  super_abbrev_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(super_abbrev_icon,
               SuperAbbrevIcon(ReadBoolSetting(fp::kSuperAbbrevSetting, true)));
  page.Children().Append(SettingRowWithIcon(
      L"超级简拼",
      L"使用更短的声母组合快速出词。",
      SettingSwitch(fp::kSuperAbbrevSetting,
                    true,
                    [super_abbrev_icon](bool enabled) {
                      SetIconChild(super_abbrev_icon, SuperAbbrevIcon(enabled));
                      RequestRimeOptionRefresh(enabled);
                    }),
      super_abbrev_icon,
      L"已联动"));
  Grid smart_fuzzy_icon;
  smart_fuzzy_icon.Width(kSettingIconHostSize);
  smart_fuzzy_icon.Height(kSettingIconHostSize);
  smart_fuzzy_icon.HorizontalAlignment(HorizontalAlignment::Center);
  smart_fuzzy_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(smart_fuzzy_icon,
               SmartFuzzyPinyinIcon(
                   ReadBoolSetting(fp::kSmartFuzzyPinyinSetting, true)));
  page.Children().Append(SettingRowWithIcon(
      L"智能模糊拼音",
      L"自动处理常见模糊音。",
      SettingSwitch(fp::kSmartFuzzyPinyinSetting,
                    true,
                    [smart_fuzzy_icon](bool enabled) {
                      SetIconChild(smart_fuzzy_icon, SmartFuzzyPinyinIcon(enabled));
                      RequestPinyinConfigRestart(enabled);
                    }),
      smart_fuzzy_icon,
      L"已联动"));
  Grid fuzzy_rules_icon;
  fuzzy_rules_icon.Width(kSettingIconHostSize);
  fuzzy_rules_icon.Height(kSettingIconHostSize);
  fuzzy_rules_icon.HorizontalAlignment(HorizontalAlignment::Center);
  fuzzy_rules_icon.VerticalAlignment(VerticalAlignment::Center);
  SetIconChild(fuzzy_rules_icon,
               FuzzyPinyinRulesStateIcon(
                   ReadBoolSetting(fp::kFuzzyPinyinSetting, false)));
  auto suppress_fuzzy_dialog = std::make_shared<bool>(false);
  ToggleSwitch fuzzy_rules_switch;
  ApplySettingsUIFont(fuzzy_rules_switch);
  fuzzy_rules_switch.MinWidth(0);
  fuzzy_rules_switch.HorizontalAlignment(HorizontalAlignment::Right);
  fuzzy_rules_switch.IsOn(ReadBoolSetting(fp::kFuzzyPinyinSetting, false));
  fuzzy_rules_switch.Toggled(
      [xaml_root, fuzzy_rules_switch, fuzzy_rules_icon, suppress_fuzzy_dialog](auto const&,
                                                                               auto const&) {
    WriteBoolSetting(fp::kFuzzyPinyinSetting, fuzzy_rules_switch.IsOn());
    SetIconChild(fuzzy_rules_icon, FuzzyPinyinRulesStateIcon(fuzzy_rules_switch.IsOn()));
    if (*suppress_fuzzy_dialog) {
      return;
    }
    if (fuzzy_rules_switch.IsOn()) {
      ShowFuzzyPinyinRulesDialog(xaml_root,
                                 fuzzy_rules_switch,
                                 fuzzy_rules_icon,
                                 true,
                                 suppress_fuzzy_dialog);
    } else {
      RequestApplyInputConfigDeferred();
    }
  });
  auto fuzzy_rules_edit = StableIconToolButton(
      kFluentIconEditSettings20RegularPath,
      L"编辑规则",
      0.92,
      [xaml_root, fuzzy_rules_switch, fuzzy_rules_icon, suppress_fuzzy_dialog]() {
        ShowFuzzyPinyinRulesDialog(xaml_root,
                                   fuzzy_rules_switch,
                                   fuzzy_rules_icon,
                                   false,
                                   suppress_fuzzy_dialog);
      });
  StackPanel fuzzy_rules_controls;
  fuzzy_rules_controls.Orientation(Orientation::Horizontal);
  fuzzy_rules_controls.Spacing(10);
  fuzzy_rules_controls.HorizontalAlignment(HorizontalAlignment::Right);
  fuzzy_rules_controls.Children().Append(fuzzy_rules_edit);
  fuzzy_rules_controls.Children().Append(fuzzy_rules_switch);
  page.Children().Append(SettingRowWithIcon(L"模糊拼音规则",
                                            L"选择启用的模糊音规则。",
                                            fuzzy_rules_controls,
                                            fuzzy_rules_icon,
                                            L"已联动",
                                            176));
  page.Children().Append(SectionHeader(L"恢复默认"));
  auto reset_button = StableIconToolButton(
      kFluentIconArrowClockwise20RegularPath,
      L"重置",
      0.92,
      [reset_defaults]() {
        reset_defaults();
      });
  page.Children().Append(SettingRowWithIcon(
      L"重置到默认状态",
      L"恢复设置，不清除词库。",
      reset_button,
      FluentPathIcon(kFluentIconApprovalsApp20RegularPath, 0.96),
      L"已联动"));
  return Scroll(page);
}

}  // namespace fp::config_winui
