#include "config_winui/default_settings.h"

#include "common/candidate_font.h"
#include "common/constants.h"
#include "common/theme.h"
#include "config_winui/hotkey_helpers.h"
#include "config_winui/settings_binding.h"
#include "config_winui/theme_helpers.h"
#include "config_winui/wanxiang_modes.h"

#include <filesystem>
#include <system_error>

namespace fp::config_winui {

void ResetDefaultSettings() {
  std::error_code error;
  std::filesystem::remove(SettingsPath(), error);
  ResetSettingsCache();

  WriteIntSetting(fp::kCandidateCountSetting, fp::kDefaultCandidateCount);
  WriteIntSetting(fp::kCandidateFontSizeLevelSetting, fp::kDefaultCandidateFontSizeLevel);
  WriteStringSetting(fp::kCandidateFontFamilySetting, fp::kDefaultCandidateFontFamily);
  WriteBoolSetting(fp::kCandidateHorizontalSetting, true);
  WriteStringSetting(fp::kCandidateLayoutSetting, fp::kDefaultCandidateLayout);
  WriteBoolSetting(fp::kStatusTipEnabledSetting, true);
  WriteStringSetting(fp::kStatusTipBlacklistSetting, fp::kDefaultStatusTipBlacklist);
  WriteBoolSetting(fp::kToolbarVisibleSetting, false);
  WriteBoolSetting(fp::kLegacyToolbarVisibleSetting, false);
  WriteStringSetting(fp::kLegacyThemeSetting, fp::kThemeModeSystem);
  WriteStringSetting(fp::kThemeModeSetting, fp::kThemeModeSystem);
  WriteStringSetting(fp::kThemePresetSetting, DefaultPresetForThemeMode(fp::kThemeModeSystem));
  WriteStringSetting(fp::kInputSchemeSetting, fp::kDefaultInputScheme);
  WriteStringSetting(fp::kDoublePinyinSchemeSetting, fp::kDefaultDoublePinyinScheme);
  WriteStringSetting(fp::kInputProfileSetting, fp::kInputProfileBase);
  WriteStringSetting(fp::kDefaultInputModeSetting, fp::kDefaultInputMode);
  WriteStringSetting(fp::kDefaultCharsetSetting, fp::kDefaultCharset);
  WriteBoolSetting(fp::kDefaultShapeHalfSetting, true);
  WriteBoolSetting(fp::kDefaultChinesePunctuationSetting, true);
  WriteBoolSetting(fp::kAutoPinyinCorrectionSetting, true);
  WriteBoolSetting(fp::kSuperAbbrevSetting, true);
  WriteBoolSetting(fp::kSmartFuzzyPinyinSetting, true);
  WriteBoolSetting(fp::kFuzzyPinyinSetting, false);
  WriteStringSetting(fp::kFuzzyPinyinRulesSetting, fp::kDefaultFuzzyPinyinRules);
  WriteStringSetting(fp::kFuzzyPinyinCustomRulesSetting, L"");
  WriteBoolSetting(fp::kUserLexiconEnabledSetting, true);
  WriteBoolSetting(fp::kCustomPhrasesEnabledSetting, true);
  WriteBoolSetting(fp::kImportedLexiconsEnabledSetting, true);
  WriteBoolSetting(fp::kNameInputSetting, true);
  WriteBoolSetting(fp::kLegacyUModeSetting, true);
  WriteBoolSetting(fp::kLegacyVModeSetting, true);
  for (const auto& mode : WanxiangModeDefinitions()) {
    WriteBoolSetting(mode.setting_key, mode.default_value);
  }

  for (const auto& shortcut : HotkeyShortcutDefinitions()) {
    WriteStringSetting(shortcut.key, shortcut.fallback);
  }
  WriteBoolSetting(fp::kSyncClipboardSetting, false);
  WriteBoolSetting(fp::kSyncUserDataSetting, false);
  WriteStringSetting(fp::kSyncProviderSetting, fp::kSyncProviderObject);
  WriteBoolSetting(fp::kSyncAutoEnabledSetting, false);
  WriteIntSetting(fp::kSyncAutoIntervalMinutesSetting, fp::kDefaultSyncAutoIntervalMinutes);
  WriteStringSetting(fp::kSyncObjectRegionSetting, fp::kSyncObjectRegionAuto);
  WriteStringSetting(fp::kSyncObjectKeySetting, fp::kDefaultSyncObjectKey);
}

}  // namespace fp::config_winui
