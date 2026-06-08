#include "tsf/input_mode_state.h"

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
  {
    const auto state = fp::tsf::ResolveCapsLockTransition(false, false, true);
    Expect(state.ascii_mode, "CapsLock on enters ASCII mode from Chinese");
    Expect(state.caps_lock_ascii_mode,
           "CapsLock on marks ASCII mode as CapsLock-owned");
  }
  {
    const auto state = fp::tsf::ResolveCapsLockTransition(true, true, false);
    Expect(!state.ascii_mode, "CapsLock off returns to Chinese when it owned ASCII mode");
    Expect(!state.caps_lock_ascii_mode, "CapsLock ownership clears after turning off");
  }
  {
    const auto state = fp::tsf::ResolveCapsLockTransition(true, false, false);
    Expect(state.ascii_mode,
           "CapsLock off preserves manually selected ASCII mode");
    Expect(!state.caps_lock_ascii_mode,
           "Manual ASCII mode remains non-CapsLock-owned");
  }
  {
    const auto state = fp::tsf::ResolveCapsLockTransition(false, false, false);
    Expect(!state.ascii_mode, "CapsLock off preserves Chinese mode");
    Expect(!state.caps_lock_ascii_mode,
           "Chinese mode remains non-CapsLock-owned");
  }
  {
    const auto state = fp::tsf::ResolveToggleAsciiTransition(false, false);
    Expect(state.ascii_mode, "Manual toggle enters ASCII mode from Chinese");
    Expect(!state.caps_lock_ascii_mode,
           "Manual toggle does not mark ASCII as CapsLock-owned");
  }
  {
    const auto state = fp::tsf::ResolveToggleAsciiTransition(true, false);
    Expect(!state.ascii_mode, "Manual toggle returns from ASCII to Chinese");
    Expect(!state.caps_lock_ascii_mode,
           "Manual toggle clears CapsLock ownership");
  }
  {
    const auto state = fp::tsf::ResolveToggleAsciiTransition(true, true);
    Expect(!state.ascii_mode,
           "Manual toggle returns from CapsLock-owned ASCII to Chinese");
    Expect(!state.caps_lock_ascii_mode,
           "Manual toggle clears stale CapsLock ownership");
  }
  {
    const auto state = fp::tsf::ResolveToggleAsciiTransition(false, true);
    Expect(state.ascii_mode, "CapsLock toggle enters ASCII mode");
    Expect(state.caps_lock_ascii_mode,
           "CapsLock toggle marks ASCII mode as CapsLock-owned");
  }

  return g_failures == 0 ? 0 : 1;
}
