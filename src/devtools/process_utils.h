#pragma once

#include <windows.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace fp::devtools {

std::vector<DWORD> FindProcessIds(std::wstring_view process_name);
std::vector<DWORD> FindAllProcessIds();
std::vector<DWORD> FindProcessIdsLoadingModules(
    const std::vector<std::wstring_view>& module_names);
std::wstring ReadProcessCurrentDirectory(DWORD process_id);
std::wstring NormalizePathForCompare(const std::filesystem::path& path);
bool IsPathWithinDirectory(const std::filesystem::path& path,
                           const std::filesystem::path& directory);
void CloseProcessGracefully(DWORD process_id, DWORD timeout_ms);
void CloseProcessesUsingDirectory(const std::filesystem::path& directory);
int CloseLegacyInputHosts();
int CloseSettingsProcess();
int RestartTextServicesProcess();

}  // namespace fp::devtools
