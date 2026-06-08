#pragma once

#include <windows.h>

#include <vector>

namespace fp::tsf {

struct CandidateLayoutMetrics {
  int width = 0;
  int height = 0;
  int top_height = 0;
  int row_height = 0;
  int content_left = 0;
  int content_right = 0;
  int candidate_top = 0;
  int expanded_columns = 0;
  RECT emoji_rect{};
  RECT previous_page_rect{};
  RECT next_page_rect{};
  RECT expand_rect{};
  RECT emoji_icon_rect{};
  RECT previous_page_icon_rect{};
  RECT next_page_icon_rect{};
  RECT expand_icon_rect{};
  RECT settings_rect{};
  RECT brand_rect{};
  RECT brand_icon_rect{};
  RECT brand_text_rect{};
  RECT tool_separator_rect{};
  RECT emoji_separator_rect{};
  RECT expand_separator_rect{};
  RECT expanded_header_separator_rect{};
  RECT expanded_footer_separator_rect{};
  std::vector<RECT> expanded_horizontal_separator_rects;
  std::vector<RECT> expanded_vertical_separator_rects;
  std::vector<RECT> tool_row_separator_rects;
  std::vector<RECT> candidate_rects;
  std::vector<int> candidate_rows;
  std::vector<int> candidate_columns;
};

}  // namespace fp::tsf
