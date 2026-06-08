#pragma once

#include <filesystem>

namespace fp::devtools {

void LogDeferredDeleteSkipped(const std::filesystem::path& path);
bool RemoveFileNow(const std::filesystem::path& path);
void RemovePathTree(const std::filesystem::path& path);
bool IsSafeInstallDirectory(const std::filesystem::path& path);

}  // namespace fp::devtools
