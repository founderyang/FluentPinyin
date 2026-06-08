#include "tsf/shortcut_model.h"

#include <algorithm>
#include <cwctype>
#include <utility>

namespace fp::tsf {
namespace {

std::wstring NormalizeShortcutToken(std::wstring token) {
  token = TrimShortcutDisplay(std::move(token));
  token.erase(std::remove_if(token.begin(),
                             token.end(),
                             [](wchar_t ch) { return std::iswspace(ch) != 0; }),
              token.end());
  std::transform(token.begin(), token.end(), token.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  return token;
}

}  // namespace

std::wstring TrimShortcutDisplay(std::wstring value) {
  size_t first = 0;
  while (first < value.size() && std::iswspace(value[first])) {
    ++first;
  }
  size_t last = value.size();
  while (last > first && std::iswspace(value[last - 1])) {
    --last;
  }
  return value.substr(first, last - first);
}

std::optional<WPARAM> ShortcutKeyFromNormalizedToken(const std::wstring& token) {
  if (token.empty()) {
    return std::nullopt;
  }
  if (token == L"." || token == L"period") {
    return VK_OEM_PERIOD;
  }
  if (token == L"," || token == L"comma") {
    return VK_OEM_COMMA;
  }
  if (token == L"-" || token == L"minus") {
    return VK_OEM_MINUS;
  }
  if (token == L"=" || token == L"plus") {
    return VK_OEM_PLUS;
  }
  if (token == L";" || token == L"semicolon") {
    return VK_OEM_1;
  }
  if (token == L"/" || token == L"slash") {
    return VK_OEM_2;
  }
  if (token == L"`" || token == L"grave") {
    return VK_OEM_3;
  }
  if (token == L"[" || token == L"leftbracket") {
    return VK_OEM_4;
  }
  if (token == L"\\" || token == L"backslash") {
    return VK_OEM_5;
  }
  if (token == L"]" || token == L"rightbracket") {
    return VK_OEM_6;
  }
  if (token == L"'" || token == L"quote") {
    return VK_OEM_7;
  }
  if (token == L"tab") {
    return VK_TAB;
  }
  if (token == L"pgup" || token == L"pageup") {
    return VK_PRIOR;
  }
  if (token == L"pgdn" || token == L"pagedown") {
    return VK_NEXT;
  }
  if (token == L"esc" || token == L"escape") {
    return VK_ESCAPE;
  }
  if (token == L"backspace") {
    return VK_BACK;
  }
  if (token == L"delete") {
    return VK_DELETE;
  }
  if (token == L"insert") {
    return VK_INSERT;
  }
  if (token == L"home") {
    return VK_HOME;
  }
  if (token == L"end") {
    return VK_END;
  }
  if (token == L"left") {
    return VK_LEFT;
  }
  if (token == L"right") {
    return VK_RIGHT;
  }
  if (token == L"up") {
    return VK_UP;
  }
  if (token == L"down") {
    return VK_DOWN;
  }
  if (token == L"space") {
    return VK_SPACE;
  }
  if (token == L"enter" || token == L"return") {
    return VK_RETURN;
  }
  if (token.size() >= 2 && token[0] == L'f') {
    try {
      const int function_key = std::stoi(token.substr(1));
      if (function_key >= 1 && function_key <= 12) {
        return static_cast<WPARAM>(VK_F1 + function_key - 1);
      }
    } catch (...) {
      return std::nullopt;
    }
  }
  if (token.size() == 1 && token[0] >= L'0' && token[0] <= L'9') {
    return static_cast<WPARAM>(token[0]);
  }
  if (token.size() == 1 && token[0] >= L'a' && token[0] <= L'z') {
    return static_cast<WPARAM>(std::towupper(token[0]));
  }
  return std::nullopt;
}

std::optional<WPARAM> ShortcutKeyFromToken(const std::wstring& token) {
  return ShortcutKeyFromNormalizedToken(NormalizeShortcutToken(token));
}

ShortcutChord ParseShortcutChord(std::wstring_view display) {
  ShortcutChord chord;
  std::wstring text(display);
  size_t start = 0;
  while (start <= text.size()) {
    const size_t separator = text.find(L'+', start);
    const size_t end = separator == std::wstring::npos ? text.size() : separator;
    const std::wstring token = NormalizeShortcutToken(text.substr(start, end - start));
    if (token == L"ctrl" || token == L"control") {
      chord.ctrl = true;
    } else if (token == L"shift") {
      chord.shift = true;
    } else if (token == L"alt") {
      chord.alt = true;
    } else if (token == L"win" || token == L"windows") {
      chord.win = true;
    } else if (const auto key = ShortcutKeyFromNormalizedToken(token)) {
      chord.key = *key;
    } else {
      return {};
    }
    if (separator == std::wstring::npos) {
      break;
    }
    start = separator + 1;
  }
  if (chord.key == 0) {
    const int modifier_count =
        (chord.ctrl ? 1 : 0) + (chord.shift ? 1 : 0) + (chord.alt ? 1 : 0) +
        (chord.win ? 1 : 0);
    if (modifier_count == 1) {
      if (chord.ctrl) {
        chord.key = VK_CONTROL;
      } else if (chord.shift) {
        chord.key = VK_SHIFT;
      } else if (chord.alt) {
        chord.key = VK_MENU;
      } else if (chord.win) {
        chord.key = VK_LWIN;
      }
    }
  }
  chord.valid = chord.key != 0;
  return chord;
}

bool ShortcutKeyEquals(WPARAM expected, WPARAM actual) {
  if (expected == actual) {
    return true;
  }
  if (expected == VK_SHIFT) {
    return actual == VK_LSHIFT || actual == VK_RSHIFT;
  }
  if (expected == VK_CONTROL) {
    return actual == VK_LCONTROL || actual == VK_RCONTROL;
  }
  if (expected == VK_MENU) {
    return actual == VK_LMENU || actual == VK_RMENU;
  }
  if (expected == VK_LWIN || expected == VK_RWIN) {
    return actual == VK_LWIN || actual == VK_RWIN;
  }
  return false;
}

bool IsShortcutModifierVirtualKey(WPARAM wparam) {
  return wparam == VK_SHIFT || wparam == VK_LSHIFT || wparam == VK_RSHIFT ||
         wparam == VK_CONTROL || wparam == VK_LCONTROL || wparam == VK_RCONTROL ||
         wparam == VK_MENU || wparam == VK_LMENU || wparam == VK_RMENU ||
         wparam == VK_LWIN || wparam == VK_RWIN;
}

}  // namespace fp::tsf
