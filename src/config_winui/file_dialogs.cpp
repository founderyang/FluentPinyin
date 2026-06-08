#include "config_winui/file_dialogs.h"

#include "common/path_utils.h"
#include "sync/sync_service.h"

#include <windows.h>
#include <commdlg.h>

#include <cwchar>
#include <string>

namespace fp::config_winui {

std::optional<std::filesystem::path> PickRimeDictionaryFile(HWND owner) {
  std::wstring file_name(32768, L'\0');
  OPENFILENAMEW open_file{};
  open_file.lStructSize = sizeof(open_file);
  open_file.hwndOwner = owner;
  open_file.lpstrFilter =
      L"词库 (*.dict.yaml)\0*.dict.yaml\0YAML 文件 (*.yaml)\0*.yaml\0所有文件 (*.*)\0*.*\0";
  open_file.lpstrFile = file_name.data();
  open_file.nMaxFile = static_cast<DWORD>(file_name.size());
  open_file.lpstrTitle = L"选择要导入的词库";
  open_file.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&open_file)) {
    return std::nullopt;
  }
  file_name.resize(std::wcslen(file_name.c_str()));
  return std::filesystem::path(file_name);
}

std::optional<std::filesystem::path> PickSyncBackupFile(HWND owner) {
  std::wstring file_name(32768, L'\0');
  OPENFILENAMEW open_file{};
  open_file.lStructSize = sizeof(open_file);
  open_file.hwndOwner = owner;
  open_file.lpstrFilter = L"流畅拼音同步包 (*.fpsync)\0*.fpsync\0所有文件 (*.*)\0*.*\0";
  open_file.lpstrFile = file_name.data();
  open_file.nMaxFile = static_cast<DWORD>(file_name.size());
  open_file.lpstrTitle = L"选择要恢复的备份包";
  open_file.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&open_file)) {
    return std::nullopt;
  }
  file_name.resize(std::wcslen(file_name.c_str()));
  return std::filesystem::path(file_name);
}

std::optional<std::filesystem::path> PickSyncBackupSaveFile(HWND owner) {
  fp::EnsureDirectory(fp::sync::DefaultBackupDirectory());
  std::wstring file_name =
      (fp::sync::DefaultBackupDirectory() / fp::sync::NewTimestampedBackupName()).wstring();
  file_name.resize(32768, L'\0');
  OPENFILENAMEW save_file{};
  save_file.lStructSize = sizeof(save_file);
  save_file.hwndOwner = owner;
  save_file.lpstrFilter = L"流畅拼音同步包 (*.fpsync)\0*.fpsync\0所有文件 (*.*)\0*.*\0";
  save_file.lpstrFile = file_name.data();
  save_file.nMaxFile = static_cast<DWORD>(file_name.size());
  save_file.lpstrTitle = L"保存加密备份包";
  save_file.lpstrDefExt = L"fpsync";
  save_file.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
  if (!GetSaveFileNameW(&save_file)) {
    return std::nullopt;
  }
  file_name.resize(std::wcslen(file_name.c_str()));
  return std::filesystem::path(file_name);
}

}  // namespace fp::config_winui
