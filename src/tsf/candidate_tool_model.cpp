#include "tsf/candidate_tool_model.h"

namespace fp::tsf {
namespace {

bool PtInRectInclusive(const RECT& rect, int x, int y) {
  return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

int ScaleForDpi(int value, UINT dpi) {
  return MulDiv(value, static_cast<int>(dpi), 96);
}

int ScaleHalfDipForDpi(int half_dips, UINT dpi) {
  return MulDiv(half_dips, static_cast<int>(dpi), 192);
}

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
  if (PtInRectInclusive(layout.previous_page_rect, x, y)) {
    return kCandidateToolPrevious;
  }
  if (PtInRectInclusive(layout.next_page_rect, x, y)) {
    return kCandidateToolNext;
  }
  if (PtInRectInclusive(layout.emoji_rect, x, y)) {
    return kCandidateToolEmoji;
  }
  if (PtInRectInclusive(layout.expand_rect, x, y)) {
    return kCandidateToolExpand;
  }
  if (PtInRectInclusive(layout.settings_rect, x, y)) {
    return kCandidateToolSettings;
  }
  if (PtInRectInclusive(layout.brand_rect, x, y)) {
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
    const int vertical_padding = ScaleForDpi(5, dpi);
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
  const int side = ScaleHalfDipForDpi(kCandidateToolFeedbackSize * 2 - 1, dpi);
  if (centered_tools && tool >= kCandidateToolPrevious && tool <= kCandidateToolExpand) {
    const int rect_width = rect.right - rect.left;
    const int rect_height = rect.bottom - rect.top;
    if (rect_width <= rect_height + ScaleForDpi(4, dpi)) {
      return rect;
    }
    const int vertical_inset = ScaleHalfDipForDpi(3, dpi);
    const int bottom_inset = ScaleHalfDipForDpi(7, dpi);
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
        visual_offset_x = -ScaleHalfDipForDpi(4, dpi);
        break;
      case kCandidateToolEmoji:
        visual_offset_x = -ScaleHalfDipForDpi(3, dpi);
        break;
      case kCandidateToolExpand:
        visual_offset_x = -ScaleHalfDipForDpi(6, dpi);
        break;
      case kCandidateToolSettings:
        break;
      default:
        break;
    }
  }
  const int left = center_x + visual_offset_x - side / 2 + ScaleHalfDipForDpi(1, dpi);
  const int top = center_y - side / 2 + ScaleHalfDipForDpi(1, dpi);
  return RECT{left, top, left + side, top + side};
}

}  // namespace fp::tsf
