#include "tsf/tray_icon_model.h"

#include <algorithm>

namespace fp::tsf {

int TrayIconUnitsToPixels(int units, int dimension) {
  return std::clamp(MulDiv(units, dimension, kTrayIconReferenceSize), 1, dimension);
}

RECT TrayIconTargetRectHr(int units_width,
                          int units_height,
                          int render_width,
                          int render_height) {
  const int target_width = TrayIconUnitsToPixels(units_width, render_width);
  const int target_height = TrayIconUnitsToPixels(units_height, render_height);
  const int target_left = std::max(0, (render_width - target_width) / 2);
  const int target_top = std::max(0, (render_height - target_height) / 2);
  return RECT{target_left,
              target_top,
              std::min(render_width, target_left + target_width),
              std::min(render_height, target_top + target_height)};
}

}  // namespace fp::tsf
