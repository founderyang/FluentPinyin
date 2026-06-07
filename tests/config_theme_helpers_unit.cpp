#include "config_winui/theme_helpers.h"

#include "config_winui/settings_binding.h"

#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string_view>
#include <system_error>

namespace {

int g_failures = 0;
std::filesystem::path g_isolated_appdata_root;

bool SameColor(fp::ThemeColor lhs, fp::ThemeColor rhs) {
  return lhs.red == rhs.red && lhs.green == rhs.green && lhs.blue == rhs.blue;
}

void Expect(bool condition, std::string_view message) {
  if (condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAIL: " << message << "\n";
}

void UseIsolatedAppData() {
  g_isolated_appdata_root = std::filesystem::current_path() /
                            (L"config-theme-helpers-appdata-" +
                             std::to_wstring(GetCurrentProcessId()));
  const auto local = g_isolated_appdata_root / L"Local";
  const auto roaming = g_isolated_appdata_root / L"Roaming";
  const auto program_data = g_isolated_appdata_root / L"ProgramData";
  std::error_code error;
  std::filesystem::remove_all(g_isolated_appdata_root, error);
  std::filesystem::create_directories(local, error);
  std::filesystem::create_directories(roaming, error);
  std::filesystem::create_directories(program_data, error);
  SetEnvironmentVariableW(L"LOCALAPPDATA", local.c_str());
  SetEnvironmentVariableW(L"APPDATA", roaming.c_str());
  SetEnvironmentVariableW(L"PROGRAMDATA", program_data.c_str());
}

void TestThemeModeHelpers() {
  namespace settings = fp::config_winui;

  settings::ResetSettingsCache();
  Expect(settings::CurrentThemeModeSetting() == fp::kThemeModeSystem,
         "default theme mode falls back to system");
  Expect(settings::ThemeModeDisplayText(fp::kThemeModeLight) == L"\u6D45\u8272",
         "formats light theme mode label");
  Expect(settings::ThemeModeDisplayText(fp::kThemeModeDark) == L"\u6DF1\u8272",
         "formats dark theme mode label");
  Expect(settings::ThemeModeDisplayText(fp::kThemeModeCustom) == L"\u9884\u8BBE",
         "formats custom theme mode label");
  Expect(settings::ThemeModeIndex(fp::kThemeModeSystem) == 0,
         "maps system theme mode to index");
  Expect(settings::ThemeModeIndex(fp::kThemeModeLight) == 1,
         "maps light theme mode to index");
  Expect(settings::ThemeModeIndex(fp::kThemeModeDark) == 2,
         "maps dark theme mode to index");
  Expect(settings::ThemeModeIndex(fp::kThemeModeCustom) == 3,
         "maps custom theme mode to index");
  Expect(settings::ThemeModeValueForIndex(2) == fp::kThemeModeDark,
         "maps theme mode index to value");
  Expect(settings::ThemeModeValueForIndex(99) == fp::kThemeModeSystem,
         "falls back to system for unknown theme mode index");

  settings::WriteStringSetting(fp::kThemeModeSetting, fp::kThemeModeCustom);
  settings::WriteStringSetting(fp::kThemePresetSetting, fp::kThemePresetPurple);
  settings::ResetSettingsCache();
  Expect(settings::CurrentThemeModeSetting() == fp::kThemeModeCustom,
         "reads persisted theme mode");
  Expect(settings::CurrentThemePresetSetting() == fp::kThemePresetPurple,
         "reads persisted theme preset");
  Expect(settings::EffectiveThemePreset() == fp::kThemePresetPurple,
         "custom mode uses selected preset");
}

void TestPalettes() {
  namespace settings = fp::config_winui;

  const auto preview = settings::ThemePreview(fp::kThemePresetPurple);
  const auto common = fp::ThemePaletteForPreset(fp::kThemePresetPurple);
  Expect(SameColor(preview.background, common.toolbar_background),
         "theme preview uses toolbar background");
  Expect(SameColor(preview.symbol, common.accent),
         "theme preview uses accent symbol color");

  settings::ClearSettingsPaletteOverride();
  settings::WriteStringSetting(fp::kThemeModeSetting, fp::kThemeModeCustom);
  settings::WriteStringSetting(fp::kThemePresetSetting, fp::kThemePresetBlack);
  settings::ResetSettingsCache();
  const auto palette = settings::CurrentSettingsPalette();
  const auto expected = fp::ThemePaletteForPreset(fp::kThemePresetBlack);
  Expect(palette.light == expected.light &&
             SameColor(palette.background, expected.settings_background) &&
             SameColor(palette.card, expected.settings_card),
         "settings palette is derived from active preset");

  settings::SettingsThemePalette override_palette = palette;
  override_palette.background = {1, 2, 3};
  settings::SetSettingsPaletteOverride(override_palette);
  Expect(SameColor(settings::CurrentSettingsPalette().background, {1, 2, 3}),
         "settings palette override is used");
  Expect(settings::SettingsPaletteOverride().has_value(),
         "settings palette override is observable");
  settings::ClearSettingsPaletteOverride();
  Expect(!settings::SettingsPaletteOverride().has_value(),
         "settings palette override can be cleared");
}

}  // namespace

int main() {
  UseIsolatedAppData();
  TestThemeModeHelpers();
  TestPalettes();
  std::error_code cleanup_error;
  std::filesystem::remove_all(g_isolated_appdata_root, cleanup_error);
  if (g_failures != 0) {
    std::cerr << g_failures << " config theme helper failure(s)\n";
    return 1;
  }
  std::cout << "Config theme helper tests passed\n";
  return 0;
}
