#include "tsf/toolbar_model.h"

#include "common/encoding.h"

#include <algorithm>

namespace fp::tsf {

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

}  // namespace fp::tsf
