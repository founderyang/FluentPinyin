#pragma once

#include <windef.h>

#include <filesystem>
#include <optional>

namespace fp::config_winui {

std::optional<std::filesystem::path> PickRimeDictionaryFile(HWND owner);
std::optional<std::filesystem::path> PickSyncBackupFile(HWND owner);
std::optional<std::filesystem::path> PickSyncBackupSaveFile(HWND owner);

}  // namespace fp::config_winui
