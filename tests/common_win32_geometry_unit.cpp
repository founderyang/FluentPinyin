#include "common/win32_geometry.h"

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
  const RECT rect{10, 20, 30, 40};
  Expect(fp::RectHasPositiveArea(rect), "positive rect reports area");
  Expect(fp::PointInRectInclusive(rect, 10, 20),
         "inclusive hit test accepts top-left edge");
  Expect(fp::PointInRectInclusive(rect, 30, 40),
         "inclusive hit test accepts bottom-right edge");
  Expect(!fp::PointInRectInclusive(rect, 31, 40),
         "inclusive hit test rejects points outside right edge");
  Expect(!fp::PointInRectInclusive(RECT{0, 0, 0, 10}, 0, 5),
         "zero-width rect is not hittable");
  Expect(!fp::PointInRectInclusive(RECT{0, 10, 10, 0}, 5, 5),
         "inverted rect is not hittable");

  return g_failures == 0 ? 0 : 1;
}
