#include "tsf/tray_icon_model.h"

#include <iostream>

namespace {

int g_failures = 0;

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

bool RectEquals(const RECT& actual, const RECT& expected) {
  return actual.left == expected.left && actual.top == expected.top &&
         actual.right == expected.right && actual.bottom == expected.bottom;
}

}  // namespace

int main() {
  Expect(fp::tsf::TrayIconUnitsToPixels(fp::tsf::kTrayIconReferenceSize, 144) == 144,
         "reference units fill the render dimension");
  Expect(fp::tsf::TrayIconUnitsToPixels(0, 144) == 1,
         "tray icon units clamp to at least one pixel");
  Expect(fp::tsf::TrayIconUnitsToPixels(72, 144) == 144,
         "tray icon units clamp to the render dimension");

  const RECT status_rect =
      fp::tsf::TrayIconTargetRectHr(fp::tsf::kTrayStatusGlyphWidthUnits,
                                    fp::tsf::kTrayStatusGlyphHeightUnits,
                                    144,
                                    144);
  const RECT expected_status_rect{14, 6, 130, 138};
  Expect(RectEquals(status_rect, expected_status_rect),
         "tray status glyph target keeps centered proportions");

  const RECT disabled_mark =
      fp::tsf::TrayIconTargetRectHr(fp::tsf::kTrayDisabledMarkSideUnits,
                                    fp::tsf::kTrayDisabledMarkSideUnits,
                                    144,
                                    144);
  const RECT expected_disabled_mark{40, 40, 104, 104};
  Expect(RectEquals(disabled_mark, expected_disabled_mark),
         "disabled mark target is centered");

  return g_failures == 0 ? 0 : 1;
}
