#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

namespace fp::tsf {

struct ShortcutChord {
  bool valid = false;
  bool ctrl = false;
  bool shift = false;
  bool alt = false;
  bool win = false;
  WPARAM key = 0;
};

std::wstring TrimShortcutDisplay(std::wstring value);
std::optional<WPARAM> ShortcutKeyFromToken(const std::wstring& token);
ShortcutChord ParseShortcutChord(std::wstring_view display);
bool ShortcutKeyEquals(WPARAM expected, WPARAM actual);
bool IsShortcutModifierVirtualKey(WPARAM wparam);

}  // namespace fp::tsf
