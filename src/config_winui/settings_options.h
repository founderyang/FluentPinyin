#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fp::config_winui {

using StringSettingChoice = std::pair<std::wstring, std::wstring>;
using BoolSettingChoice = std::pair<std::wstring, bool>;

const std::vector<StringSettingChoice>& InputSchemeChoices();
const std::vector<StringSettingChoice>& DoublePinyinSchemeChoices();
const std::vector<StringSettingChoice>& DefaultInputModeChoices();
const std::vector<StringSettingChoice>& DefaultCharsetChoices();
const std::vector<StringSettingChoice>& CandidateLayoutChoices();
const std::vector<StringSettingChoice>& SyncAutoIntervalChoices();

const std::vector<BoolSettingChoice>& DefaultShapeChoices();
const std::vector<BoolSettingChoice>& DefaultPunctuationChoices();

std::wstring InputSchemeIconText(std::wstring_view value);
std::wstring DefaultInputModeChoiceIconText(std::wstring_view value);
std::wstring DefaultCharsetChoiceIconText(std::wstring_view value);
std::wstring DefaultInputModeIconText();
std::wstring FirstIconText(std::wstring_view value);
std::wstring BoolIconText(bool value);
std::wstring ChoiceIconText(const std::vector<StringSettingChoice>& choices,
                            std::wstring_view value);
std::wstring IntChoiceIconText(const std::vector<std::wstring>& labels, int value);
std::wstring DefaultCharsetIconText();
std::wstring CandidateFontIconText(std::wstring_view value);
int SyncAutoIntervalMinutesFromValue(std::wstring_view value);

}  // namespace fp::config_winui
