#include "tsf/shortcut_model.h"

#include <iostream>

namespace {

int g_failures = 0;

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

}  // namespace

int main() {
  Expect(fp::tsf::TrimShortcutDisplay(L"  Ctrl + Shift + F  ") ==
             L"Ctrl + Shift + F",
         "shortcut display trim keeps internal spacing");
  Expect(fp::tsf::ShortcutKeyFromToken(L"period") == VK_OEM_PERIOD,
         "period token maps to OEM period");
  Expect(fp::tsf::ShortcutKeyFromToken(L" F12 ") == VK_F12,
         "function key token is normalized and maps to virtual key");

  const fp::tsf::ShortcutChord ctrl_shift_f =
      fp::tsf::ParseShortcutChord(L"Ctrl+Shift+F");
  Expect(ctrl_shift_f.valid, "ctrl shift f parses");
  Expect(ctrl_shift_f.ctrl && ctrl_shift_f.shift && !ctrl_shift_f.alt &&
             !ctrl_shift_f.win,
         "ctrl shift f modifiers parse");
  Expect(ctrl_shift_f.key == 'F', "ctrl shift f key parses");

  const fp::tsf::ShortcutChord win_period = fp::tsf::ParseShortcutChord(L"Win+.");
  Expect(win_period.valid && win_period.win, "win period parses");
  Expect(win_period.key == VK_OEM_PERIOD, "win period key parses");

  const fp::tsf::ShortcutChord control = fp::tsf::ParseShortcutChord(L"Control");
  Expect(control.valid && control.ctrl && control.key == VK_CONTROL,
         "single control parses as modifier key");
  Expect(!fp::tsf::ParseShortcutChord(L"Ctrl+DefinitelyNotAKey").valid,
         "unknown shortcut token is invalid");

  Expect(fp::tsf::ShortcutKeyEquals(VK_SHIFT, VK_LSHIFT),
         "left shift matches generic shift");
  Expect(fp::tsf::ShortcutKeyEquals(VK_CONTROL, VK_RCONTROL),
         "right control matches generic control");
  Expect(fp::tsf::ShortcutKeyEquals(VK_MENU, VK_LMENU),
         "left alt matches generic alt");
  Expect(fp::tsf::IsShortcutModifierVirtualKey(VK_LMENU),
         "left alt is a shortcut modifier");
  Expect(!fp::tsf::IsShortcutModifierVirtualKey('A'),
         "letter key is not a shortcut modifier");

  return g_failures == 0 ? 0 : 1;
}
