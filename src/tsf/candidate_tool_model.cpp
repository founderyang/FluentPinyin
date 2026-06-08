#include "tsf/candidate_tool_model.h"

#include "common/dpi.h"
#include "common/win32_geometry.h"

namespace fp::tsf {
namespace {

}  // namespace

RECT CandidateToolRect(const CandidateLayoutMetrics& layout, int tool) {
  switch (tool) {
    case kCandidateToolPrevious:
      return layout.previous_page_rect;
    case kCandidateToolNext:
      return layout.next_page_rect;
    case kCandidateToolEmoji:
      return layout.emoji_rect;
    case kCandidateToolExpand:
      return layout.expand_rect;
    case kCandidateToolSettings:
      return layout.settings_rect;
    case kCandidateToolBrand:
      return layout.brand_rect;
    default:
      return RECT{};
  }
}

RECT CandidateToolIconRect(const CandidateLayoutMetrics& layout, int tool) {
  switch (tool) {
    case kCandidateToolPrevious:
      return layout.previous_page_icon_rect.right > layout.previous_page_icon_rect.left
                 ? layout.previous_page_icon_rect
                 : layout.previous_page_rect;
    case kCandidateToolNext:
      return layout.next_page_icon_rect.right > layout.next_page_icon_rect.left
                 ? layout.next_page_icon_rect
                 : layout.next_page_rect;
    case kCandidateToolEmoji:
      return layout.emoji_icon_rect.right > layout.emoji_icon_rect.left ? layout.emoji_icon_rect
                                                                         : layout.emoji_rect;
    case kCandidateToolExpand:
      return layout.expand_icon_rect.right > layout.expand_icon_rect.left ? layout.expand_icon_rect
                                                                          : layout.expand_rect;
    default:
      return CandidateToolRect(layout, tool);
  }
}

int CandidateToolAtPoint(const CandidateLayoutMetrics& layout, int x, int y) {
  if (fp::PointInRectInclusive(layout.previous_page_rect, x, y)) {
    return kCandidateToolPrevious;
  }
  if (fp::PointInRectInclusive(layout.next_page_rect, x, y)) {
    return kCandidateToolNext;
  }
  if (fp::PointInRectInclusive(layout.emoji_rect, x, y)) {
    return kCandidateToolEmoji;
  }
  if (fp::PointInRectInclusive(layout.expand_rect, x, y)) {
    return kCandidateToolExpand;
  }
  if (fp::PointInRectInclusive(layout.settings_rect, x, y)) {
    return kCandidateToolSettings;
  }
  if (fp::PointInRectInclusive(layout.brand_rect, x, y)) {
    return kCandidateToolBrand;
  }
  return kCandidateToolNone;
}

bool IsCandidateToolEnabled(int tool, bool has_previous_page, bool has_next_page) {
  switch (tool) {
    case kCandidateToolPrevious:
      return has_previous_page;
    case kCandidateToolNext:
      return has_next_page;
    case kCandidateToolEmoji:
    case kCandidateToolExpand:
    case kCandidateToolSettings:
    case kCandidateToolBrand:
      return true;
    default:
      return false;
  }
}

RECT CandidateToolFeedbackRect(RECT rect, int tool, UINT dpi, bool centered_tools) {
  if (tool == kCandidateToolBrand) {
    const int vertical_padding = fp::ScaleForDpi(5, dpi);
    const int horizontal_padding = 0;
    RECT feedback{rect.left - horizontal_padding,
                  rect.top - vertical_padding,
                  rect.right + horizontal_padding,
                  rect.bottom + vertical_padding};
    if (feedback.bottom <= feedback.top) {
      feedback.top = rect.top;
      feedback.bottom = rect.bottom;
    }
    return feedback;
  }
  const int side = fp::ScaleHalfDipForDpi(kCandidateToolFeedbackSize * 2 - 1, dpi);
  if (centered_tools && tool >= kCandidateToolPrevious && tool <= kCandidateToolExpand) {
    const int rect_width = rect.right - rect.left;
    const int rect_height = rect.bottom - rect.top;
    if (rect_width <= rect_height + fp::ScaleForDpi(4, dpi)) {
      return rect;
    }
    const int vertical_inset = fp::ScaleHalfDipForDpi(3, dpi);
    const int bottom_inset = fp::ScaleHalfDipForDpi(7, dpi);
    RECT feedback{rect.left,
                  rect.top + vertical_inset,
                  rect.right,
                  rect.bottom - bottom_inset};
    if (feedback.bottom <= feedback.top) {
      feedback.top = rect.top;
      feedback.bottom = rect.bottom;
    }
    return feedback;
  }
  const int center_x = rect.left + (rect.right - rect.left) / 2;
  const int center_y = rect.top + (rect.bottom - rect.top) / 2;
  int visual_offset_x = 0;
  if (!centered_tools) {
    switch (tool) {
      case kCandidateToolPrevious:
        visual_offset_x = -fp::ScaleHalfDipForDpi(4, dpi);
        break;
      case kCandidateToolEmoji:
        visual_offset_x = -fp::ScaleHalfDipForDpi(3, dpi);
        break;
      case kCandidateToolExpand:
        visual_offset_x = -fp::ScaleHalfDipForDpi(6, dpi);
        break;
      case kCandidateToolSettings:
        break;
      default:
        break;
    }
  }
  const int left = center_x + visual_offset_x - side / 2 + fp::ScaleHalfDipForDpi(1, dpi);
  const int top = center_y - side / 2 + fp::ScaleHalfDipForDpi(1, dpi);
  return RECT{left, top, left + side, top + side};
}

int CandidateToolTooltipHorizontalPadding(UINT dpi) {
  return fp::ScaleForDpi(kCandidateToolTooltipPaddingDips, dpi);
}

int CandidateToolTooltipOverhangGuard(UINT dpi) {
  return fp::ScaleForDpi(kCandidateToolTooltipOverhangGuardDips, dpi);
}

int CandidateToolTooltipHeight(UINT dpi) {
  return fp::ScaleForDpi(kCandidateToolTooltipHeightDips, dpi);
}

int CandidateToolTooltipOverlap(UINT dpi) {
  return fp::ScaleForDpi(kCandidateToolTooltipOverlapDips, dpi);
}

int CandidateToolTooltipCornerRadius(UINT dpi) {
  return fp::ScaleHalfDipForDpi(kCandidateToolTooltipCornerRadiusHalfDips, dpi);
}

}  // namespace fp::tsf
