#pragma once

#include <windows.h>

namespace fp {

inline bool RectHasPositiveArea(const RECT& rect) {
  return rect.right > rect.left && rect.bottom > rect.top;
}

inline bool PointInRectInclusive(const RECT& rect, int x, int y) {
  if (!RectHasPositiveArea(rect)) {
    return false;
  }
  return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

}  // namespace fp
