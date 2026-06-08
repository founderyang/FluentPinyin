#include "tsf/candidate_tool_model.h"

#include <iostream>

namespace {

int g_failures = 0;

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

bool RectEquals(const RECT& actual, const RECT& expected) {
  return actual.left == expected.left && actual.top == expected.top &&
         actual.right == expected.right && actual.bottom == expected.bottom;
}

}  // namespace

int main() {
  fp::tsf::CandidateLayoutMetrics layout;
  layout.previous_page_rect = {0, 0, 20, 20};
  layout.next_page_rect = {24, 0, 44, 20};
  layout.emoji_rect = {48, 0, 68, 20};
  layout.expand_rect = {72, 0, 92, 20};
  layout.settings_rect = {96, 0, 116, 20};
  layout.brand_rect = {0, 28, 70, 48};
  layout.emoji_icon_rect = {50, 2, 66, 18};

  Expect(RectEquals(fp::tsf::CandidateToolRect(layout, fp::tsf::kCandidateToolNext),
                    layout.next_page_rect),
         "tool rect returns the requested candidate tool");
  Expect(RectEquals(fp::tsf::CandidateToolIconRect(layout, fp::tsf::kCandidateToolEmoji),
                    layout.emoji_icon_rect),
         "tool icon rect prefers explicit icon geometry");
  Expect(RectEquals(fp::tsf::CandidateToolIconRect(layout, fp::tsf::kCandidateToolExpand),
                    layout.expand_rect),
         "tool icon rect falls back to tool geometry");
  Expect(fp::tsf::CandidateToolAtPoint(layout, 20, 20) ==
             fp::tsf::kCandidateToolPrevious,
         "tool hit testing keeps right and bottom edges inclusive");
  Expect(fp::tsf::CandidateToolAtPoint(layout, 95, 20) ==
             fp::tsf::kCandidateToolNone,
         "tool hit testing ignores gaps");

  Expect(!fp::tsf::IsCandidateToolEnabled(fp::tsf::kCandidateToolPrevious,
                                          false,
                                          true),
         "previous page tool follows availability flag");
  Expect(fp::tsf::IsCandidateToolEnabled(fp::tsf::kCandidateToolNext, false, true),
         "next page tool follows availability flag");
  Expect(fp::tsf::IsCandidateToolEnabled(fp::tsf::kCandidateToolSettings, false, false),
         "settings tool is always enabled");
  Expect(!fp::tsf::IsCandidateToolEnabled(fp::tsf::kCandidateToolNone, true, true),
         "none is not an enabled tool");

  const RECT centered_source{0, 0, 80, 24};
  const RECT centered_feedback =
      fp::tsf::CandidateToolFeedbackRect(centered_source,
                                         fp::tsf::kCandidateToolNext,
                                         96,
                                         true);
  const RECT expected_centered_feedback{0, 1, 80, 20};
  Expect(RectEquals(centered_feedback, expected_centered_feedback),
         "wide centered tool feedback preserves full width with vertical insets");

  const RECT brand_source{10, 10, 80, 30};
  const RECT brand_feedback =
      fp::tsf::CandidateToolFeedbackRect(brand_source,
                                         fp::tsf::kCandidateToolBrand,
                                         96,
                                         false);
  const RECT expected_brand_feedback{10, 5, 80, 35};
  Expect(RectEquals(brand_feedback, expected_brand_feedback),
         "brand feedback expands vertically");

  return g_failures == 0 ? 0 : 1;
}
