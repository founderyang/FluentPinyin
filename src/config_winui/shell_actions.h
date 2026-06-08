#pragma once

#include <filesystem>
#include <string_view>

namespace fp::config_winui {

void OpenPath(const std::filesystem::path& path);
void OpenUrl(std::wstring_view url);
void RunTool(std::wstring_view exe_name, std::wstring_view parameters = L"");

}  // namespace fp::config_winui
