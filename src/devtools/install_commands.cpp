#include "devtools/install_commands.h"

#include "common/logging.h"
#include "common/path_utils.h"
#include "devtools/cleanup_utils.h"
#include "devtools/font_cleanup.h"
#include "devtools/process_utils.h"
#include "devtools/registry_utils.h"
#include "devtools/scheduled_task_utils.h"

#include <windows.h>

#include <iostream>
#include <string>

namespace fp::devtools {
namespace {

std::filesystem::path EnvironmentPath(const wchar_t* name) {
  const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
  if (required == 0) {
    return {};
  }

  std::wstring value(required, L'\0');
  const DWORD written = GetEnvironmentVariableW(name, value.data(), required);
  if (written == 0 || written >= required) {
    return {};
  }
  value.resize(written);
  return std::filesystem::path(value);
}

}  // namespace

std::filesystem::path DefaultInstallDir() {
  auto program_files = EnvironmentPath(L"ProgramFiles");
  if (program_files.empty()) {
    program_files = L"C:\\Program Files";
  }
  return program_files / L"FluentPinyin";
}

int PrepareInstall() {
  const ULONGLONG start_tick = GetTickCount64();
  fp::devtools::CloseLegacyInputHosts();
  fp::devtools::RemoveStalePendingDeletes();
  const auto font_cleanup = fp::devtools::RemoveFontFilesAndRegistry();
  const ULONGLONG elapsed_ms = GetTickCount64() - start_tick;
  fp::LogInfo(L"installer",
              std::wstring(L"prepare-install completed in ") +
                  std::to_wstring(elapsed_ms) +
                  L" ms; font_registry_values_deleted=" +
                  std::to_wstring(font_cleanup.registry_values_deleted) +
                  L"; font_file_delete_requests=" +
                  std::to_wstring(font_cleanup.file_delete_requests) + L".");
  std::cout << "Install prepared in " << elapsed_ms << " ms.\n";
  return 0;
}

int FinalizeInstall() {
  const ULONGLONG start_tick = GetTickCount64();
  const auto install_dir = DefaultInstallDir();
  const auto icon_path = install_dir / L"fluent-pinyin.ico";
  const std::wstring display_icon = icon_path.wstring();
  const std::wstring install_location = install_dir.wstring();
  bool updated = false;

  for (const auto& subkey : fp::devtools::FindUninstallKeysByName(L"FluentPinyin")) {
    updated = fp::devtools::SetRegistryStringValue(
                  HKEY_LOCAL_MACHINE, subkey, L"DisplayIcon", display_icon) ||
              updated;
    updated = fp::devtools::SetRegistryStringValue(
                  HKEY_LOCAL_MACHINE, subkey, L"InstallLocation", install_location) ||
              updated;
  }

  const ULONGLONG elapsed_ms = GetTickCount64() - start_tick;
  fp::LogInfo(L"installer",
              std::wstring(L"finalize-install completed in ") + std::to_wstring(elapsed_ms) +
                  L" ms; updated=" + (updated ? std::wstring(L"yes") : std::wstring(L"no")) +
                  L".");
  std::cout << "Install finalized in " << elapsed_ms << " ms.\n";
  return updated ? 0 : 1;
}

int CleanupInstall(const std::filesystem::path& install_dir,
                   bool skip_install_dir,
                   bool keep_user_data,
                   bool restart_text_services) {
  const ULONGLONG start_tick = GetTickCount64();
  fp::LogInfo(L"installer",
              L"cleanup started: install_dir=" + install_dir.wstring() +
                  L"; skip_install_dir=" +
                  (skip_install_dir ? std::wstring(L"yes") : std::wstring(L"no")) +
                  L"; keep_user_data=" +
                  (keep_user_data ? std::wstring(L"yes") : std::wstring(L"no")) +
                  L"; restart_text_services=" +
                  (restart_text_services ? std::wstring(L"yes") : std::wstring(L"no")) + L".");
  fp::devtools::RemoveStalePendingDeletes();
  fp::devtools::CloseSettingsProcess();
  fp::devtools::CloseProcessesUsingDirectory(install_dir);
  if (restart_text_services) {
    fp::devtools::RestartTextServicesProcess();
  }
  const auto font_cleanup = fp::devtools::RemoveFontFilesAndRegistry();
  const bool scheduled_task_cleanup_ok = fp::devtools::RemoveScheduledTask();

  constexpr wchar_t kClsid[] = L"{76e3ad5b-1dd8-4584-b3cd-127df0239720}";
  constexpr wchar_t kProfile[] = L"{21e29f6d-32dc-4f6d-8477-9ed72313c625}";
  constexpr wchar_t kKeyboardLayout[] = L"E0200804";
  const std::wstring user_profile_value = std::wstring(L"0804:") + kClsid + kProfile;

  fp::devtools::DeleteRegistryTree(HKEY_LOCAL_MACHINE, L"Software\\FluentPinyin");
  fp::devtools::DeleteRegistryTree(
      HKEY_LOCAL_MACHINE,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FluentPinyin");
  fp::devtools::DeleteRegistryTree(HKEY_LOCAL_MACHINE,
                                   std::wstring(L"Software\\Microsoft\\CTF\\TIP\\") + kClsid);
  fp::devtools::DeleteRegistryTree(HKEY_CURRENT_USER,
                                   std::wstring(L"Software\\Microsoft\\CTF\\TIP\\") + kClsid);
  fp::devtools::DeleteRegistryTree(HKEY_CURRENT_USER,
                                   std::wstring(L"Software\\Classes\\CLSID\\") + kClsid);
  fp::devtools::DeleteRegistryTree(HKEY_LOCAL_MACHINE,
                                   std::wstring(L"Software\\Classes\\CLSID\\") + kClsid);
  fp::devtools::DeleteRegistryTree(HKEY_USERS,
                                   std::wstring(L"S-1-5-18\\Software\\Classes\\CLSID\\") +
                                       kClsid);
  fp::devtools::DeleteRegistryTree(HKEY_CLASSES_ROOT, std::wstring(L"CLSID\\") + kClsid);
  fp::devtools::DeleteRegistryTree(
      HKEY_LOCAL_MACHINE,
      std::wstring(L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\") + kKeyboardLayout);
  fp::devtools::DeleteRegistryValue(HKEY_CURRENT_USER,
                                    L"Control Panel\\International\\User Profile\\zh-Hans-CN",
                                    user_profile_value);

  const std::wstring normalized_install_dir = install_dir.wstring();
  fp::devtools::RemoveRegistryValuesMatching(
      HKEY_CURRENT_USER,
      L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache",
      {normalized_install_dir, L"FluentPinyin", L"fluent-pinyin"},
      {normalized_install_dir, L"FluentPinyin", L"fluent-pinyin"});
  fp::devtools::RemoveRegistryValuesMatching(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store",
      {normalized_install_dir, L"FluentPinyin", L"fluent-pinyin"},
      {normalized_install_dir, L"FluentPinyin", L"fluent-pinyin"});

  if (!keep_user_data) {
    fp::devtools::RemovePathTree(fp::GetRoamingAppDataPath() / L"FluentPinyin");
    fp::devtools::RemovePathTree(fp::GetLocalAppDataPath() / L"FluentPinyin");
    fp::devtools::RemovePathTree(fp::GetProgramDataPath() / L"FluentPinyin");
    const auto temp = EnvironmentPath(L"TEMP");
    if (!temp.empty()) {
      fp::devtools::RemovePathTree(temp / L"FluentPinyin-update");
    }
  }

  if (!skip_install_dir) {
    if (!fp::devtools::IsSafeInstallDirectory(install_dir)) {
      fp::LogError(L"installer",
                   L"cleanup refused unexpected install directory: " + install_dir.wstring());
      std::wcerr << L"Refusing to remove unexpected install directory: "
                 << install_dir.wstring() << L"\n";
      return 1;
    }
    fp::devtools::RemovePathTree(install_dir);
  }

  const ULONGLONG elapsed_ms = GetTickCount64() - start_tick;
  fp::LogInfo(L"installer",
              L"cleanup completed in " + std::to_wstring(elapsed_ms) +
                  L" ms; font_registry_values_deleted=" +
                  std::to_wstring(font_cleanup.registry_values_deleted) +
                  L"; font_file_delete_requests=" +
                  std::to_wstring(font_cleanup.file_delete_requests) +
                  L"; scheduled_task_cleanup=" +
                  (scheduled_task_cleanup_ok ? std::wstring(L"ok") : std::wstring(L"failed")) +
                  L".");
  std::cout << "Cleanup completed.\n";
  return 0;
}

}  // namespace fp::devtools
