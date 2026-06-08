#pragma once

#include <string>
#include <string_view>

namespace fp::config_winui {

inline constexpr std::wstring_view kSettingsPageGeneral = L"general";
inline constexpr std::wstring_view kSettingsPageAppearance = L"appearance";
inline constexpr std::wstring_view kSettingsPageLexicon = L"lexicon";
inline constexpr std::wstring_view kSettingsPageHotkeys = L"hotkeys";
inline constexpr std::wstring_view kSettingsPageSync = L"sync";
inline constexpr std::wstring_view kSettingsPageAdvanced = L"advanced";
inline constexpr std::wstring_view kSettingsPageAbout = L"about";

std::wstring ExtractSettingsPageValue(std::wstring_view command_line);
std::wstring NormalizeSettingsPageTag(std::wstring_view value);
std::wstring InitialSettingsPageTag(std::wstring_view command_line);

}  // namespace fp::config_winui
