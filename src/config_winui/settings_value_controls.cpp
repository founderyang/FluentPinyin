#include "config_winui/settings_value_controls.h"

#include "common/candidate_font.h"
#include "common/constants.h"
#include "config_winui/candidate_layout_settings.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_options.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fp::config_winui {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

ComboBox StringChoiceComboWithIcon(
    std::wstring_view key,
    const std::vector<std::pair<std::wstring, std::wstring>>& choices,
    std::wstring_view default_value,
    TextBlock const& icon_label,
    std::wstring_view initial_value,
    bool restart_input_core_on_change,
    std::function<std::wstring(std::wstring_view)> icon_text,
    std::function<void(std::wstring_view)> on_change);

ComboBox CandidateCountCombo() {
  const int current =
      ReadIntSetting(fp::kCandidateCountSetting,
                     fp::kDefaultCandidateCount,
                     fp::kMinCandidateCount,
                     fp::kMaxCandidateCount);
  std::vector<std::wstring> labels;
  for (int value = fp::kMinCandidateCount; value <= fp::kMaxCandidateCount; ++value) {
    labels.push_back(std::to_wstring(value));
  }
  return ChoiceCombo(labels, current - fp::kMinCandidateCount, [](int selected) {
    WriteIntSetting(fp::kCandidateCountSetting, fp::kMinCandidateCount + selected);
    RequestCandidateWindowVisualRefresh();
  });
}

ComboBox CandidateCountCombo(TextBlock const& icon_label) {
  const int current =
      ReadIntSetting(fp::kCandidateCountSetting,
                     fp::kDefaultCandidateCount,
                     fp::kMinCandidateCount,
                     fp::kMaxCandidateCount);
  icon_label.Text(std::to_wstring(current));
  std::vector<std::wstring> labels;
  for (int value = fp::kMinCandidateCount; value <= fp::kMaxCandidateCount; ++value) {
    labels.push_back(std::to_wstring(value));
  }
  return ChoiceCombo(labels, current - fp::kMinCandidateCount, [icon_label](int selected) {
    const int value = fp::kMinCandidateCount + selected;
    WriteIntSetting(fp::kCandidateCountSetting, value);
    icon_label.Text(std::to_wstring(value));
    RequestCandidateWindowVisualRefresh();
  });
}

ComboBox CandidateFontSizeCombo() {
  const int current = ReadIntSetting(fp::kCandidateFontSizeLevelSetting,
                                     fp::kDefaultCandidateFontSizeLevel,
                                     fp::kMinCandidateFontSizeLevel,
                                     fp::kMaxCandidateFontSizeLevel);
  std::vector<std::wstring> labels;
  labels.reserve(fp::kCandidateFontSizeLabels.size());
  for (const auto label : fp::kCandidateFontSizeLabels) {
    labels.emplace_back(label);
  }
  return ChoiceCombo(labels, current, [](int selected) {
    if (selected < fp::kMinCandidateFontSizeLevel ||
        selected > fp::kMaxCandidateFontSizeLevel) {
      return;
    }
    WriteIntSetting(fp::kCandidateFontSizeLevelSetting, selected);
    RequestCandidateWindowVisualRefresh();
    RequestCandidateWindowVisualRefreshDeferred(120);
  });
}

ComboBox CandidateFontFamilyCombo(TextBlock const& icon_label) {
  return StringChoiceComboWithIcon(
      fp::kCandidateFontFamilySetting,
      {{L"MiSans", std::wstring(fp::kDefaultCandidateFontFamily)},
       {L"思源黑体", std::wstring(fp::kSourceHanSansCandidateFontFamily)}},
      fp::kDefaultCandidateFontFamily,
      icon_label,
      L"",
      false,
      CandidateFontIconText,
      [](std::wstring_view) {
        RequestCandidateWindowVisualRefresh();
        RequestCandidateWindowVisualRefreshDeferred(120);
      });
}

ComboBox IntChoiceCombo(std::wstring_view key,
                        const std::vector<std::wstring>& labels,
                        int default_value,
                        int min_value,
                        int max_value) {
  const int current = ReadIntSetting(key, default_value, min_value, max_value);
  return ChoiceCombo(labels, current, [key = std::wstring(key)](int selected) {
    WriteIntSetting(key, selected);
  });
}

ComboBox IntChoiceCombo(std::wstring_view key,
                        const std::vector<std::wstring>& labels,
                        int default_value,
                        int min_value,
                        int max_value,
                        TextBlock const& icon_label) {
  const int current = ReadIntSetting(key, default_value, min_value, max_value);
  icon_label.Text(IntChoiceIconText(labels, current));
  return ChoiceCombo(labels, current, [key = std::wstring(key), labels, icon_label](int selected) {
    WriteIntSetting(key, selected);
    icon_label.Text(IntChoiceIconText(labels, selected));
  });
}

ComboBox StringChoiceCombo(std::wstring_view key,
                           const std::vector<std::pair<std::wstring, std::wstring>>& choices,
                           std::wstring_view default_value,
                           std::wstring_view initial_value,
                           bool restart_input_core_on_change,
                           std::function<void(std::wstring_view)> on_change) {
  const std::wstring current =
      !initial_value.empty()
          ? std::wstring(initial_value)
          : (key == fp::kCandidateLayoutSetting ? ReadCandidateLayoutSetting(default_value)
                                                : ReadStringSetting(key, default_value));
  std::vector<std::wstring> labels;
  int selected_index = 0;
  for (size_t index = 0; index < choices.size(); ++index) {
    labels.push_back(choices[index].first);
    if (choices[index].second == current) {
      selected_index = static_cast<int>(index);
    }
  }

  return ChoiceCombo(labels,
                     selected_index,
                     [choices,
                      key = std::wstring(key),
                      restart_input_core_on_change,
                      on_change](int selected) {
    if (selected < 0 || selected >= static_cast<int>(choices.size())) {
      return;
    }
    const std::wstring value = choices[static_cast<size_t>(selected)].second;
    const std::wstring previous =
        key == fp::kCandidateLayoutSetting
            ? ReadCandidateLayoutSetting(fp::kDefaultCandidateLayout)
            : ReadStringSetting(key);
    if (key == fp::kCandidateLayoutSetting) {
      WriteCandidateLayoutSetting(value);
    } else {
      WriteStringSetting(key, value);
    }
    const bool active_double_pinyin_setting =
        key != fp::kDoublePinyinSchemeSetting ||
        ReadStringSetting(fp::kInputSchemeSetting, fp::kDefaultInputScheme) ==
            std::wstring(fp::kInputSchemeDoublePinyin);
    if (restart_input_core_on_change && active_double_pinyin_setting && previous != value) {
      RequestApplyInputConfigDeferred();
    }
    if (on_change) {
      on_change(value);
    }
  });
}

ComboBox StringChoiceComboWithIcon(
    std::wstring_view key,
    const std::vector<std::pair<std::wstring, std::wstring>>& choices,
    std::wstring_view default_value,
    TextBlock const& icon_label,
    std::wstring_view initial_value,
    bool restart_input_core_on_change,
    std::function<std::wstring(std::wstring_view)> icon_text,
    std::function<void(std::wstring_view)> on_change) {
  auto resolve_icon = [choices, icon_text](std::wstring_view value) {
    return icon_text ? icon_text(value) : ChoiceIconText(choices, value);
  };
  const std::wstring current =
      !initial_value.empty()
          ? std::wstring(initial_value)
          : (key == fp::kCandidateLayoutSetting ? ReadCandidateLayoutSetting(default_value)
                                                : ReadStringSetting(key, default_value));
  icon_label.Text(resolve_icon(current));
  return StringChoiceCombo(key,
                           choices,
                           default_value,
                           initial_value,
                           restart_input_core_on_change,
                           [icon_label, resolve_icon, on_change](std::wstring_view value) {
                             icon_label.Text(resolve_icon(value));
                             if (on_change) {
                               on_change(value);
                             }
                           });
}

ComboBox BoolChoiceCombo(std::wstring_view key,
                         const std::vector<std::pair<std::wstring, bool>>& choices,
                         bool default_value,
                         std::function<void(bool)> on_change) {
  const bool current = ReadBoolSetting(key, default_value);
  std::vector<std::wstring> labels;
  int selected_index = 0;
  for (size_t index = 0; index < choices.size(); ++index) {
    labels.push_back(choices[index].first);
    if (choices[index].second == current) {
      selected_index = static_cast<int>(index);
    }
  }

  return ChoiceCombo(labels,
                     selected_index,
                     [choices, key = std::wstring(key), default_value, on_change](int selected) {
    if (selected < 0 || selected >= static_cast<int>(choices.size())) {
      return;
    }
    const bool value = choices[static_cast<size_t>(selected)].second;
    const bool previous = ReadBoolSetting(key, default_value);
    WriteBoolSetting(key, value);
    if (on_change && previous != value) {
      on_change(value);
    }
  });
}

void RequestPinyinConfigRestart(bool) {
  RequestApplyInputConfigDeferred();
}

void RequestRimeOptionRefresh(bool) {
  RequestInputStateRefreshDeferred();
}

ComboBox InputSchemeCombo(ComboBox const& double_scheme_combo,
                          std::function<void(std::wstring_view)> on_change) {
  const auto choices = InputSchemeChoices();
  const std::wstring current =
      ReadStringSetting(fp::kInputSchemeSetting, fp::kDefaultInputScheme);
  std::vector<std::wstring> labels;
  int selected_index = 0;
  for (size_t index = 0; index < choices.size(); ++index) {
    labels.push_back(choices[index].first);
    if (choices[index].second == current) {
      selected_index = static_cast<int>(index);
    }
  }
  double_scheme_combo.IsEnabled(current == std::wstring(fp::kInputSchemeDoublePinyin));

  return ChoiceCombo(labels, selected_index, [double_scheme_combo, choices, on_change](int selected) {
    if (selected < 0 || selected >= static_cast<int>(choices.size())) {
      return;
    }
    const std::wstring value = choices[static_cast<size_t>(selected)].second;
    const std::wstring previous =
        ReadStringSetting(fp::kInputSchemeSetting, fp::kDefaultInputScheme);
    WriteStringSetting(fp::kInputSchemeSetting, value);
    double_scheme_combo.IsEnabled(value == std::wstring(fp::kInputSchemeDoublePinyin));
    if (previous != value) {
      RequestApplyInputConfigDeferred();
    }
    if (on_change) {
      on_change(value);
    }
  });
}

ComboBox InputSchemeCombo(ComboBox const& double_scheme_combo,
                          TextBlock const& icon_label,
                          std::function<void(std::wstring_view)> on_change) {
  icon_label.Text(InputSchemeIconText(
      ReadStringSetting(fp::kInputSchemeSetting, fp::kDefaultInputScheme)));
  return InputSchemeCombo(double_scheme_combo,
                          [icon_label, on_change](std::wstring_view value) {
                            icon_label.Text(InputSchemeIconText(value));
                            if (on_change) {
                              on_change(value);
                            }
                          });
}

TextBox SettingTextBox(std::wstring_view key,
                       std::wstring_view placeholder,
                       double min_width,
                       std::wstring_view default_value) {
  TextBox box;
  ApplySettingsUIFont(box);
  box.MinWidth(min_width);
  box.HorizontalAlignment(HorizontalAlignment::Right);
  box.Text(ReadStringSetting(key, default_value));
  box.PlaceholderText(placeholder);
  box.LostFocus([box, key = std::wstring(key)](auto const&, auto const&) {
    WriteStringSetting(key, box.Text().c_str());
  });
  return box;
}

TextBox SettingTextBox(std::wstring_view key,
                       std::wstring_view placeholder,
                       double min_width,
                       std::wstring_view default_value,
                       TextBlock const& icon_label) {
  icon_label.Text(FirstIconText(ReadStringSetting(key, default_value)));
  auto box = SettingTextBox(key, placeholder, min_width, default_value);
  box.TextChanged([box, icon_label](auto const&, auto const&) {
    icon_label.Text(FirstIconText(std::wstring_view(box.Text())));
  });
  return box;
}

ToggleSwitch SettingSwitch(std::wstring_view key,
                           bool default_value,
                           std::function<void(bool)> on_change) {
  ToggleSwitch toggle;
  ApplySettingsUIFont(toggle);
  toggle.MinWidth(0);
  toggle.HorizontalAlignment(HorizontalAlignment::Right);
  toggle.IsOn(ReadBoolSetting(key, default_value));
  toggle.Toggled([toggle, key = std::wstring(key), on_change](auto const&, auto const&) {
    WriteBoolSetting(key, toggle.IsOn());
    if (on_change) {
      on_change(toggle.IsOn());
    }
  });
  return toggle;
}

ToggleSwitch SettingSwitch(std::wstring_view key,
                           bool default_value,
                           TextBlock const& icon_label,
                           std::function<std::wstring(bool)> icon_text,
                           std::function<void(bool)> on_change) {
  auto resolve_icon = [icon_text](bool value) { return icon_text ? icon_text(value) : BoolIconText(value); };
  const bool current = ReadBoolSetting(key, default_value);
  icon_label.Text(resolve_icon(current));
  return SettingSwitch(key,
                       default_value,
                       [icon_label, resolve_icon, on_change](bool value) {
                         icon_label.Text(resolve_icon(value));
                         if (on_change) {
                           on_change(value);
                         }
                       });
}

UIElement WanxiangModeIcon(std::wstring_view value) {
  if (value == L"En") {
    return FluentPathIcon(kFluentIconTextGrammarCheckmark20RegularPath, 0.94);
  }
  if (value == L"Mix") {
    return FluentPathIcon(kFluentIconSparkleCircle20RegularPath, 0.94);
  }
  if (value == L"词" || value == L"``") {
    return FluentPathIcon(kFluentIconEdit20RegularPath, 0.94);
  }
  if (value == L"统") {
    return FluentPathIcon(kFluentIconDatabase20RegularPath, 0.96);
  }
  if (value == L"命") {
    return FluentPathIcon(kFluentIconKeyboard20RegularPath, 1.02);
  }
  return TextIcon(value, value.size() > 1 ? 13.0 : 15.0);
}

ToggleSwitch RimeConfigSwitch(std::wstring_view key, bool default_value) {
  return SettingSwitch(key, default_value, [](bool) {
    RequestApplyInputConfigDeferred();
  });
}

ToggleSwitch WanxiangModeSwitch(const WanxiangModeDefinition& mode) {
  ToggleSwitch toggle;
  ApplySettingsUIFont(toggle);
  toggle.MinWidth(0);
  toggle.HorizontalAlignment(HorizontalAlignment::Right);
  toggle.IsOn(mode.legacy_key.empty()
                  ? ReadBoolSetting(mode.setting_key, mode.default_value)
                  : ReadBoolSettingMigrated(mode.setting_key,
                                            mode.default_value,
                                            mode.legacy_key));
  toggle.Toggled([toggle, key = std::wstring(mode.setting_key)](auto const&, auto const&) {
    WriteBoolSetting(key, toggle.IsOn());
    RequestApplyInputConfigDeferred();
  });
  return toggle;
}

}  // namespace fp::config_winui
