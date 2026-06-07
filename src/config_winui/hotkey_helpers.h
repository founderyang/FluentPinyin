#pragma once

#include <windows.h>

#include <string>
#include <string_view>

namespace fp::config_winui {

std::wstring NormalizeShortcutDisplay(std::wstring_view value);
std::wstring ShortcutDisplayForKey(WPARAM virtual_key);
bool IsShortcutModifierKey(WPARAM virtual_key);
bool ShortcutModifierKeyEquals(WPARAM expected, WPARAM actual);
std::wstring RecordedShortcutFromKey(WPARAM virtual_key);

}  // namespace fp::config_winui
