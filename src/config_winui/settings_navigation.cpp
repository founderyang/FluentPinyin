#include "config_winui/settings_navigation.h"

namespace fp::config_winui {

std::wstring ExtractSettingsPageValue(std::wstring_view command_line) {
  constexpr std::wstring_view marker = L"--page=";
  const size_t marker_pos = command_line.find(marker);
  if (marker_pos == std::wstring_view::npos) {
    return L"";
  }

  size_t value_start = marker_pos + marker.size();
  if (value_start < command_line.size() && command_line[value_start] == L'"') {
    ++value_start;
  }
  size_t value_end = command_line.find_first_of(L" \"", value_start);
  if (value_end == std::wstring_view::npos) {
    value_end = command_line.size();
  }

  return std::wstring(command_line.substr(value_start, value_end - value_start));
}

std::wstring NormalizeSettingsPageTag(std::wstring_view value) {
  if (value.empty()) {
    return std::wstring(kSettingsPageGeneral);
  }

  if (value == L"phrases" || value == L"custom-phrases" ||
      value == L"custom_phrases" || value == L"dictionaries" ||
      value == L"lexicon-management" || value == L"lexicon_management" ||
      value == L"domain" || value == L"imports") {
    return std::wstring(kSettingsPageLexicon);
  }
  if (value == L"keys") {
    return std::wstring(kSettingsPageHotkeys);
  }
  if (value == L"input") {
    return std::wstring(kSettingsPageGeneral);
  }
  if (value == kSettingsPageGeneral || value == kSettingsPageAdvanced ||
      value == kSettingsPageAppearance || value == kSettingsPageLexicon ||
      value == kSettingsPageHotkeys || value == kSettingsPageSync ||
      value == kSettingsPageAbout) {
    return std::wstring(value);
  }
  return std::wstring(kSettingsPageGeneral);
}

std::wstring InitialSettingsPageTag(std::wstring_view command_line) {
  return NormalizeSettingsPageTag(ExtractSettingsPageValue(command_line));
}

}  // namespace fp::config_winui
