#include "tsf/status_tip_model.h"

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
  Expect(fp::tsf::StatusTipTextIconGap(96) == 6,
         "status tip text/icon gap uses base dpi value");
  Expect(fp::tsf::StatusTipTextIconGap(192) == 12,
         "status tip text/icon gap scales with dpi");
  Expect(fp::tsf::StatusTipDetailIconSize(96) == 19,
         "status tip detail icon size preserves rounded scale");
  Expect(fp::tsf::StatusTipMouseOffsetX(96) == 4,
         "status tip horizontal mouse offset uses base dpi value");
  Expect(fp::tsf::StatusTipMouseOffsetY(192) == 14,
         "status tip vertical mouse offset scales with dpi");

  return g_failures == 0 ? 0 : 1;
}
