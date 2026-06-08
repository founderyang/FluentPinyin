#pragma once

#include "tsf/candidate_layout_model.h"

#include <windows.h>

namespace fp::tsf {

enum CandidateToolId : int {
  kCandidateToolNone = 0,
  kCandidateToolPrevious = 1,
  kCandidateToolNext = 2,
  kCandidateToolEmoji = 3,
  kCandidateToolExpand = 4,
  kCandidateToolSettings = 5,
  kCandidateToolBrand = 6,
};

inline constexpr int kCandidateToolButtonSize = 18;
inline constexpr int kCandidateToolFeedbackSize = 20;
inline constexpr int kCandidateToolCandidateGap = 6;
inline constexpr int kCandidateToolTooltipHeightDips = 33;
inline constexpr int kCandidateToolTooltipOverlapDips = 6;
inline constexpr int kCandidateToolTooltipPaddingDips = 10;
inline constexpr int kCandidateToolTooltipCornerRadiusHalfDips = 5;

RECT CandidateToolRect(const CandidateLayoutMetrics& layout, int tool);
RECT CandidateToolIconRect(const CandidateLayoutMetrics& layout, int tool);
int CandidateToolAtPoint(const CandidateLayoutMetrics& layout, int x, int y);
bool IsCandidateToolEnabled(int tool, bool has_previous_page, bool has_next_page);
RECT CandidateToolFeedbackRect(RECT rect, int tool, UINT dpi, bool centered_tools);
int CandidateToolTooltipHorizontalPadding(UINT dpi);
int CandidateToolTooltipHeight(UINT dpi);
int CandidateToolTooltipOverlap(UINT dpi);
int CandidateToolTooltipCornerRadius(UINT dpi);

}  // namespace fp::tsf
