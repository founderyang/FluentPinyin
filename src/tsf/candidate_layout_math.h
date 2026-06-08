#pragma once

#include <windows.h>

#include "tsf/candidate_layout_model.h"

#include <cstddef>
#include <vector>

namespace fp::tsf {

inline constexpr int kHorizontalExpandedTailRows = 3;
inline constexpr int kVerticalExpandedCandidateColumns = 4;
inline constexpr int kVerticalExpandedCandidateRows = 9;

int CompactCandidateCount(int compact_count);
int ExpandedCandidateColumnCount(int compact_count);
int ExpandedCandidateColumnCount(bool horizontal, int compact_count);
int ExpandedCandidateRowCount(bool horizontal);
int ExpandedCandidatePageSize(bool horizontal, int compact_count);
int ExpandedCandidatePageSize(int compact_count);
int CandidateItemHeightDips(int base_dips, int candidate_font_point_size);
int CandidateRowStepDips(int base_dips, int candidate_font_point_size);
int ScaleCandidateSelectionMark(int base_pixels, int item_height, UINT dpi);
bool IsSelectableCandidateRect(const RECT& rect);
std::vector<size_t> SelectableCandidateIndices(const CandidateLayoutMetrics& layout,
                                               size_t candidate_count);
int CandidatePageSizeLimit(bool horizontal, bool expanded, int compact_count);
int CandidatePageSizeLimit(bool expanded, int compact_count);

}  // namespace fp::tsf
