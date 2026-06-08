#include "tsf/input_mode_state.h"

namespace fp::tsf {

CapsLockTransition ResolveCapsLockTransition(bool current_ascii_mode,
                                             bool current_caps_lock_ascii_mode,
                                             bool caps_lock_on) noexcept {
  if (caps_lock_on) {
    return {true, true};
  }
  if (current_caps_lock_ascii_mode) {
    return {false, false};
  }
  return {current_ascii_mode, false};
}

CapsLockTransition ResolveToggleAsciiTransition(bool current_ascii_mode,
                                                bool caps_lock_source) noexcept {
  const bool ascii_mode = !current_ascii_mode;
  return {ascii_mode, ascii_mode && caps_lock_source};
}

}  // namespace fp::tsf
