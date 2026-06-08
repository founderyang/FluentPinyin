#include "tsf/candidate_layout_math.h"

#include "common/constants.h"

#include <algorithm>

namespace fp::tsf {
int CompactCandidateCount(int compact_count) {
  return fp::ClampCandidateCount(compact_count);
}

int ExpandedCandidateColumnCount(int compact_count) {
  return CompactCandidateCount(compact_count);
}

int ExpandedCandidateColumnCount(bool horizontal, int compact_count) {
  return horizontal ? ExpandedCandidateColumnCount(compact_count) : kVerticalExpandedCandidateColumns;
}

int ExpandedCandidateRowCount(bool horizontal) {
  return horizontal ? 1 + kHorizontalExpandedTailRows : kVerticalExpandedCandidateRows;
}

int ExpandedCandidatePageSize(bool horizontal, int compact_count) {
  (void)horizontal;
  return CompactCandidateCount(compact_count) +
         fp::kMaxCandidateCount * kHorizontalExpandedTailRows;
}

int ExpandedCandidatePageSize(int compact_count) {
  return ExpandedCandidatePageSize(true, compact_count);
}

int CandidateItemHeightDips(int base_dips, int candidate_font_point_size) {
  return base_dips +
         std::max(0, candidate_font_point_size - fp::kBaseCandidateFontPointSize) * 2;
}

int CandidateRowStepDips(int base_dips, int candidate_font_point_size) {
  return base_dips +
         std::max(0, candidate_font_point_size - fp::kBaseCandidateFontPointSize) * 2;
}

int ScaleCandidateSelectionMark(int base_pixels, int item_height, UINT dpi) {
  const int base_item_height =
      MulDiv(CandidateItemHeightDips(33, fp::kBaseCandidateFontPointSize),
             static_cast<int>(dpi),
             96);
  return MulDiv(base_pixels, std::max(1, item_height), std::max(1, base_item_height));
}

}  // namespace fp::tsf
