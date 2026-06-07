#include "config_winui/theme_helpers.h"

#include "config_winui/settings_binding.h"

#include <windows.h>

namespace fp::config_winui {
namespace {

std::optional<SettingsThemePalette> g_settings_palette_override;

}  // namespace

bool SystemAppsUseLightTheme() {
  DWORD value = 1;
  DWORD size = sizeof(value);
  const LSTATUS status =
      RegGetValueW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"AppsUseLightTheme",
                   RRF_RT_REG_DWORD,
                   nullptr,
                   &value,
                   &size);
  return status != ERROR_SUCCESS || value != 0;
}

std::wstring CurrentThemeModeSetting() {
  const std::wstring value = ReadStringSetting(fp::kThemeModeSetting);
  if (fp::IsThemeMode(value)) {
    return value;
  }
  return fp::NormalizeThemeModeSetting(
      ReadStringSetting(fp::kLegacyThemeSetting, fp::kThemeModeSystem));
}

std::wstring CurrentThemePresetSetting() {
  const std::wstring value = ReadStringSetting(fp::kThemePresetSetting);
  if (fp::IsThemePreset(value)) {
    return value;
  }
  return fp::NormalizeThemePresetSetting(
      ReadStringSetting(fp::kLegacyThemeSetting, fp::kThemePresetDefaultDark));
}

std::wstring EffectiveThemePreset() {
  return fp::EffectiveThemePreset(
      CurrentThemeModeSetting(), CurrentThemePresetSetting(), SystemAppsUseLightTheme());
}

std::wstring DefaultPresetForThemeMode(std::wstring_view mode) {
  return fp::DefaultPresetForThemeMode(mode, SystemAppsUseLightTheme());
}

std::wstring ThemeModeDisplayText(std::wstring_view mode) {
  if (mode == fp::kThemeModeLight) {
    return L"\u6D45\u8272";
  }
  if (mode == fp::kThemeModeDark) {
    return L"\u6DF1\u8272";
  }
  if (mode == fp::kThemeModeCustom) {
    return L"\u9884\u8BBE";
  }
  return L"\u8DDF\u968F\u7CFB\u7EDF";
}

int ThemeModeIndex(std::wstring_view mode) {
  if (mode == fp::kThemeModeLight) {
    return 1;
  }
  if (mode == fp::kThemeModeDark) {
    return 2;
  }
  if (mode == fp::kThemeModeCustom) {
    return 3;
  }
  return 0;
}

std::wstring ThemeModeValueForIndex(int index) {
  if (index == 1) {
    return std::wstring(fp::kThemeModeLight);
  }
  if (index == 2) {
    return std::wstring(fp::kThemeModeDark);
  }
  if (index == 3) {
    return std::wstring(fp::kThemeModeCustom);
  }
  return std::wstring(fp::kThemeModeSystem);
}

ThemePreviewPalette ThemePreview(std::wstring_view preset) {
  const auto palette = fp::ThemePaletteForPreset(preset);
  return {palette.toolbar_background,
          palette.toolbar_drag,
          palette.border,
          palette.highlight,
          palette.text,
          palette.accent};
}

SettingsThemePalette CurrentSettingsPalette() {
  if (g_settings_palette_override) {
    return *g_settings_palette_override;
  }
  const auto palette = fp::ThemePaletteForPreset(EffectiveThemePreset());
  return {palette.light,
          palette.settings_background,
          palette.settings_card,
          palette.border,
          palette.border,
          palette.icon_backdrop,
          palette.text,
          palette.secondary_text,
          palette.muted_text,
          palette.text,
          palette.button,
          palette.button_hover,
          palette.button_pressed,
          palette.accent};
}

void SetSettingsPaletteOverride(SettingsThemePalette palette) {
  g_settings_palette_override = palette;
}

void ClearSettingsPaletteOverride() {
  g_settings_palette_override.reset();
}

std::optional<SettingsThemePalette> SettingsPaletteOverride() {
  return g_settings_palette_override;
}

}  // namespace fp::config_winui
