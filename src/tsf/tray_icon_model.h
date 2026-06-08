#pragma once

#include <windows.h>

namespace fp::tsf {

inline constexpr int kTrayIconReferenceSize = 36;
inline constexpr int kTrayStatusGlyphWidthUnits = 29;
inline constexpr int kTrayStatusGlyphHeightUnits = 33;
inline constexpr int kTrayDisabledCircleSideUnits = kTrayStatusGlyphHeightUnits;
inline constexpr int kTrayDisabledMarkSideUnits = 16;

int TrayIconUnitsToPixels(int units, int dimension);
RECT TrayIconTargetRectHr(int units_width,
                          int units_height,
                          int render_width,
                          int render_height);

}  // namespace fp::tsf
