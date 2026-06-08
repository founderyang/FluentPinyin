#include "tsf/status_tip_model.h"

#include "common/dpi.h"

#include <algorithm>
#include <cmath>

namespace fp::tsf {
namespace {

constexpr int kStatusTipTextIconGapDips = 6;
constexpr int kStatusTipIconSizeDips = 24;
constexpr float kStatusTipDetailIconScale = 0.78f;
constexpr int kStatusTipMouseOffsetXDips = 4;
constexpr int kStatusTipMouseOffsetYDips = 7;

}  // namespace

int StatusTipTextIconGap(UINT dpi) {
  return fp::ScaleForDpi(kStatusTipTextIconGapDips, dpi);
}

int StatusTipDetailIconSize(UINT dpi) {
  const int base_size = fp::ScaleForDpi(kStatusTipIconSizeDips, dpi);
  return std::max(
      1,
      static_cast<int>(std::lround(static_cast<float>(base_size) * kStatusTipDetailIconScale)));
}

int StatusTipMouseOffsetX(UINT dpi) {
  return fp::ScaleForDpi(kStatusTipMouseOffsetXDips, dpi);
}

int StatusTipMouseOffsetY(UINT dpi) {
  return fp::ScaleForDpi(kStatusTipMouseOffsetYDips, dpi);
}

}  // namespace fp::tsf
