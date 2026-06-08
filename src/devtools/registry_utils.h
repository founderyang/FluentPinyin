#pragma once

#include <windows.h>

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace fp::devtools {

bool SetRegistryStringValue(HKEY root,
                            std::wstring_view subkey,
                            std::wstring_view name,
                            std::wstring_view value);
bool DeleteRegistryValue(HKEY root, std::wstring_view subkey, std::wstring_view name);
void DeleteRegistryTree(HKEY root, std::wstring_view subkey);
std::vector<std::wstring> FindUninstallKeysByName(std::wstring_view display_name);
void RemoveRegistryValuesMatching(
    HKEY root,
    std::wstring_view subkey,
    std::initializer_list<std::wstring_view> name_needles,
    std::initializer_list<std::wstring_view> value_needles);
void RemoveStalePendingDeletes();

}  // namespace fp::devtools
