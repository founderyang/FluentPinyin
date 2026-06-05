#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace fp {

inline constexpr std::wstring_view kThemeModeSetting = L"theme_mode";
inline constexpr std::wstring_view kThemePresetSetting = L"theme_preset";
inline constexpr std::wstring_view kLegacyThemeSetting = L"theme";

inline constexpr std::wstring_view kThemeModeSystem = L"system";
inline constexpr std::wstring_view kThemeModeLight = L"light";
inline constexpr std::wstring_view kThemeModeDark = L"dark";
inline constexpr std::wstring_view kThemeModeCustom = L"custom";

inline constexpr std::wstring_view kThemePresetDefaultLight = L"default_light";
inline constexpr std::wstring_view kThemePresetDefaultDark = L"default_dark";
inline constexpr std::wstring_view kThemePresetPurple = L"purple";
inline constexpr std::wstring_view kThemePresetBlack = L"black";
inline constexpr std::wstring_view kThemePresetWhite = L"white";
inline constexpr std::wstring_view kThemePresetBrick = L"brick";
inline constexpr std::wstring_view kThemePresetBlueGray = L"blue_gray";
inline constexpr std::wstring_view kThemePresetGray = L"gray";

struct ThemePresetDefinition {
  std::wstring_view id;
  std::wstring_view label;
  std::wstring_view mode_hint;
};

struct ThemeColor {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct ThemePalette {
  bool light;
  ThemeColor window_background;
  ThemeColor settings_background;
  ThemeColor settings_card;
  ThemeColor toolbar_background;
  ThemeColor toolbar_drag;
  ThemeColor toolbar_edge;
  ThemeColor edge;
  ThemeColor border;
  ThemeColor highlight;
  ThemeColor accent;
  ThemeColor text;
  ThemeColor secondary_text;
  ThemeColor muted_text;
  ThemeColor disabled_text;
  ThemeColor separator;
  ThemeColor button;
  ThemeColor button_hover;
  ThemeColor button_pressed;
  ThemeColor icon_backdrop;
};

inline constexpr ThemeColor kMicrosoftPinyinLightEdge{202, 202, 202};
inline constexpr ThemeColor kMicrosoftPinyinLightSeparator{234, 234, 234};
inline constexpr ThemeColor kMicrosoftPinyinDefaultDarkEdge{202, 202, 202};
inline constexpr ThemeColor kMicrosoftPinyinDefaultDarkSeparator{63, 63, 63};
inline constexpr ThemeColor kMicrosoftPinyinBlackEdge{63, 63, 63};
inline constexpr ThemeColor kMicrosoftPinyinPurpleEdge{133, 134, 180};
inline constexpr ThemeColor kMicrosoftPinyinBrickEdge{182, 109, 94};
inline constexpr ThemeColor kMicrosoftPinyinBlueGrayEdge{150, 161, 168};
inline constexpr ThemeColor kMicrosoftPinyinGrayEdge{139, 141, 141};

inline constexpr std::array<ThemePresetDefinition, 8> kThemePresetDefinitions{{
    {kThemePresetDefaultLight, L"\u6D45\u8272", kThemeModeLight},
    {kThemePresetDefaultDark, L"\u6DF1\u8272", kThemeModeDark},
    {kThemePresetWhite, L"\u767D\u8272", kThemeModeLight},
    {kThemePresetBlack, L"\u9ED1\u8272", kThemeModeDark},
    {kThemePresetPurple, L"\u7D2B\u8272", kThemeModeDark},
    {kThemePresetBrick, L"\u7816\u7EA2", kThemeModeDark},
    {kThemePresetBlueGray, L"\u84DD\u7070", kThemeModeDark},
    {kThemePresetGray, L"\u7070\u8272", kThemeModeDark},
}};

inline bool IsThemeMode(std::wstring_view value) {
  return value == kThemeModeSystem || value == kThemeModeLight ||
         value == kThemeModeDark || value == kThemeModeCustom;
}

inline bool IsThemePreset(std::wstring_view value) {
  for (const auto& preset : kThemePresetDefinitions) {
    if (preset.id == value) {
      return true;
    }
  }
  return false;
}

inline std::wstring NormalizeThemeModeSetting(std::wstring_view value) {
  if (IsThemeMode(value)) {
    return std::wstring(value);
  }
  if (value == L"light") {
    return std::wstring(kThemeModeLight);
  }
  if (value == L"system") {
    return std::wstring(kThemeModeSystem);
  }
  return std::wstring(kThemeModeDark);
}

inline std::wstring NormalizeThemePresetSetting(std::wstring_view value) {
  if (IsThemePreset(value)) {
    return std::wstring(value);
  }
  if (value == L"light") {
    return std::wstring(kThemePresetDefaultLight);
  }
  if (value == L"purple") {
    return std::wstring(kThemePresetPurple);
  }
  if (value == L"red") {
    return std::wstring(kThemePresetBrick);
  }
  if (value == L"blue") {
    return std::wstring(kThemePresetBlueGray);
  }
  if (value == L"gray") {
    return std::wstring(kThemePresetGray);
  }
  return std::wstring(kThemePresetDefaultDark);
}

inline std::wstring DefaultPresetForThemeMode(std::wstring_view mode, bool apps_use_light) {
  if (mode == kThemeModeLight) {
    return std::wstring(kThemePresetDefaultLight);
  }
  if (mode == kThemeModeSystem) {
    return apps_use_light ? std::wstring(kThemePresetDefaultLight)
                          : std::wstring(kThemePresetDefaultDark);
  }
  return std::wstring(kThemePresetDefaultDark);
}

inline std::wstring EffectiveThemePreset(std::wstring_view mode,
                                         std::wstring_view preset,
                                         bool apps_use_light) {
  if (mode == kThemeModeLight) {
    return std::wstring(kThemePresetDefaultLight);
  }
  if (mode == kThemeModeCustom) {
    return NormalizeThemePresetSetting(preset);
  }
  if (mode == kThemeModeSystem) {
    return apps_use_light ? std::wstring(kThemePresetDefaultLight)
                          : std::wstring(kThemePresetDefaultDark);
  }
  return std::wstring(kThemePresetDefaultDark);
}

inline std::wstring ThemePresetLabelText(std::wstring_view preset) {
  for (const auto& item : kThemePresetDefinitions) {
    if (item.id == preset) {
      return std::wstring(item.label);
    }
  }
  return L"\u6DF1\u8272";
}

inline ThemePalette ThemePaletteForPreset(std::wstring_view preset) {
  if (preset == kThemePresetDefaultLight) {
    return {true,
            {249, 249, 249},
            {245, 245, 245},
            {249, 249, 249},
            {245, 245, 245},
            {241, 241, 241},
            kMicrosoftPinyinLightEdge,
            kMicrosoftPinyinLightEdge,
            kMicrosoftPinyinLightEdge,
            {240, 240, 240},
            {71, 74, 178},
            {24, 24, 24},
            {72, 72, 72},
            {112, 112, 112},
            {152, 152, 152},
            kMicrosoftPinyinLightSeparator,
            {240, 240, 240},
            {234, 234, 234},
            {224, 224, 224},
            {240, 240, 240}};
  }
  if (preset == kThemePresetWhite) {
    return {true,
            {255, 255, 255},
            {255, 255, 255},
            {255, 255, 255},
            {246, 246, 246},
            {255, 255, 255},
            kMicrosoftPinyinLightEdge,
            kMicrosoftPinyinLightEdge,
            kMicrosoftPinyinLightEdge,
            {238, 238, 238},
            {71, 74, 178},
            {0, 0, 0},
            {32, 32, 32},
            {96, 96, 96},
            {128, 128, 128},
            kMicrosoftPinyinLightSeparator,
            {238, 238, 238},
            {231, 231, 231},
            {217, 217, 217},
            {238, 238, 238}};
  }
  if (preset == kThemePresetBlack) {
    return {false,
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0},
            {9, 9, 9},
            {0, 0, 0},
            kMicrosoftPinyinBlackEdge,
            kMicrosoftPinyinBlackEdge,
            kMicrosoftPinyinBlackEdge,
            {17, 17, 17},
            {189, 187, 230},
            {255, 255, 255},
            {230, 230, 230},
            {160, 160, 160},
            {128, 128, 128},
            {21, 21, 21},
            {17, 17, 17},
            {24, 24, 24},
            {38, 38, 38},
            {17, 17, 17}};
  }
  if (preset == kThemePresetPurple) {
    return {false,
            {93, 94, 155},
            {93, 94, 155},
            {99, 100, 159},
            {100, 101, 159},
            {93, 94, 155},
            kMicrosoftPinyinPurpleEdge,
            kMicrosoftPinyinPurpleEdge,
            {107, 108, 164},
            {103, 104, 162},
            {189, 187, 230},
            {255, 255, 255},
            {232, 232, 240},
            {181, 181, 209},
            {181, 181, 209},
            {107, 108, 164},
            {103, 104, 162},
            {106, 107, 163},
            {116, 117, 171},
            {103, 104, 162}};
  }
  if (preset == kThemePresetBrick) {
    return {false,
            {158, 61, 41},
            {158, 61, 41},
            {161, 68, 49},
            {161, 68, 49},
            {158, 61, 41},
            kMicrosoftPinyinBrickEdge,
            kMicrosoftPinyinBrickEdge,
            {166, 77, 59},
            {164, 72, 54},
            {235, 150, 112},
            {255, 255, 255},
            {242, 228, 226},
            {203, 151, 141},
            {203, 151, 141},
            {166, 77, 59},
            {164, 72, 54},
            {166, 77, 59},
            {178, 101, 85},
            {164, 72, 54}};
  }
  if (preset == kThemePresetBlueGray) {
    return {false,
            {116, 130, 139},
            {116, 130, 139},
            {121, 134, 143},
            {121, 134, 143},
            {116, 130, 139},
            kMicrosoftPinyinBlueGrayEdge,
            kMicrosoftPinyinBlueGrayEdge,
            {127, 140, 149},
            {124, 137, 146},
            {158, 199, 214},
            {255, 255, 255},
            {236, 238, 239},
            {181, 188, 193},
            {181, 188, 193},
            {127, 140, 149},
            {124, 137, 146},
            {127, 140, 149},
            {145, 156, 163},
            {124, 137, 146}};
  }
  if (preset == kThemePresetGray) {
    return {false,
            {101, 103, 104},
            {101, 103, 104},
            {106, 108, 109},
            {106, 108, 109},
            {101, 103, 104},
            kMicrosoftPinyinGrayEdge,
            kMicrosoftPinyinGrayEdge,
            {114, 116, 116},
            {110, 112, 113},
            {194, 197, 198},
            {255, 255, 255},
            {234, 234, 234},
            {173, 174, 174},
            {173, 174, 174},
            {114, 116, 116},
            {110, 112, 113},
            {114, 116, 116},
            {133, 135, 135},
            {110, 112, 113}};
  }
  return {false,
          {44, 44, 44},
          {32, 32, 32},
          {43, 43, 43},
          {55, 55, 55},
          {48, 48, 48},
          kMicrosoftPinyinDefaultDarkEdge,
          kMicrosoftPinyinDefaultDarkEdge,
          {58, 58, 58},
          {56, 56, 56},
          {189, 187, 230},
          {242, 242, 242},
          {204, 204, 204},
          {150, 150, 150},
          {135, 135, 135},
          kMicrosoftPinyinDefaultDarkSeparator,
          {54, 54, 54},
          {62, 62, 62},
          {70, 70, 70},
          {54, 54, 54}};
}

}  // namespace fp
