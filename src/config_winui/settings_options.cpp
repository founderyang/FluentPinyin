#include "config_winui/settings_options.h"

#include "common/constants.h"

namespace fp::config_winui {

const std::vector<StringSettingChoice>& InputSchemeChoices() {
  static const std::vector<StringSettingChoice> choices{
      {L"全拼", std::wstring(fp::kInputSchemePinyin)},
      {L"双拼", std::wstring(fp::kInputSchemeDoublePinyin)},
  };
  return choices;
}

const std::vector<StringSettingChoice>& DoublePinyinSchemeChoices() {
  static const std::vector<StringSettingChoice> choices{
      {L"自然码", L"zrm"},
      {L"小鹤双拼", L"flypy"},
      {L"微软双拼", L"mspy"},
      {L"搜狗双拼", L"sogou"},
      {L"智能 ABC", L"abc"},
      {L"紫光双拼", L"ziguang"},
      {L"拼音加加", L"pyjj"},
      {L"国标双拼", L"gbpy"},
      {L"乱序 17", L"lxsq"},
  };
  return choices;
}

const std::vector<StringSettingChoice>& DefaultInputModeChoices() {
  static const std::vector<StringSettingChoice> choices{
      {L"中文", std::wstring(fp::kInputModeChinese)},
      {L"英文", std::wstring(fp::kInputModeEnglish)},
  };
  return choices;
}

const std::vector<StringSettingChoice>& DefaultCharsetChoices() {
  static const std::vector<StringSettingChoice> choices{
      {L"简体", std::wstring(fp::kCharsetSimplified)},
      {L"繁体", std::wstring(fp::kCharsetTraditional)},
  };
  return choices;
}

const std::vector<StringSettingChoice>& CandidateLayoutChoices() {
  static const std::vector<StringSettingChoice> choices{
      {L"横排", std::wstring(fp::kCandidateLayoutHorizontal)},
      {L"竖排", std::wstring(fp::kCandidateLayoutVertical)},
  };
  return choices;
}

const std::vector<StringSettingChoice>& SyncAutoIntervalChoices() {
  static const std::vector<StringSettingChoice> choices{
      {L"每 15 分钟", L"15"},
      {L"每 30 分钟", L"30"},
      {L"每 1 小时", L"60"},
      {L"每 6 小时", L"360"},
  };
  return choices;
}

const std::vector<BoolSettingChoice>& DefaultShapeChoices() {
  static const std::vector<BoolSettingChoice> choices{
      {L"半角", true},
      {L"全角", false},
  };
  return choices;
}

const std::vector<BoolSettingChoice>& DefaultPunctuationChoices() {
  static const std::vector<BoolSettingChoice> choices{
      {L"中文标点", true},
      {L"英文标点", false},
  };
  return choices;
}

std::wstring InputSchemeIconText(std::wstring_view value) {
  return value == fp::kInputSchemeDoublePinyin ? L"双" : L"全";
}

std::wstring DefaultInputModeChoiceIconText(std::wstring_view value) {
  return value == fp::kInputModeEnglish ? L"英" : L"中";
}

std::wstring DefaultCharsetChoiceIconText(std::wstring_view value) {
  return value == fp::kCharsetTraditional ? L"繁" : L"简";
}

int SyncAutoIntervalMinutesFromValue(std::wstring_view value) {
  try {
    return std::stoi(std::wstring(value));
  } catch (...) {
    return fp::kDefaultSyncAutoIntervalMinutes;
  }
}

}  // namespace fp::config_winui
