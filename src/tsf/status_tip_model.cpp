#include "tsf/status_tip_model.h"

#include <algorithm>
#include <cmath>

namespace fp::tsf {
namespace {

constexpr int kStatusTipTextIconGapDips = 6;
constexpr int kStatusTipIconSizeDips = 24;
constexpr float kStatusTipDetailIconScale = 0.78f;
constexpr int kStatusTipMouseOffsetXDips = 4;
constexpr int kStatusTipMouseOffsetYDips = 7;

int ScaleForDpi(int value, UINT dpi) {
  return MulDiv(value, static_cast<int>(dpi), 96);
}

}  // namespace

int StatusTipTextIconGap(UINT dpi) {
  return ScaleForDpi(kStatusTipTextIconGapDips, dpi);
}

int StatusTipDetailIconSize(UINT dpi) {
  const int base_size = ScaleForDpi(kStatusTipIconSizeDips, dpi);
  return std::max(
      1,
      static_cast<int>(std::lround(static_cast<float>(base_size) * kStatusTipDetailIconScale)));
}

int StatusTipMouseOffsetX(UINT dpi) {
  return ScaleForDpi(kStatusTipMouseOffsetXDips, dpi);
}

int StatusTipMouseOffsetY(UINT dpi) {
  return ScaleForDpi(kStatusTipMouseOffsetYDips, dpi);
}

}  // namespace fp::tsf
