#pragma once

#include <windows.h>

#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace fp::config_winui {

struct HotkeyShortcutDefinition {
  std::wstring_view key;
  std::wstring_view label;
  std::wstring_view fallback;
};

std::span<const HotkeyShortcutDefinition> HotkeyShortcutDefinitions() noexcept;
std::wstring NormalizeShortcutDisplay(std::wstring_view value);
std::wstring ShortcutConflictTooltip(
    std::wstring_view current_key,
    std::wstring_view display,
    const std::function<std::wstring(std::wstring_view key,
                                     std::wstring_view fallback)>& read_setting);
std::wstring ShortcutDisplayForKey(WPARAM virtual_key);
bool IsShortcutModifierKey(WPARAM virtual_key);
bool ShortcutModifierKeyEquals(WPARAM expected, WPARAM actual);
std::wstring RecordedShortcutFromKey(WPARAM virtual_key);

}  // namespace fp::config_winui
