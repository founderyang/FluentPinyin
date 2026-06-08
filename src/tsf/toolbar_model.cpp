#include "tsf/toolbar_model.h"

#include "common/encoding.h"

#include <algorithm>

namespace fp::tsf {
namespace {

constexpr int kToolbarHeightDips = 38;
constexpr int kToolbarDragWidthDips = 24;
constexpr int kToolbarItemSlotDips = 32;
constexpr int kToolbarTailPaddingHalfDips = 4;
constexpr int kToolbarFeedbackWidthDips = 24;
constexpr int kToolbarFeedbackHeightDips = 28;
constexpr int kToolbarFeedbackEdgeInsetDips = 4;
constexpr int kToolbarTooltipMinWidthDips = 0;
constexpr int kToolbarTooltipHeightDips = 30;
constexpr int kToolbarTooltipPaddingDips = 8;
constexpr int kToolbarTooltipOverhangGuardDips = 2;
constexpr int kToolbarTooltipCornerRadiusHalfDips = 7;
constexpr int kToolbarTooltipMouseOffsetYDips = 9;

struct ToolbarItemVisualDips {
  int left_offset;
  int top;
  int width;
  int height;
};

int ScaleForDpi(int value, UINT dpi) {
  return MulDiv(value, static_cast<int>(dpi), 96);
}

bool PtInRectInclusive(const RECT& rect, int x, int y) {
  if (rect.right <= rect.left || rect.bottom <= rect.top) {
    return false;
  }
  return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

ToolbarItemVisualDips ToolbarItemVisualMetricsDips(int item) {
  switch (item) {
    case kToolbarItemInputMode:
      return {6, 3, 20, 30};
    case kToolbarItemShape:
      return {7, 2, 19, 32};
    case kToolbarItemPunctuation:
      return {-1, 3, 29, 30};
    case kToolbarItemCharset:
      return {3, 3, 24, 30};
    case kToolbarItemEmoji:
      return {6, 3, 21, 30};
    case kToolbarItemSettings:
      return {4, 3, 22, 30};
    default:
      return {4, 3, 24, 30};
  }
}

}  // namespace

bool IsToolbarCustomItem(int item) {
  return std::any_of(kToolbarCustomItemDefinitions.begin(),
                     kToolbarCustomItemDefinitions.end(),
                     [item](const ToolbarItemDefinition& definition) {
                       return definition.id == item;
                     });
}

std::wstring_view ToolbarItemSettingId(int item) {
  for (const auto& definition : kToolbarCustomItemDefinitions) {
    if (definition.id == item) {
      return definition.setting_id;
    }
  }
  return {};
}

std::wstring ToolbarCustomItemLabel(int item) {
  for (const auto& definition : kToolbarCustomItemDefinitions) {
    if (definition.id == item) {
      return std::wstring(definition.custom_label);
    }
  }
  return {};
}

std::vector<int> DefaultToolbarVisibleItems() {
  std::vector<int> items;
  items.reserve(kToolbarCustomItemDefinitions.size());
  for (const auto& definition : kToolbarCustomItemDefinitions) {
    items.push_back(definition.id);
  }
  items.push_back(kToolbarItemSettings);
  return items;
}

bool ContainsToolbarItem(const std::vector<int>& items, int item) {
  return std::find(items.begin(), items.end(), item) != items.end();
}

std::vector<int> ParseToolbarVisibleItems(std::wstring_view value) {
  if (fp::TrimWhitespace(value).empty()) {
    return DefaultToolbarVisibleItems();
  }
  std::vector<int> items;
  size_t start = 0;
  while (start <= value.size()) {
    const size_t separator = value.find(L',', start);
    const size_t end = separator == std::wstring_view::npos ? value.size() : separator;
    const std::wstring token =
        fp::ToLowerInvariant(fp::TrimWhitespace(value.substr(start, end - start)));
    for (const auto& definition : kToolbarCustomItemDefinitions) {
      if (token == definition.setting_id && !ContainsToolbarItem(items, definition.id)) {
        items.push_back(definition.id);
        break;
      }
    }
    if (separator == std::wstring_view::npos) {
      break;
    }
    start = separator + 1;
  }
  if (!ContainsToolbarItem(items, kToolbarItemSettings)) {
    items.push_back(kToolbarItemSettings);
  }
  return items.empty() ? DefaultToolbarVisibleItems() : items;
}

std::wstring SerializeToolbarVisibleItems(const std::vector<int>& items) {
  std::wstring value;
  for (int item : items) {
    const std::wstring_view id = ToolbarItemSettingId(item);
    if (id.empty()) {
      continue;
    }
    if (!value.empty()) {
      value += L",";
    }
    value += id;
  }
  return value;
}

std::vector<int> VisibleToolbarItemsWithSettings(const std::vector<int>& items) {
  std::vector<int> visible;
  visible.reserve(kToolbarCustomItemDefinitions.size());
  for (const auto& definition : kToolbarCustomItemDefinitions) {
    if (ContainsToolbarItem(items, definition.id)) {
      visible.push_back(definition.id);
    }
  }
  if (!ContainsToolbarItem(visible, kToolbarItemSettings)) {
    visible.push_back(kToolbarItemSettings);
  }
  return visible;
}

int ScaleToolbarHalfDipsFloor(int half_dips, UINT dpi) {
  return static_cast<int>((static_cast<long long>(half_dips) * static_cast<long long>(dpi)) / 192);
}

int ToolbarThicknessPixels(UINT dpi) {
  return ScaleToolbarHalfDipsFloor(kToolbarHeightDips * 2, dpi);
}

int ToolbarWindowWidthPixels(bool vertical, size_t item_count, UINT dpi) {
  if (vertical) {
    return ToolbarThicknessPixels(dpi);
  }
  const int base_dips =
      kToolbarDragWidthDips + static_cast<int>(item_count) * kToolbarItemSlotDips;
  return ScaleForDpi(base_dips, dpi) + ScaleToolbarHalfDipsFloor(kToolbarTailPaddingHalfDips, dpi);
}

int ToolbarWindowHeightPixels(bool vertical, size_t item_count, UINT dpi) {
  if (!vertical) {
    return ToolbarThicknessPixels(dpi);
  }
  const int base_dips =
      kToolbarDragWidthDips + static_cast<int>(item_count) * kToolbarItemSlotDips;
  return ScaleForDpi(base_dips, dpi) + ScaleToolbarHalfDipsFloor(kToolbarTailPaddingHalfDips, dpi);
}

int ToolbarTooltipMinWidth(UINT dpi) {
  return ScaleForDpi(kToolbarTooltipMinWidthDips, dpi);
}

int ToolbarTooltipHeight(UINT dpi) {
  return ScaleForDpi(kToolbarTooltipHeightDips, dpi);
}

int ToolbarTooltipPadding(UINT dpi) {
  return ScaleForDpi(kToolbarTooltipPaddingDips, dpi);
}

int ToolbarTooltipOverhangGuard(UINT dpi) {
  return ScaleForDpi(kToolbarTooltipOverhangGuardDips, dpi);
}

int ToolbarTooltipCornerRadius(UINT dpi) {
  return ScaleToolbarHalfDipsFloor(kToolbarTooltipCornerRadiusHalfDips, dpi);
}

int ToolbarTooltipMouseOffsetY(UINT dpi) {
  return ScaleForDpi(kToolbarTooltipMouseOffsetYDips, dpi);
}

std::vector<ToolbarItemMetrics> ToolbarItemsForDpi(UINT dpi,
                                                   bool vertical,
                                                   const std::vector<int>& visible_items) {
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const std::vector<int> items = VisibleToolbarItemsWithSettings(visible_items);
  const auto item = [&](int id, size_t index) {
    const ToolbarItemVisualDips visual = ToolbarItemVisualMetricsDips(id);
    RECT visual_rect{};
    RECT slot_rect{};
    if (vertical) {
      const int slot_top = kToolbarDragWidthDips + static_cast<int>(index) * kToolbarItemSlotDips;
      const int visual_left = (kToolbarHeightDips - visual.height) / 2;
      const int visual_top = slot_top + (kToolbarItemSlotDips - visual.width) / 2;
      slot_rect = RECT{s(0), s(slot_top), s(kToolbarHeightDips), s(slot_top + kToolbarItemSlotDips)};
      visual_rect = RECT{s(visual_left),
                         s(visual_top),
                         s(visual_left + visual.height),
                         s(visual_top + visual.width)};
    } else {
      const int slot_left = kToolbarDragWidthDips + static_cast<int>(index) * kToolbarItemSlotDips;
      slot_rect =
          RECT{s(slot_left), s(0), s(slot_left + kToolbarItemSlotDips), s(kToolbarHeightDips)};
      visual_rect = RECT{s(slot_left + visual.left_offset),
                         s(visual.top),
                         s(slot_left + visual.left_offset + visual.width),
                         s(visual.top + visual.height)};
    }
    const int feedback_width =
        vertical ? s(kToolbarFeedbackHeightDips) : s(kToolbarFeedbackWidthDips);
    const int feedback_height =
        vertical ? s(kToolbarFeedbackWidthDips) : s(kToolbarFeedbackHeightDips);
    const int toolbar_width = ToolbarWindowWidthPixels(vertical, items.size(), dpi);
    const int toolbar_height = ToolbarWindowHeightPixels(vertical, items.size(), dpi);
    const int edge_inset = s(kToolbarFeedbackEdgeInsetDips);
    const int slot_width = std::max(1, static_cast<int>(slot_rect.right - slot_rect.left));
    const int slot_height = std::max(1, static_cast<int>(slot_rect.bottom - slot_rect.top));
    const int centered_feedback_left = slot_rect.left + (slot_width - feedback_width) / 2;
    const int feedback_left =
        std::clamp(centered_feedback_left,
                   edge_inset,
                   std::max(edge_inset, toolbar_width - edge_inset - feedback_width));
    const int feedback_top = slot_rect.top + (slot_height - feedback_height) / 2;
    const int clamped_feedback_top =
        std::clamp(feedback_top,
                   edge_inset,
                   std::max(edge_inset, toolbar_height - edge_inset - feedback_height));
    const RECT feedback_rect{feedback_left,
                             clamped_feedback_top,
                             feedback_left + feedback_width,
                             clamped_feedback_top + feedback_height};
    if (id == kToolbarItemCharset || id == kToolbarItemSettings) {
      const int visual_width =
          std::max(1, static_cast<int>(visual_rect.right - visual_rect.left));
      const int visual_height =
          std::max(1, static_cast<int>(visual_rect.bottom - visual_rect.top));
      const int visual_left = (feedback_rect.left + feedback_rect.right - visual_width) / 2;
      const int visual_top = (feedback_rect.top + feedback_rect.bottom - visual_height) / 2;
      visual_rect = RECT{visual_left,
                         visual_top,
                         visual_left + visual_width,
                         visual_top + visual_height};
    }
    return ToolbarItemMetrics{
        id,
        feedback_rect,
        visual_rect,
        feedback_rect,
    };
  };

  std::vector<ToolbarItemMetrics> metrics;
  metrics.reserve(items.size());
  for (size_t index = 0; index < items.size(); ++index) {
    metrics.push_back(item(items[index], index));
  }
  return metrics;
}

RECT ToolbarItemIconRect(const ToolbarItemMetrics& item, UINT dpi, int inset_dips) {
  const int inset = ScaleForDpi(inset_dips, dpi);
  RECT rect{item.visual_rect.left + inset,
            item.visual_rect.top + inset,
            item.visual_rect.right - inset,
            item.visual_rect.bottom - inset};
  if (rect.right <= rect.left || rect.bottom <= rect.top) {
    return item.visual_rect;
  }
  return rect;
}

int ToolbarItemAtPoint(UINT dpi, bool vertical, const std::vector<int>& visible_items, int x, int y) {
  if ((!vertical && x < ScaleForDpi(kToolbarDragWidthDips, dpi)) ||
      (vertical && y < ScaleForDpi(kToolbarDragWidthDips, dpi))) {
    return kToolbarItemDrag;
  }
  const std::vector<ToolbarItemMetrics> items = ToolbarItemsForDpi(dpi, vertical, visible_items);
  for (const auto& item : items) {
    if (PtInRectInclusive(item.hit_rect, x, y)) {
      return item.id;
    }
  }
  return kToolbarItemNone;
}

}  // namespace fp::tsf
