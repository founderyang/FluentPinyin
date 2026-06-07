#include "config_winui/hotkey_helpers.h"

#include <windows.h>

#include <iostream>
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

}  // namespace

int main() {
  TestNormalizeShortcutDisplay();
  TestVirtualKeyDisplay();
  TestModifierHelpers();
  if (g_failures != 0) {
    std::cerr << g_failures << " config hotkey helper failure(s)\n";
    return 1;
  }
  std::cout << "Config hotkey helper tests passed\n";
  return 0;
}
