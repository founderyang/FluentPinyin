#include "config_winui/hotkey_helpers.h"

#include "common/constants.h"

#include <windows.h>

#include <iostream>
#include <string>
#include <string_view>

namespace {

int g_failures = 0;

void Expect(bool condition, std::string_view message) {
  if (condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAIL: " << message << "\n";
}

void TestNormalizeShortcutDisplay() {
  namespace hotkey = fp::config_winui;

  Expect(hotkey::NormalizeShortcutDisplay(L" ctrl + shift + f ") == L"Ctrl+Shift+F",
         "normalizes modifier names, spaces, and letters");
  Expect(hotkey::NormalizeShortcutDisplay(L"Control+PageDown") == L"Ctrl+PgDn",
         "normalizes control and page down aliases");
  Expect(hotkey::NormalizeShortcutDisplay(L"windows+period") == L"Win+.",
         "normalizes Windows and punctuation aliases");
  Expect(hotkey::NormalizeShortcutDisplay(L"Alt++").empty(),
         "rejects empty shortcut tokens");
}

void TestVirtualKeyDisplay() {
  namespace hotkey = fp::config_winui;

  Expect(hotkey::ShortcutDisplayForKey('A') == L"A", "formats alpha keys");
  Expect(hotkey::ShortcutDisplayForKey('7') == L"7", "formats numeric keys");
  Expect(hotkey::ShortcutDisplayForKey(VK_F12) == L"F12", "formats function keys");
  Expect(hotkey::ShortcutDisplayForKey(VK_OEM_PERIOD) == L".", "formats period key");
  Expect(hotkey::ShortcutDisplayForKey(VK_RETURN) == L"Enter", "formats enter key");
  Expect(hotkey::ShortcutDisplayForKey(0).empty(), "returns empty for unknown keys");
}

void TestModifierHelpers() {
  namespace hotkey = fp::config_winui;

  Expect(hotkey::IsShortcutModifierKey(VK_LCONTROL), "recognizes left ctrl modifier");
  Expect(!hotkey::IsShortcutModifierKey('A'), "rejects non-modifier keys");
  Expect(hotkey::ShortcutModifierKeyEquals(VK_CONTROL, VK_RCONTROL),
         "matches generic and side-specific ctrl");
  Expect(hotkey::ShortcutModifierKeyEquals(VK_SHIFT, VK_LSHIFT),
         "matches generic and side-specific shift");
  Expect(!hotkey::ShortcutModifierKeyEquals(VK_MENU, VK_LWIN),
         "does not match different modifier families");
}

void TestShortcutDefinitionsAndConflicts() {
  namespace hotkey = fp::config_winui;

  Expect(hotkey::HotkeyShortcutDefinitions().size() == 8,
         "exposes all configurable shortcut definitions");
  Expect(hotkey::HotkeyShortcutDefinitions().front().key ==
             fp::kShortcutToolbarInputModeSetting,
         "keeps input-mode shortcut first for conflict labels");

  const auto read_defaults = [](std::wstring_view,
                                std::wstring_view fallback) {
    return std::wstring(fallback);
  };
  Expect(hotkey::ShortcutConflictTooltip(fp::kShortcutToolbarShapeSetting,
                                         L"Shift",
                                         read_defaults) ==
             L"与 中/英文模式 使用相同热键",
         "reports conflicts against another shortcut");
  Expect(hotkey::ShortcutConflictTooltip(fp::kShortcutToolbarShapeSetting,
                                         L"",
                                         read_defaults) == L"已清除",
         "reports cleared shortcuts");
  Expect(hotkey::ShortcutConflictTooltip(fp::kShortcutToolbarShapeSetting,
                                         L"Ctrl+Alt+K",
                                         read_defaults) == L"点击后按新的组合键",
         "reports non-conflicting shortcuts");
}

}  // namespace

int main() {
  TestNormalizeShortcutDisplay();
  TestVirtualKeyDisplay();
  TestModifierHelpers();
  TestShortcutDefinitionsAndConflicts();
  if (g_failures != 0) {
    std::cerr << g_failures << " config hotkey helper failure(s)\n";
    return 1;
  }
  std::cout << "Config hotkey helper tests passed\n";
  return 0;
}
