#include "common/constants.h"
#include "tsf/guids.h"
#include "common/broadcast_messages.h"
#include "common/encoding.h"
#include "common/logging.h"
#include "common/path_utils.h"
#include "devtools/app_runtime_utils.h"
#include "devtools/font_cleanup.h"
#include "devtools/process_utils.h"
#include "devtools/registry_utils.h"
#include "devtools/rime_warmup.h"
#include "devtools/scheduled_task_utils.h"

#include <ctffunc.h>
#include <msctf.h>
#include <shellapi.h>
#include <windows.h>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

std::wstring GuidToString(REFGUID guid) {
  wchar_t buffer[64]{};
  StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
  return buffer;
}

std::wstring HresultToString(HRESULT result) {
  wchar_t buffer[16]{};
  swprintf_s(buffer, L"0x%08lX", static_cast<unsigned long>(result));
  return buffer;
}

std::wstring ErrorCodeToString(DWORD error) {
  wchar_t buffer[16]{};
  swprintf_s(buffer, L"0x%08lX", static_cast<unsigned long>(error));
  return buffer;
}


template <typename T>
class ComPtr {
 public:
  ComPtr() = default;
  ComPtr(const ComPtr&) = delete;
  ComPtr& operator=(const ComPtr&) = delete;
  ~ComPtr() { Reset(); }

  T** put() {
    Reset();
    return &ptr_;
  }

  T* get() const { return ptr_; }
  T* operator->() const { return ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

  void Reset() {
    if (ptr_ != nullptr) {
      ptr_->Release();
      ptr_ = nullptr;
    }
  }

 private:
  T* ptr_ = nullptr;
};

class ComRuntime {
 public:
  ComRuntime() : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
  ~ComRuntime() {
    if (SUCCEEDED(result_)) {
      CoUninitialize();
    }
  }

  HRESULT result() const { return result_; }

 private:
  HRESULT result_;
};

HRESULT CreateProfileManager(ComPtr<ITfInputProcessorProfileMgr>& manager) {
  return CoCreateInstance(CLSID_TF_InputProcessorProfiles,
                          nullptr,
                          CLSCTX_INPROC_SERVER,
                          IID_ITfInputProcessorProfileMgr,
                          reinterpret_cast<void**>(manager.put()));
}

bool IsFpProfile(const TF_INPUTPROCESSORPROFILE& profile) {
  return IsEqualCLSID(profile.clsid, fp::tsf::kTextServiceClsid) &&
         IsEqualGUID(profile.guidProfile, fp::tsf::kProfileGuid);
}

inline constexpr CLSID kMicrosoftPinyinClsid = {
    0x81D4E9C9,
    0x1D3B,
    0x41BC,
    {0x9E, 0x6C, 0x4B, 0x40, 0xBF, 0x79, 0xE3, 0x5E}};

inline constexpr GUID kMicrosoftPinyinProfileGuid = {
    0xFA550B04,
    0x5AD7,
    0x411F,
    {0xA5, 0xAC, 0xCA, 0x03, 0x8E, 0xC5, 0x15, 0xD7}};

bool IsMicrosoftPinyinProfile(const TF_INPUTPROCESSORPROFILE& profile) {
  return IsEqualCLSID(profile.clsid, kMicrosoftPinyinClsid) &&
         IsEqualGUID(profile.guidProfile, kMicrosoftPinyinProfileGuid);
}

std::wstring FluentPinyinTipProfileKey() {
  return L"Software\\Microsoft\\CTF\\TIP\\" + GuidToString(fp::tsf::kTextServiceClsid) +
         L"\\LanguageProfile\\0x00000804\\" + GuidToString(fp::tsf::kProfileGuid);
}

std::wstring FluentPinyinUserLanguageProfileKey() {
  return L"Control Panel\\International\\User Profile\\zh-Hans-CN";
}

std::wstring FluentPinyinUserProfileKey() {
  return L"Control Panel\\International\\User Profile";
}

std::wstring FluentPinyinUserLanguageProfileValueName() {
  return L"0804:" + GuidToString(fp::tsf::kTextServiceClsid) +
         GuidToString(fp::tsf::kProfileGuid);
}

bool RegistryDwordEquals(HKEY root,
                         std::wstring_view subkey,
                         std::wstring_view name,
                         DWORD expected) {
  DWORD value = 0;
  DWORD size = sizeof(value);
  const LSTATUS status = RegGetValueW(root,
                                      std::wstring(subkey).c_str(),
                                      std::wstring(name).c_str(),
                                      RRF_RT_REG_DWORD,
                                      nullptr,
                                      &value,
                                      &size);
  return status == ERROR_SUCCESS && value == expected;
}

bool RegistryStringEquals(HKEY root,
                          std::wstring_view subkey,
                          std::wstring_view name,
                          std::wstring_view expected) {
  std::wstring value(32768, L'\0');
  DWORD size = static_cast<DWORD>(value.size() * sizeof(wchar_t));
  DWORD type = 0;
  const LSTATUS status = RegGetValueW(root,
                                      std::wstring(subkey).c_str(),
                                      std::wstring(name).c_str(),
                                      RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                                      &type,
                                      value.data(),
                                      &size);
  if (status != ERROR_SUCCESS || size == 0) {
    return false;
  }
  value.resize((size / sizeof(wchar_t)) - 1);
  return _wcsicmp(value.c_str(), std::wstring(expected).c_str()) == 0;
}

bool SetRegistryDword(HKEY root,
                      std::wstring_view subkey,
                      std::wstring_view name,
                      DWORD value) {
  HKEY key = nullptr;
  const LSTATUS open_status = RegCreateKeyExW(root,
                                              std::wstring(subkey).c_str(),
                                              0,
                                              nullptr,
                                              REG_OPTION_NON_VOLATILE,
                                              KEY_WRITE,
                                              nullptr,
                                              &key,
                                              nullptr);
  if (open_status != ERROR_SUCCESS) {
    return false;
  }
  const LSTATUS write_status = RegSetValueExW(key,
                                              std::wstring(name).c_str(),
                                              0,
                                              REG_DWORD,
                                              reinterpret_cast<const BYTE*>(&value),
                                              sizeof(value));
  RegCloseKey(key);
  return write_status == ERROR_SUCCESS;
}

bool SetRegistryString(HKEY root,
                       std::wstring_view subkey,
                       std::wstring_view name,
                       std::wstring_view value) {
  HKEY key = nullptr;
  const LSTATUS open_status = RegCreateKeyExW(root,
                                              std::wstring(subkey).c_str(),
                                              0,
                                              nullptr,
                                              REG_OPTION_NON_VOLATILE,
                                              KEY_WRITE,
                                              nullptr,
                                              &key,
                                              nullptr);
  if (open_status != ERROR_SUCCESS) {
    return false;
  }
  const std::wstring value_text(value);
  const LSTATUS write_status =
      RegSetValueExW(key,
                     std::wstring(name).c_str(),
                     0,
                     REG_SZ,
                     reinterpret_cast<const BYTE*>(value_text.c_str()),
                     static_cast<DWORD>((value_text.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
  return write_status == ERROR_SUCCESS;
}

bool EnsureCurrentUserFluentPinyinLanguageProfile() {
  bool ok = true;
  ok = SetRegistryDword(HKEY_CURRENT_USER,
                        FluentPinyinUserLanguageProfileKey(),
                        FluentPinyinUserLanguageProfileValueName(),
                        1) &&
       ok;
  ok = SetRegistryString(HKEY_CURRENT_USER,
                         FluentPinyinUserProfileKey(),
                         L"InputMethodOverride",
                         FluentPinyinUserLanguageProfileValueName()) &&
       ok;
  ok = SetRegistryString(HKEY_CURRENT_USER, L"Keyboard Layout\\Preload", L"1", L"e0200804") &&
       ok;
  SendNotifyMessageW(HWND_BROADCAST,
                     WM_SETTINGCHANGE,
                     0,
                     reinterpret_cast<LPARAM>(L"Control Panel\\International\\User Profile"));
  SendNotifyMessageW(HWND_BROADCAST,
                     WM_SETTINGCHANGE,
                     0,
                     reinterpret_cast<LPARAM>(L"Keyboard Layout"));
  return ok;
}

bool HasOptionPrefix(std::wstring_view arg) {
  return arg.starts_with(L"--") || arg.starts_with(L"/");
}

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

int ListProfiles() {
  ComPtr<ITfInputProcessorProfileMgr> manager;
  HRESULT result = CreateProfileManager(manager);
  if (FAILED(result)) {
    std::wcerr << L"CreateProfileManager failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  ComPtr<IEnumTfInputProcessorProfiles> enum_profiles;
  result = manager->EnumProfiles(fp::tsf::kLanguageId, enum_profiles.put());
  if (FAILED(result)) {
    std::wcerr << L"EnumProfiles failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  bool found = false;
  ULONG fetched = 0;
  TF_INPUTPROCESSORPROFILE profile{};
  while (enum_profiles->Next(1, &profile, &fetched) == S_OK && fetched == 1) {
    if (!IsFpProfile(profile)) {
      continue;
    }

    found = true;
    std::cout << "FluentPinyin profile found\n";
    std::cout << "  clsid:       " << fp::WideToUtf8(GuidToString(profile.clsid)) << "\n";
    std::cout << "  profile:     " << fp::WideToUtf8(GuidToString(profile.guidProfile)) << "\n";
    std::cout << "  langid:      0x" << std::hex << std::setw(4) << std::setfill('0')
              << profile.langid << std::dec << "\n";
    std::cout << "  type:        " << profile.dwProfileType << "\n";
    std::cout << "  substitute:  0x" << std::hex
              << reinterpret_cast<ULONG_PTR>(profile.hklSubstitute) << std::dec << "\n";
    std::cout << "  hkl:         0x" << std::hex << reinterpret_cast<ULONG_PTR>(profile.hkl)
              << std::dec << "\n";
    std::cout << "  flags:       0x" << std::hex << profile.dwFlags << std::dec << "\n";
    std::cout << "  enabled:     "
              << ((profile.dwFlags & TF_IPP_FLAG_ENABLED) ? "yes" : "no") << "\n";
    std::cout << "  active:      "
              << ((profile.dwFlags & TF_IPP_FLAG_ACTIVE) ? "yes" : "no") << "\n";
  }

  if (!found) {
    std::wcerr << fp::kProductName << L" profile was not returned by TSF EnumProfiles.\n";
    return 2;
  }

  return 0;
}

int ActivateProfile(DWORD scope_flags) {
  ComPtr<ITfInputProcessorProfileMgr> manager;
  HRESULT result = CreateProfileManager(manager);
  if (FAILED(result)) {
    std::wcerr << L"CreateProfileManager failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  result = manager->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,
                                    fp::tsf::kLanguageId,
                                    fp::tsf::kTextServiceClsid,
                                    fp::tsf::kProfileGuid,
                                    nullptr,
                                    scope_flags | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
  if (FAILED(result)) {
    std::wcerr << L"ActivateProfile failed: " << HresultToString(result) << L"\n";
    return 1;
  }
  if (!EnsureCurrentUserFluentPinyinLanguageProfile()) {
    std::wcerr << L"Failed to refresh current-user FluentPinyin language profile.\n";
    return 1;
  }

  std::cout << "FluentPinyin profile activation requested";
  if ((scope_flags & TF_IPPMF_FORSESSION) != 0) {
    std::cout << " for session";
  } else if ((scope_flags & TF_IPPMF_FORPROCESS) != 0) {
    std::cout << " for process";
  }
  std::cout << ".\n";
  return 0;
}

int ActivateMicrosoftPinyinProfile(DWORD scope_flags) {
  ComPtr<ITfInputProcessorProfileMgr> manager;
  HRESULT result = CreateProfileManager(manager);
  if (FAILED(result)) {
    std::wcerr << L"CreateProfileManager failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  result = manager->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,
                                    fp::tsf::kLanguageId,
                                    kMicrosoftPinyinClsid,
                                    kMicrosoftPinyinProfileGuid,
                                    nullptr,
                                    scope_flags | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
  if (FAILED(result)) {
    std::wcerr << L"Activate fallback input profile failed: " << HresultToString(result)
               << L"\n";
    return 1;
  }

  std::cout << "Fallback input profile activation requested";
  if ((scope_flags & TF_IPPMF_FORSESSION) != 0) {
    std::cout << " for session";
  } else if ((scope_flags & TF_IPPMF_FORPROCESS) != 0) {
    std::cout << " for process";
  }
  std::cout << ".\n";
  return 0;
}

int ActiveProfile() {
  ComPtr<ITfInputProcessorProfileMgr> manager;
  HRESULT result = CreateProfileManager(manager);
  if (FAILED(result)) {
    std::wcerr << L"CreateProfileManager failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  TF_INPUTPROCESSORPROFILE profile{};
  result = manager->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &profile);
  if (FAILED(result)) {
    std::wcerr << L"GetActiveProfile failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  std::cout << "Active keyboard TIP\n";
  std::cout << "  clsid:   " << fp::WideToUtf8(GuidToString(profile.clsid)) << "\n";
  std::cout << "  profile: " << fp::WideToUtf8(GuidToString(profile.guidProfile)) << "\n";
  std::cout << "  langid:  0x" << std::hex << std::setw(4) << std::setfill('0')
            << profile.langid << std::dec << "\n";
  const char* product = "other";
  if (IsFpProfile(profile)) {
    product = "FluentPinyin";
  } else if (IsMicrosoftPinyinProfile(profile)) {
    product = "fallback input";
  }
  std::cout << "  product: " << product << "\n";
  return IsFpProfile(profile) || IsMicrosoftPinyinProfile(profile) ? 0 : 2;
}

int SendRegisteredBroadcastCommand(std::wstring_view message_name, const char* label) {
  if (fp::RegisteredBroadcastMessage(message_name) == 0) {
    std::wcerr << L"RegisterWindowMessage failed.\n";
    return 1;
  }

  fp::SendRegisteredBroadcastMessage(message_name, 3000);
  std::cout << label << " broadcast sent.\n";
  return 0;
}

int SmokeTestService() {
  ComPtr<ITfTextInputProcessor> service;
  HRESULT result = CoCreateInstance(fp::tsf::kTextServiceClsid,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_ITfTextInputProcessor,
                                    reinterpret_cast<void**>(service.put()));
  if (FAILED(result)) {
    std::wcerr << L"CoCreateInstance(" << fp::kProductName
               << L") failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  ComPtr<ITfThreadMgr> thread_mgr;
  result = CoCreateInstance(CLSID_TF_ThreadMgr,
                            nullptr,
                            CLSCTX_INPROC_SERVER,
                            IID_ITfThreadMgr,
                            reinterpret_cast<void**>(thread_mgr.put()));
  if (FAILED(result)) {
    std::wcerr << L"CoCreateInstance(CLSID_TF_ThreadMgr) failed: " << HresultToString(result)
               << L"\n";
    return 1;
  }

  TfClientId client_id = 0;
  result = thread_mgr->Activate(&client_id);
  if (FAILED(result)) {
    std::wcerr << L"ITfThreadMgr::Activate failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  result = service->Activate(thread_mgr.get(), client_id);
  if (FAILED(result)) {
    std::wcerr << fp::kProductName << L" Activate failed: " << HresultToString(result)
               << L"\n";
    thread_mgr->Deactivate();
    return 1;
  }

  result = service->Deactivate();
  const HRESULT deactivate_thread_result = thread_mgr->Deactivate();
  if (FAILED(result)) {
    std::wcerr << fp::kProductName << L" Deactivate failed: " << HresultToString(result)
               << L"\n";
    return 1;
  }
  if (FAILED(deactivate_thread_result)) {
    std::wcerr << L"ITfThreadMgr::Deactivate failed: " << HresultToString(deactivate_thread_result)
               << L"\n";
    return 1;
  }

  std::cout << "FluentPinyin service smoke test passed.\n";
  return 0;
}

int InspectLangBar() {
  ComPtr<ITfThreadMgr> thread_mgr;
  HRESULT result = CoCreateInstance(CLSID_TF_ThreadMgr,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_ITfThreadMgr,
                                    reinterpret_cast<void**>(thread_mgr.put()));
  if (FAILED(result)) {
    std::wcerr << L"CoCreateInstance(CLSID_TF_ThreadMgr) failed: " << HresultToString(result)
               << L"\n";
    return 1;
  }

  TfClientId client_id = 0;
  result = thread_mgr->Activate(&client_id);
  if (FAILED(result)) {
    std::wcerr << L"ITfThreadMgr::Activate failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  ComPtr<ITfTextInputProcessor> service;
  result = CoCreateInstance(fp::tsf::kTextServiceClsid,
                            nullptr,
                            CLSCTX_INPROC_SERVER,
                            IID_ITfTextInputProcessor,
                            reinterpret_cast<void**>(service.put()));
  if (FAILED(result)) {
    std::wcerr << fp::kProductName << L" CoCreateInstance failed: "
               << HresultToString(result) << L"\n";
    thread_mgr->Deactivate();
    return 1;
  }

  result = service->Activate(thread_mgr.get(), client_id);
  if (FAILED(result)) {
    std::wcerr << fp::kProductName << L" Activate failed: " << HresultToString(result)
               << L"\n";
    thread_mgr->Deactivate();
    return 1;
  }

  ComPtr<ITfLangBarItemMgr> manager;
  result = thread_mgr->QueryInterface(IID_ITfLangBarItemMgr,
                                      reinterpret_cast<void**>(manager.put()));
  if (FAILED(result)) {
    std::wcerr << L"ITfLangBarItemMgr unavailable: " << HresultToString(result) << L"\n";
    service->Deactivate();
    thread_mgr->Deactivate();
    return 1;
  }

  ULONG count = 0;
  result = manager->GetItemNum(&count);
  if (FAILED(result)) {
    std::wcerr << L"GetItemNum failed: " << HresultToString(result) << L"\n";
  } else {
    std::cout << "LangBar items: " << count << "\n";
  }

  auto inspect_item = [&](REFGUID guid,
                          const char* label,
                          std::wstring_view expected_description,
                          std::initializer_list<std::wstring_view> expected_texts,
                          bool expect_text_color_icon) {
    bool ok = true;
    ComPtr<ITfLangBarItem> item;
    const HRESULT get_result = manager->GetItem(guid, item.put());
    std::cout << "GetItem(" << label << "): "
              << fp::WideToUtf8(HresultToString(get_result)) << "\n";
    if (FAILED(get_result) || !item) {
      return false;
    }

    TF_LANGBARITEMINFO info{};
    DWORD status = 0;
    if (SUCCEEDED(item->GetInfo(&info))) {
      std::cout << "  clsid: " << fp::WideToUtf8(GuidToString(info.clsidService)) << "\n";
      std::cout << "  desc:  " << fp::WideToUtf8(info.szDescription) << "\n";
      std::cout << "  style: 0x" << std::hex << info.dwStyle << std::dec << "\n";
      ok = ok && IsEqualGUID(info.guidItem, guid) &&
           IsEqualCLSID(info.clsidService, fp::tsf::kTextServiceClsid) &&
           std::wstring_view(info.szDescription) == expected_description &&
           (info.dwStyle & TF_LBI_STYLE_SHOWNINTRAY) != 0 &&
           (info.dwStyle & TF_LBI_STYLE_BTN_BUTTON) != 0;
      const bool text_color_icon = (info.dwStyle & TF_LBI_STYLE_TEXTCOLORICON) != 0;
      ok = ok && text_color_icon == expect_text_color_icon;
    } else {
      ok = false;
    }
    if (SUCCEEDED(item->GetStatus(&status))) {
      std::cout << "  status: 0x" << std::hex << status << std::dec << "\n";
    } else {
      ok = false;
    }

    ITfLangBarItemButton* button = nullptr;
    result = item->QueryInterface(IID_ITfLangBarItemButton,
                                  reinterpret_cast<void**>(&button));
    if (FAILED(result) || button == nullptr) {
      std::cout << "  button: unavailable\n";
      return false;
    }
    BSTR text = nullptr;
    result = button->GetText(&text);
    if (SUCCEEDED(result) && text != nullptr) {
      const std::wstring value(text, SysStringLen(text));
      std::cout << "  text:  " << fp::WideToUtf8(value) << "\n";
      bool text_ok = false;
      for (std::wstring_view expected : expected_texts) {
        text_ok = text_ok || value == expected;
      }
      ok = ok && text_ok;
      SysFreeString(text);
    } else {
      ok = false;
    }
    HICON icon = nullptr;
    result = button->GetIcon(&icon);
    std::cout << "  icon:  " << fp::WideToUtf8(HresultToString(result)) << "\n";
    ok = ok && SUCCEEDED(result) && icon != nullptr;
    button->Release();
    return ok;
  };

  const bool brand_ok = inspect_item(fp::tsf::kBrandLangBarItemGuid,
                                     "kBrandLangBarItemGuid",
                                     L"\u6D41\u7545\u62FC\u97F3",
                                     {L"\u7545"},
                                     false);
  const bool input_mode_ok = inspect_item(fp::tsf::kInputModeLangBarItemGuid,
                                          "kInputModeLangBarItemGuid",
                                          L"\u4E2D\u82F1\u5207\u6362",
                                          {L"\u7545"},
                                          true);
  const bool user_profile_ok =
      RegistryDwordEquals(HKEY_CURRENT_USER,
                          FluentPinyinUserLanguageProfileKey(),
                          FluentPinyinUserLanguageProfileValueName(),
                          1) &&
      RegistryStringEquals(HKEY_CURRENT_USER,
                           FluentPinyinUserProfileKey(),
                           L"InputMethodOverride",
                           FluentPinyinUserLanguageProfileValueName()) &&
      RegistryStringEquals(HKEY_CURRENT_USER, L"Keyboard Layout\\Preload", L"1", L"e0200804");
  std::cout << "Current-user language profile entry: "
            << (user_profile_ok ? "ok" : "missing") << "\n";
  const bool layout_text_ok =
      RegistryStringEquals(HKEY_LOCAL_MACHINE,
                           L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\E0200804",
                           L"Layout Text",
                           fp::tsf::kLanguageListLabel) &&
      RegistryStringEquals(HKEY_LOCAL_MACHINE,
                           L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\E0200804",
                           L"Layout Display Name",
                           fp::tsf::kLanguageListLabel);
  std::cout << "Substitute keyboard layout label: "
            << (layout_text_ok ? "ok" : "wrong") << "\n";

  service->Deactivate();
  const HRESULT deactivate_thread_result = thread_mgr->Deactivate();
  if (FAILED(deactivate_thread_result)) {
    std::wcerr << L"ITfThreadMgr::Deactivate failed: " << HresultToString(deactivate_thread_result)
               << L"\n";
    return 1;
  }
  return brand_ok && input_mode_ok && user_profile_ok && layout_text_ok ? 0 : 2;
}

int ShowToolbarForVerification() {
  ComPtr<ITfThreadMgr> thread_mgr;
  HRESULT result = CoCreateInstance(CLSID_TF_ThreadMgr,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_ITfThreadMgr,
                                    reinterpret_cast<void**>(thread_mgr.put()));
  if (FAILED(result)) {
    std::wcerr << L"CoCreateInstance(CLSID_TF_ThreadMgr) failed: " << HresultToString(result)
               << L"\n";
    return 1;
  }

  TfClientId client_id = 0;
  result = thread_mgr->Activate(&client_id);
  if (FAILED(result)) {
    std::wcerr << L"ITfThreadMgr::Activate failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  ComPtr<ITfTextInputProcessor> service;
  result = CoCreateInstance(fp::tsf::kTextServiceClsid,
                            nullptr,
                            CLSCTX_INPROC_SERVER,
                            IID_ITfTextInputProcessor,
                            reinterpret_cast<void**>(service.put()));
  if (FAILED(result)) {
    std::wcerr << fp::kProductName << L" CoCreateInstance failed: "
               << HresultToString(result) << L"\n";
    thread_mgr->Deactivate();
    return 1;
  }

  result = service->Activate(thread_mgr.get(), client_id);
  if (FAILED(result)) {
    std::wcerr << fp::kProductName << L" Activate failed: " << HresultToString(result)
               << L"\n";
    thread_mgr->Deactivate();
    return 1;
  }

  std::cout << "Toolbar verification host active.\n";
  const ULONGLONG deadline = GetTickCount64() + 15000;
  while (GetTickCount64() < deadline) {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    Sleep(16);
  }

  service->Deactivate();
  const HRESULT deactivate_thread_result = thread_mgr->Deactivate();
  if (FAILED(deactivate_thread_result)) {
    std::wcerr << L"ITfThreadMgr::Deactivate failed: " << HresultToString(deactivate_thread_result)
               << L"\n";
    return 1;
  }
  return 0;
}

void PrintUsage() {
  std::cout
      << "Usage: fluent-pinyin-devtools <profiles|activate|activate-process|activate-session|activate-ms-pinyin-session|activate-ms-pinyin-process|active|shutdown-core|smoke|langbar|toolbar|prepare-install|ensure-winapp-runtime|finalize-install|start-rime-warmup|warmup-rime|cleanup-install|close-settings|restart-text-services>\n";
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  fp::UseSystemCurrentDirectory();
  if (argc < 2) {
    PrintUsage();
    return 1;
  }

  ComRuntime com;
  if (FAILED(com.result()) && com.result() != RPC_E_CHANGED_MODE) {
    std::wcerr << L"COM initialization failed: " << HresultToString(com.result()) << L"\n";
    return 1;
  }

  const std::wstring command = argv[1];
  if (command == L"profiles") {
    return ListProfiles();
  }
  if (command == L"activate") {
    return ActivateProfile(0);
  }
  if (command == L"activate-process") {
    return ActivateProfile(TF_IPPMF_FORPROCESS);
  }
  if (command == L"activate-session") {
    return ActivateProfile(TF_IPPMF_FORSESSION);
  }
  if (command == L"activate-ms-pinyin-session") {
    return ActivateMicrosoftPinyinProfile(TF_IPPMF_FORSESSION);
  }
  if (command == L"activate-ms-pinyin-process") {
    return ActivateMicrosoftPinyinProfile(TF_IPPMF_FORPROCESS);
  }
  if (command == L"active") {
    return ActiveProfile();
  }
  if (command == L"shutdown-core") {
    return SendRegisteredBroadcastCommand(fp::kShutdownInputCoreMessageName,
                                          "Input core shutdown");
  }
  if (command == L"smoke") {
    return SmokeTestService();
  }
  if (command == L"langbar") {
    return InspectLangBar();
  }
  if (command == L"toolbar") {
    return ShowToolbarForVerification();
  }
  if (command == L"prepare-install") {
    return PrepareInstall();
  }
  if (command == L"ensure-winapp-runtime") {
    return fp::devtools::EnsureWindowsAppRuntime();
  }
  if (command == L"finalize-install") {
    return FinalizeInstall();
  }
  if (command == L"start-rime-warmup") {
    return fp::devtools::StartRimeWarmupProcess();
  }
  if (command == L"warmup-rime") {
    return fp::devtools::WarmupRime();
  }
  if (command == L"close-settings") {
    return fp::devtools::CloseSettingsProcess();
  }
  if (command == L"restart-text-services") {
    return fp::devtools::RestartTextServicesProcess();
  }
  if (command == L"cleanup-install" || command == L"cleanup-uninstall") {
    std::filesystem::path install_dir = DefaultInstallDir();
    bool skip_install_dir = false;
    bool keep_user_data = false;
    bool restart_text_services = true;
    for (int index = 2; index < argc; ++index) {
      const std::wstring_view arg = argv[index];
      if (arg == L"--skip-install-dir" || arg == L"/skip-install-dir") {
        skip_install_dir = true;
      } else if (arg == L"--keep-user-data" || arg == L"/keep-user-data") {
        keep_user_data = true;
      } else if (arg == L"--no-restart-text-services" ||
                 arg == L"/no-restart-text-services") {
        restart_text_services = false;
      } else if (!HasOptionPrefix(arg)) {
        install_dir = arg;
      }
    }
    return CleanupInstall(install_dir, skip_install_dir, keep_user_data, restart_text_services);
  }

  PrintUsage();
  return 1;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  fp::UseSystemCurrentDirectory();
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr) {
    return wmain(0, nullptr);
  }
  const int result = wmain(argc, argv);
  LocalFree(argv);
  return result;
}
