#pragma once

#include <windows.h>

namespace fp {

inline int ScaleForDpi(int value, UINT dpi) {
  return MulDiv(value, static_cast<int>(dpi), 96);
}

inline int ScaleHalfDipForDpi(int half_dips, UINT dpi) {
  return MulDiv(half_dips, static_cast<int>(dpi), 192);
}

inline int ScaleHalfDipFloorForDpi(int half_dips, UINT dpi) {
  return static_cast<int>((static_cast<long long>(half_dips) * static_cast<long long>(dpi)) / 192);
}

}  // namespace fp
