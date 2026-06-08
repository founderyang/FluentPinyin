#pragma once

#include <filesystem>
#include <string_view>
#include <windows.h>

namespace fp {

std::filesystem::path GetLocalAppDataPath();
std::filesystem::path GetRoamingAppDataPath();
std::filesystem::path GetProgramDataPath();
std::filesystem::path GetFpLocalDataPath();
std::filesystem::path GetFpRoamingDataPath();
std::filesystem::path GetFpLogDirectory();
std::filesystem::path GetModulePath(HMODULE module);
std::filesystem::path GetModuleDirectory(HMODULE module);
std::filesystem::path GetModuleExecutablePath();
std::filesystem::path GetModuleDirectory();
std::filesystem::path GetSiblingExecutablePath(std::wstring_view name);
std::filesystem::path GetSystemDirectoryPath();
void UseSystemCurrentDirectory();
bool EnsureDirectory(const std::filesystem::path& path);

}  // namespace fp
