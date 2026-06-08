#pragma once

namespace fp::tsf {

struct CapsLockTransition {
  bool ascii_mode = false;
  bool caps_lock_ascii_mode = false;
};

CapsLockTransition ResolveCapsLockTransition(bool current_ascii_mode,
                                             bool current_caps_lock_ascii_mode,
                                             bool caps_lock_on) noexcept;
CapsLockTransition ResolveToggleAsciiTransition(bool current_ascii_mode,
                                                bool caps_lock_source) noexcept;

}  // namespace fp::tsf
