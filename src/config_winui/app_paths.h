#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace fp::config_winui {

std::filesystem::path RimeUserDataPath();
std::filesystem::path ModuleDirectory();
std::filesystem::path SiblingExe(std::wstring_view name);
std::wstring WindowIconPath();
void EnsureUiFontsLoaded();

}  // namespace fp::config_winui
