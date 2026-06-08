#include "config_winui/hotkey_helpers.h"

#include "common/constants.h"
#include "common/encoding.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <utility>
#include <vector>

namespace fp::config_winui {

std::span<const HotkeyShortcutDefinition> HotkeyShortcutDefinitions() noexcept {
  static constexpr std::array<HotkeyShortcutDefinition, 8> shortcuts{{
      {fp::kShortcutToolbarInputModeSetting,
       L"中/英文模式",
       fp::kDefaultShortcutToolbarInputMode},
      {fp::kShortcutToolbarShapeSetting, L"全/半角", fp::kDefaultShortcutToolbarShape},
      {fp::kShortcutToolbarPunctuationSetting,
       L"中/英文标点",
       fp::kDefaultShortcutToolbarPunctuation},
      {fp::kShortcutToolbarCharsetSetting,
       L"简体/繁体",
       fp::kDefaultShortcutToolbarCharset},
      {fp::kShortcutToolbarEmojiSetting,
       L"表情符号/符号",
       fp::kDefaultShortcutToolbarEmoji},
      {fp::kShortcutCandidateExpandSetting,
       L"展开/收起候选框",
       fp::kDefaultShortcutCandidateExpand},
      {fp::kShortcutCandidatePreviousPageSetting,
       L"上一页",
       fp::kDefaultShortcutCandidatePreviousPage},
      {fp::kShortcutCandidateNextPageSetting,
       L"下一页",
       fp::kDefaultShortcutCandidateNextPage},
  }};
  return shortcuts;
}

std::wstring NormalizeShortcutDisplay(std::wstring_view value) {
  std::wstring text = fp::TrimWhitespace(value);
  text.erase(std::remove_if(text.begin(), text.end(), [](wchar_t ch) {
               return std::iswspace(ch) != 0;
             }),
             text.end());
  if (text.empty()) {
    return {};
  }

  std::vector<std::wstring> tokens;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t separator = text.find(L'+', start);
    const size_t end = separator == std::wstring::npos ? text.size() : separator;
    std::wstring token = text.substr(start, end - start);
    std::transform(token.begin(), token.end(), token.begin(), [](wchar_t ch) {
      return static_cast<wchar_t>(std::towlower(ch));
    });

    if (token == L"control") {
      token = L"Ctrl";
    } else if (token == L"ctrl") {
      token = L"Ctrl";
    } else if (token == L"shift") {
      token = L"Shift";
    } else if (token == L"alt") {
      token = L"Alt";
    } else if (token == L"win" || token == L"windows") {
      token = L"Win";
    } else if (token == L"tab") {
      token = L"Tab";
    } else if (token == L"pgup" || token == L"pageup") {
      token = L"PgUp";
    } else if (token == L"pgdn" || token == L"pagedown") {
      token = L"PgDn";
    } else if (token == L"esc" || token == L"escape") {
      token = L"Esc";
    } else if (token == L"period") {
      token = L".";
    } else if (token == L"comma") {
      token = L",";
    } else if (token.size() == 1 && token[0] >= L'a' && token[0] <= L'z') {
      token[0] = static_cast<wchar_t>(std::towupper(token[0]));
    }

    if (token.empty()) {
      return {};
    }
    tokens.push_back(std::move(token));
    if (separator == std::wstring::npos) {
      break;
    }
    start = separator + 1;
  }

  std::wstring normalized;
  for (size_t index = 0; index < tokens.size(); ++index) {
    if (index != 0) {
      normalized += L"+";
    }
    normalized += tokens[index];
  }
  return normalized;
}

std::wstring ShortcutConflictTooltip(
    std::wstring_view current_key,
    std::wstring_view display,
    const std::function<std::wstring(std::wstring_view key,
                                     std::wstring_view fallback)>& read_setting) {
  const std::wstring normalized = NormalizeShortcutDisplay(display);
  if (normalized.empty()) {
    return L"已清除";
  }

  for (const auto& entry : HotkeyShortcutDefinitions()) {
    if (entry.key == current_key) {
      continue;
    }
    if (NormalizeShortcutDisplay(read_setting(entry.key, entry.fallback)) == normalized) {
      return L"与 " + std::wstring(entry.label) + L" 使用相同热键";
    }
  }
  return L"点击后按新的组合键";
}

std::wstring ShortcutDisplayForKey(WPARAM virtual_key) {
  if (virtual_key >= 'A' && virtual_key <= 'Z') {
    return std::wstring(1, static_cast<wchar_t>(virtual_key));
  }
  if (virtual_key >= '0' && virtual_key <= '9') {
    return std::wstring(1, static_cast<wchar_t>(virtual_key));
  }
  if (virtual_key >= VK_F1 && virtual_key <= VK_F12) {
    return L"F" + std::to_wstring(virtual_key - VK_F1 + 1);
  }

  switch (virtual_key) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
      return L"Shift";
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
      return L"Ctrl";
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
      return L"Alt";
    case VK_LWIN:
    case VK_RWIN:
      return L"Win";
    case VK_TAB:
      return L"Tab";
    case VK_PRIOR:
      return L"PgUp";
    case VK_NEXT:
      return L"PgDn";
    case VK_ESCAPE:
      return L"Esc";
    case VK_BACK:
      return L"Backspace";
    case VK_DELETE:
      return L"Delete";
    case VK_INSERT:
      return L"Insert";
    case VK_HOME:
      return L"Home";
    case VK_END:
      return L"End";
    case VK_LEFT:
      return L"Left";
    case VK_RIGHT:
      return L"Right";
    case VK_UP:
      return L"Up";
    case VK_DOWN:
      return L"Down";
    case VK_SPACE:
      return L"Space";
    case VK_RETURN:
      return L"Enter";
    case VK_OEM_PERIOD:
      return L".";
    case VK_OEM_COMMA:
      return L",";
    case VK_OEM_MINUS:
      return L"-";
    case VK_OEM_PLUS:
      return L"=";
    case VK_OEM_1:
      return L";";
    case VK_OEM_2:
      return L"/";
    case VK_OEM_3:
      return L"`";
    case VK_OEM_4:
      return L"[";
    case VK_OEM_5:
      return L"\\";
    case VK_OEM_6:
      return L"]";
    case VK_OEM_7:
      return L"'";
    default:
      break;
  }
  return {};
}

bool IsShortcutModifierKey(WPARAM virtual_key) {
  return virtual_key == VK_SHIFT || virtual_key == VK_LSHIFT || virtual_key == VK_RSHIFT ||
         virtual_key == VK_CONTROL || virtual_key == VK_LCONTROL || virtual_key == VK_RCONTROL ||
         virtual_key == VK_MENU || virtual_key == VK_LMENU || virtual_key == VK_RMENU ||
         virtual_key == VK_LWIN || virtual_key == VK_RWIN;
}

bool ShortcutModifierKeyEquals(WPARAM expected, WPARAM actual) {
  if (expected == actual) {
    return true;
  }
  if (expected == VK_SHIFT || expected == VK_LSHIFT || expected == VK_RSHIFT) {
    return actual == VK_SHIFT || actual == VK_LSHIFT || actual == VK_RSHIFT;
  }
  if (expected == VK_CONTROL || expected == VK_LCONTROL || expected == VK_RCONTROL) {
    return actual == VK_CONTROL || actual == VK_LCONTROL || actual == VK_RCONTROL;
  }
  if (expected == VK_MENU || expected == VK_LMENU || expected == VK_RMENU) {
    return actual == VK_MENU || actual == VK_LMENU || actual == VK_RMENU;
  }
  if (expected == VK_LWIN || expected == VK_RWIN) {
    return actual == VK_LWIN || actual == VK_RWIN;
  }
  return false;
}

std::wstring RecordedShortcutFromKey(WPARAM virtual_key) {
  const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
  const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
  const bool win = (GetKeyState(VK_LWIN) & 0x8000) != 0 ||
                   (GetKeyState(VK_RWIN) & 0x8000) != 0;

  if (IsShortcutModifierKey(virtual_key)) {
    const int modifier_count = (ctrl ? 1 : 0) + (shift ? 1 : 0) + (alt ? 1 : 0) + (win ? 1 : 0);
    if (modifier_count != 1) {
      return {};
    }
  }

  std::wstring key = ShortcutDisplayForKey(virtual_key);
  if (key.empty()) {
    return {};
  }

  std::vector<std::wstring> parts;
  if (ctrl && key != L"Ctrl") {
    parts.push_back(L"Ctrl");
  }
  if (shift && key != L"Shift") {
    parts.push_back(L"Shift");
  }
  if (alt && key != L"Alt") {
    parts.push_back(L"Alt");
  }
  if (win && key != L"Win") {
    parts.push_back(L"Win");
  }
  parts.push_back(std::move(key));

  std::wstring display;
  for (size_t index = 0; index < parts.size(); ++index) {
    if (index != 0) {
      display += L"+";
    }
    display += parts[index];
  }
  return NormalizeShortcutDisplay(display);
}

}  // namespace fp::config_winui
