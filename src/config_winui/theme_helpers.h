#pragma once

#include "common/theme.h"

#include <optional>
#include <string>
#include <string_view>

namespace fp::config_winui {

struct SettingsThemePalette {
  bool light;
  fp::ThemeColor background;
  fp::ThemeColor card;
  fp::ThemeColor edge;
  fp::ThemeColor border;
  fp::ThemeColor icon_backdrop;
  fp::ThemeColor text;
  fp::ThemeColor secondary_text;
  fp::ThemeColor muted_text;
  fp::ThemeColor icon;
  fp::ThemeColor button;
  fp::ThemeColor button_hover;
  fp::ThemeColor button_pressed;
  fp::ThemeColor accent;
};

struct ThemePreviewPalette {
  fp::ThemeColor background;
  fp::ThemeColor tile;
  fp::ThemeColor edge;
  fp::ThemeColor circle;
  fp::ThemeColor text;
  fp::ThemeColor symbol;
};

bool SystemAppsUseLightTheme();
std::wstring CurrentThemeModeSetting();
std::wstring CurrentThemePresetSetting();
std::wstring EffectiveThemePreset();
std::wstring DefaultPresetForThemeMode(std::wstring_view mode);
std::wstring ThemeModeDisplayText(std::wstring_view mode);
int ThemeModeIndex(std::wstring_view mode);
std::wstring ThemeModeValueForIndex(int index);
ThemePreviewPalette ThemePreview(std::wstring_view preset);
SettingsThemePalette CurrentSettingsPalette();
void SetSettingsPaletteOverride(SettingsThemePalette palette);
void ClearSettingsPaletteOverride();
std::optional<SettingsThemePalette> SettingsPaletteOverride();

}  // namespace fp::config_winui
