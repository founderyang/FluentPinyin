#include "common/dpi.h"

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
  Expect(fp::ScaleForDpi(10, 96) == 10, "base dpi leaves DIP values unchanged");
  Expect(fp::ScaleForDpi(10, 144) == 15, "DIP scaling uses MulDiv rounding");
  Expect(fp::ScaleHalfDipForDpi(7, 96) == 4,
         "half-DIP scaling preserves MulDiv rounding");
  Expect(fp::ScaleHalfDipFloorForDpi(7, 96) == 3,
         "half-DIP floor scaling preserves truncation semantics");
  Expect(fp::ScaleHalfDipFloorForDpi(4, 192) == 4,
         "half-DIP floor scaling handles exact integer scale");

  return g_failures == 0 ? 0 : 1;
}
