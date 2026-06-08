#include "tsf/toolbar_model.h"

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
  const auto defaults = fp::tsf::DefaultToolbarVisibleItems();
  Expect(defaults.size() == 6, "default toolbar shows five custom items plus settings");
  Expect(defaults.back() == fp::tsf::kToolbarItemSettings,
         "settings item is last by default");
  Expect(fp::tsf::IsToolbarCustomItem(fp::tsf::kToolbarItemInputMode),
         "input mode is custom-toggleable");
  Expect(!fp::tsf::IsToolbarCustomItem(fp::tsf::kToolbarItemSettings),
         "settings is pinned, not custom-toggleable");
  Expect(fp::tsf::ToolbarItemSettingId(fp::tsf::kToolbarItemShape) == L"shape",
         "shape setting id is stable");
  Expect(fp::tsf::ToolbarCustomItemLabel(fp::tsf::kToolbarItemEmoji) ==
             L"\u8868\u60C5\u7B26\u53F7/\u7B26\u53F7",
         "emoji label is stable");

  const auto parsed = fp::tsf::ParseToolbarVisibleItems(L"shape, emoji, shape");
  Expect(parsed.size() == 3, "parser deduplicates and appends settings");
  Expect(parsed[0] == fp::tsf::kToolbarItemShape, "parser keeps shape");
  Expect(parsed[1] == fp::tsf::kToolbarItemEmoji, "parser keeps emoji");
  Expect(parsed[2] == fp::tsf::kToolbarItemSettings, "parser appends settings");
  Expect(fp::tsf::SerializeToolbarVisibleItems(parsed) == L"shape,emoji",
         "serializer skips pinned settings");

  const auto visible = fp::tsf::VisibleToolbarItemsWithSettings(
      {fp::tsf::kToolbarItemEmoji, fp::tsf::kToolbarItemInputMode});
  Expect(visible.size() == 3, "visible items include selected custom items plus settings");
  Expect(visible[0] == fp::tsf::kToolbarItemInputMode,
         "visible items follow canonical order");
  Expect(visible[1] == fp::tsf::kToolbarItemEmoji,
         "visible items keep selected emoji");
  Expect(visible[2] == fp::tsf::kToolbarItemSettings,
         "visible items append settings");

  Expect(fp::tsf::ToolbarWindowWidthPixels(false, visible.size(), 96) == 122,
         "horizontal toolbar width follows item count");
  Expect(fp::tsf::ToolbarWindowHeightPixels(false, visible.size(), 96) == 38,
         "horizontal toolbar height is fixed thickness");
  Expect(fp::tsf::ToolbarWindowWidthPixels(true, visible.size(), 96) == 38,
         "vertical toolbar width is fixed thickness");
  Expect(fp::tsf::ToolbarWindowHeightPixels(true, visible.size(), 96) == 122,
         "vertical toolbar height follows item count");

  const auto metrics = fp::tsf::ToolbarItemsForDpi(96, false, visible);
  Expect(metrics.size() == 3, "toolbar metrics follow visible item count");
  Expect(metrics[0].id == fp::tsf::kToolbarItemInputMode,
         "toolbar metrics preserve visible order");
  Expect(fp::tsf::ToolbarItemAtPoint(96, false, visible, 1, 1) ==
             fp::tsf::kToolbarItemDrag,
         "toolbar hit test returns drag zone");
  Expect(fp::tsf::ToolbarItemAtPoint(96, false, visible, metrics[1].hit_rect.left, 10) ==
             fp::tsf::kToolbarItemEmoji,
         "toolbar hit test returns item on inclusive edge");

  const RECT icon_rect = fp::tsf::ToolbarItemIconRect(metrics[0], 96, 1);
  Expect(icon_rect.left > metrics[0].visual_rect.left &&
             icon_rect.right < metrics[0].visual_rect.right,
         "toolbar icon rect applies inset");

  return g_failures == 0 ? 0 : 1;
}
