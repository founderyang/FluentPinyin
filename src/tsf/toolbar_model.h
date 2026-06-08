#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace fp::tsf {

enum ToolbarItemId : int {
  kToolbarItemNone = 0,
  kToolbarItemDrag = 1,
  kToolbarItemInputMode = 2,
  kToolbarItemShape = 3,
  kToolbarItemPunctuation = 4,
  kToolbarItemCharset = 5,
  kToolbarItemEmoji = 6,
  kToolbarItemSettings = 7,
};

struct ToolbarItemDefinition {
  int id;
  std::wstring_view setting_id;
  std::wstring_view custom_label;
};

inline constexpr std::array<ToolbarItemDefinition, 5> kToolbarCustomItemDefinitions{{
    {kToolbarItemInputMode, L"input_mode", L"\u4E2D/\u82F1\u6587"},
    {kToolbarItemShape, L"shape", L"\u5168/\u534A\u89D2"},
    {kToolbarItemPunctuation, L"punctuation", L"\u4E2D/\u82F1\u6587\u6807\u70B9"},
    {kToolbarItemCharset, L"charset", L"\u7B80\u4F53/\u7E41\u4F53\u4E2D\u6587\u5B57\u7B26"},
    {kToolbarItemEmoji, L"emoji", L"\u8868\u60C5\u7B26\u53F7/\u7B26\u53F7"},
}};

bool IsToolbarCustomItem(int item);
std::wstring_view ToolbarItemSettingId(int item);
std::wstring ToolbarCustomItemLabel(int item);
std::vector<int> DefaultToolbarVisibleItems();
bool ContainsToolbarItem(const std::vector<int>& items, int item);
std::vector<int> ParseToolbarVisibleItems(std::wstring_view value);
std::wstring SerializeToolbarVisibleItems(const std::vector<int>& items);
std::vector<int> VisibleToolbarItemsWithSettings(const std::vector<int>& items);

}  // namespace fp::tsf
