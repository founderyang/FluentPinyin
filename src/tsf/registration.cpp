#include "tsf/module.h"

#include "common/constants.h"
#include "common/logging.h"
#include "common/path_utils.h"
#include "tsf/guids.h"

#include <msctf.h>
#include <windows.h>

#include <cwchar>
#include <filesystem>
#include <iterator>
#include <string>

namespace fp::tsf {
namespace {

std::wstring GuidToString(REFGUID guid) {
  wchar_t buffer[64]{};
  StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
  return buffer;
}

HRESULT SetRegistryString(HKEY root,
                          const std::wstring& subkey,
                          const wchar_t* name,
                          const std::wstring& value) {
  HKEY key = nullptr;
  LSTATUS status = RegCreateKeyExW(root,
                                   subkey.c_str(),
                                   0,
                                   nullptr,
                                   REG_OPTION_NON_VOLATILE,
                                   KEY_WRITE,
                                   nullptr,
                                   &key,
                                   nullptr);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }

  status = RegSetValueExW(key,
                          name,
                          0,
                          REG_SZ,
                          reinterpret_cast<const BYTE*>(value.c_str()),
                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
  return HRESULT_FROM_WIN32(status);
}

HRESULT DeleteRegistryTree(HKEY root, const std::wstring& subkey) {
  const LSTATUS status = RegDeleteTreeW(root, subkey.c_str());
  if (status == ERROR_FILE_NOT_FOUND) {
    return S_OK;
  }

  return HRESULT_FROM_WIN32(status);
}

std::wstring ModulePath() {
  return fp::GetModulePath(g_module_instance).wstring();
}

bool SystemUsesLightTheme() {
  DWORD value = 1;
  DWORD size = sizeof(value);
  const LSTATUS status =
      RegGetValueW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"SystemUsesLightTheme",
                   RRF_RT_REG_DWORD,
                   nullptr,
                   &value,
                   &size);
  return status != ERROR_SUCCESS || value != 0;
}

std::wstring ProfileIconPath(const std::wstring& module_path) {
  const auto module = std::filesystem::path(module_path);
  const auto icon = module.parent_path() / L"fluent-pinyin.ico";
  if (GetFileAttributesW(icon.c_str()) != INVALID_FILE_ATTRIBUTES) {
    return icon.wstring();
  }
  return module_path;
}

HRESULT SetRegistryDword(HKEY root,
                         const std::wstring& subkey,
                         const wchar_t* name,
                         DWORD value) {
  HKEY key = nullptr;
  LSTATUS status = RegCreateKeyExW(root,
                                   subkey.c_str(),
                                   0,
                                   nullptr,
                                   REG_OPTION_NON_VOLATILE,
                                   KEY_WRITE,
                                   nullptr,
                                   &key,
                                   nullptr);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }

  status = RegSetValueExW(
      key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
  RegCloseKey(key);
  return HRESULT_FROM_WIN32(status);
}

HRESULT SetRegistryExpandableString(HKEY root,
                                    const std::wstring& subkey,
                                    const wchar_t* name,
                                    const std::wstring& value) {
  HKEY key = nullptr;
  LSTATUS status = RegCreateKeyExW(root,
                                   subkey.c_str(),
                                   0,
                                   nullptr,
                                   REG_OPTION_NON_VOLATILE,
                                   KEY_WRITE,
                                   nullptr,
                                   &key,
                                   nullptr);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }

  status = RegSetValueExW(key,
                          name,
                          0,
                          REG_EXPAND_SZ,
                          reinterpret_cast<const BYTE*>(value.c_str()),
                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
  return HRESULT_FROM_WIN32(status);
}

std::wstring TipLanguageProfileKey() {
  return L"Software\\Microsoft\\CTF\\TIP\\" + GuidToString(kTextServiceClsid) +
         L"\\LanguageProfile\\0x00000804\\" + GuidToString(kProfileGuid);
}

std::wstring UserLanguageProfileKeyName() {
  return L"0804:" + GuidToString(kTextServiceClsid) + GuidToString(kProfileGuid);
}

std::wstring UserLanguageProfileKey() {
  return L"Control Panel\\International\\User Profile\\zh-Hans-CN";
}

std::wstring SubstituteKeyboardLayoutId() {
  return L"E0200804";
}

HKL SubstituteKeyboardLayout() {
  return reinterpret_cast<HKL>(static_cast<ULONG_PTR>(0xE0200804));
}

std::wstring SubstituteKeyboardLayoutKey() {
  return L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\" +
         SubstituteKeyboardLayoutId();
}

constexpr const GUID* kRegisteredCategories[] = {
    &GUID_TFCAT_TIP_KEYBOARD,
    &GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
    &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    &GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
    &GUID_TFCAT_DISPLAYATTRIBUTEPROPERTY,
};

HRESULT RegisterSubstituteKeyboardLayout() {
  HRESULT result = SetRegistryString(
      HKEY_LOCAL_MACHINE, SubstituteKeyboardLayoutKey(), L"Layout Text", kLanguageListLabel);
  if (FAILED(result)) {
    return result;
  }

  result = SetRegistryString(
      HKEY_LOCAL_MACHINE, SubstituteKeyboardLayoutKey(), L"Layout Display Name", kLanguageListLabel);
  if (FAILED(result)) {
    return result;
  }

  return SetRegistryString(
      HKEY_LOCAL_MACHINE, SubstituteKeyboardLayoutKey(), L"Layout File", L"KBDUS.DLL");
}

HRESULT UnregisterSubstituteKeyboardLayout() {
  return DeleteRegistryTree(HKEY_LOCAL_MACHINE, SubstituteKeyboardLayoutKey());
}

HRESULT RegisterComServer() {
  const std::wstring module_path = ModulePath();
  if (module_path.empty()) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  const std::wstring clsid = GuidToString(kTextServiceClsid);
  const std::wstring clsid_key = L"Software\\Classes\\CLSID\\" + clsid;
  const std::wstring server_key = clsid_key + L"\\InprocServer32";
  DeleteRegistryTree(HKEY_CURRENT_USER, clsid_key);

  HRESULT result =
      SetRegistryString(HKEY_LOCAL_MACHINE,
                        clsid_key,
                        nullptr,
                        std::wstring(fp::kProductName) + L" Text Service");
  if (FAILED(result)) {
    return result;
  }

  result = SetRegistryString(HKEY_LOCAL_MACHINE, server_key, nullptr, module_path);
  if (FAILED(result)) {
    return result;
  }

  return SetRegistryString(HKEY_LOCAL_MACHINE, server_key, L"ThreadingModel", L"Apartment");
}

HRESULT UnregisterComServer() {
  const std::wstring clsid = GuidToString(kTextServiceClsid);
  const std::wstring clsid_key = L"Software\\Classes\\CLSID\\" + clsid;
  HRESULT result = DeleteRegistryTree(HKEY_LOCAL_MACHINE, clsid_key);
  const HRESULT user_result = DeleteRegistryTree(HKEY_CURRENT_USER, clsid_key);
  if (FAILED(result)) {
    return result;
  }
  return user_result;
}

HRESULT RegisterTsfProfile() {
  ITfInputProcessorProfiles* profiles = nullptr;
  HRESULT result = CoCreateInstance(CLSID_TF_InputProcessorProfiles,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_ITfInputProcessorProfiles,
                                    reinterpret_cast<void**>(&profiles));
  if (FAILED(result)) {
    return result;
  }

  const std::wstring module_path = ModulePath();
  if (module_path.empty()) {
    profiles->Release();
    return HRESULT_FROM_WIN32(GetLastError());
  }
  const std::wstring profile_icon_path = ProfileIconPath(module_path);

  result = profiles->Register(kTextServiceClsid);
  if (FAILED(result) && result != E_FAIL) {
    profiles->Release();
    return result;
  }

  ITfInputProcessorProfileMgr* profile_mgr = nullptr;
  result = CoCreateInstance(CLSID_TF_InputProcessorProfiles,
                            nullptr,
                            CLSCTX_INPROC_SERVER,
                            IID_ITfInputProcessorProfileMgr,
                            reinterpret_cast<void**>(&profile_mgr));
  if (FAILED(result)) {
    profiles->Release();
    return result;
  }

  constexpr DWORD kProfileCaps = TF_IPP_CAPS_UIELEMENTENABLED |
                                 TF_IPP_CAPS_IMMERSIVESUPPORT |
                                 TF_IPP_CAPS_SYSTRAYSUPPORT;
  result = profile_mgr->RegisterProfile(kTextServiceClsid,
                                        kLanguageId,
                                        kProfileGuid,
                                        kProfileDescription,
                                        static_cast<ULONG>(wcslen(kProfileDescription)),
                                        profile_icon_path.c_str(),
                                        static_cast<ULONG>(profile_icon_path.size()),
                                        0,
                                        SubstituteKeyboardLayout(),
                                        0,
                                        TRUE,
                                        kProfileCaps);
  profile_mgr->Release();
  if (FAILED(result)) {
    profiles->Release();
    return result;
  }

  profiles->SubstituteKeyboardLayout(
      kTextServiceClsid, kLanguageId, kProfileGuid, SubstituteKeyboardLayout());

  const HRESULT enable_result =
      profiles->EnableLanguageProfile(kTextServiceClsid, kLanguageId, kProfileGuid, TRUE);
  if (FAILED(enable_result)) {
    fp::LogWarning(L"tsf", L"EnableLanguageProfile failed; continuing registration.");
  }
  profiles->EnableLanguageProfileByDefault(kTextServiceClsid, kLanguageId, kProfileGuid, TRUE);
  profiles->Release();

  ITfCategoryMgr* category_mgr = nullptr;
  result = CoCreateInstance(CLSID_TF_CategoryMgr,
                            nullptr,
                            CLSCTX_INPROC_SERVER,
                            IID_ITfCategoryMgr,
                            reinterpret_cast<void**>(&category_mgr));
  if (FAILED(result)) {
    return result;
  }

  for (const GUID* category : kRegisteredCategories) {
    result = category_mgr->RegisterCategory(kTextServiceClsid, *category, kTextServiceClsid);
    if (FAILED(result)) {
      break;
    }
  }
  category_mgr->Release();
  if (FAILED(result)) {
    return result;
  }

  SetRegistryString(
      HKEY_LOCAL_MACHINE, TipLanguageProfileKey(), L"Display Description", kProfileDescription);
  SetRegistryString(
      HKEY_CURRENT_USER, TipLanguageProfileKey(), L"Display Description", kProfileDescription);
  SetRegistryExpandableString(
      HKEY_LOCAL_MACHINE, TipLanguageProfileKey(), L"IconFile", profile_icon_path);
  SetRegistryDword(HKEY_LOCAL_MACHINE, TipLanguageProfileKey(), L"IconIndex", 0);
  SetRegistryExpandableString(
      HKEY_CURRENT_USER, TipLanguageProfileKey(), L"IconFile", profile_icon_path);
  SetRegistryDword(HKEY_CURRENT_USER, TipLanguageProfileKey(), L"IconIndex", 0);
  SetRegistryDword(
      HKEY_CURRENT_USER, UserLanguageProfileKey(), UserLanguageProfileKeyName().c_str(), 1);

  return S_OK;
}

HRESULT UnregisterTsfProfile() {
  ITfCategoryMgr* category_mgr = nullptr;
  HRESULT result = CoCreateInstance(CLSID_TF_CategoryMgr,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_ITfCategoryMgr,
                                    reinterpret_cast<void**>(&category_mgr));
  if (SUCCEEDED(result)) {
    for (const GUID* category : kRegisteredCategories) {
      category_mgr->UnregisterCategory(kTextServiceClsid, *category, kTextServiceClsid);
    }
    category_mgr->Release();
  }

  ITfInputProcessorProfiles* profiles = nullptr;
  result = CoCreateInstance(CLSID_TF_InputProcessorProfiles,
                            nullptr,
                            CLSCTX_INPROC_SERVER,
                            IID_ITfInputProcessorProfiles,
                            reinterpret_cast<void**>(&profiles));
  if (FAILED(result)) {
    return result;
  }

  profiles->EnableLanguageProfile(kTextServiceClsid, kLanguageId, kProfileGuid, FALSE);
  profiles->EnableLanguageProfileByDefault(kTextServiceClsid, kLanguageId, kProfileGuid, FALSE);
  profiles->RemoveLanguageProfile(kTextServiceClsid, kLanguageId, kProfileGuid);
  result = profiles->Unregister(kTextServiceClsid);
  profiles->Release();

  ITfInputProcessorProfileMgr* profile_mgr = nullptr;
  const HRESULT profile_mgr_result = CoCreateInstance(CLSID_TF_InputProcessorProfiles,
                                                      nullptr,
                                                      CLSCTX_INPROC_SERVER,
                                                      IID_ITfInputProcessorProfileMgr,
                                                      reinterpret_cast<void**>(&profile_mgr));
  if (SUCCEEDED(profile_mgr_result) && profile_mgr != nullptr) {
    profile_mgr->UnregisterProfile(kTextServiceClsid, kLanguageId, kProfileGuid, 0);
    profile_mgr->Release();
  }

  HKEY user_language_key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    UserLanguageProfileKey().c_str(),
                    0,
                    KEY_SET_VALUE,
                    &user_language_key) == ERROR_SUCCESS) {
    RegDeleteValueW(user_language_key, UserLanguageProfileKeyName().c_str());
    RegCloseKey(user_language_key);
  }

  return result;
}

class ScopedComInitialization {
 public:
  ScopedComInitialization() : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
  ~ScopedComInitialization() {
    if (SUCCEEDED(result_)) {
      CoUninitialize();
    }
  }

  [[nodiscard]] HRESULT result() const noexcept { return result_; }

 private:
  HRESULT result_;
};

}  // namespace

HRESULT RegisterServer() {
  fp::LogInfo(L"tsf", L"RegisterServer started.");

  HRESULT result = RegisterSubstituteKeyboardLayout();
  if (FAILED(result)) {
    fp::LogError(L"tsf", L"Substitute keyboard layout registration failed.");
    return result;
  }

  result = RegisterComServer();
  if (FAILED(result)) {
    fp::LogError(L"tsf", L"COM registration failed.");
    UnregisterSubstituteKeyboardLayout();
    return result;
  }

  ScopedComInitialization com;
  if (FAILED(com.result()) && com.result() != RPC_E_CHANGED_MODE) {
    return com.result();
  }

  result = RegisterTsfProfile();
  if (FAILED(result)) {
    fp::LogError(L"tsf", L"TSF profile registration failed.");
    UnregisterComServer();
    UnregisterSubstituteKeyboardLayout();
    return result;
  }

  fp::LogInfo(L"tsf", L"RegisterServer completed.");
  return S_OK;
}

HRESULT UnregisterServer() {
  fp::LogInfo(L"tsf", L"UnregisterServer started.");

  ScopedComInitialization com;
  if (SUCCEEDED(com.result()) || com.result() == RPC_E_CHANGED_MODE) {
    UnregisterTsfProfile();
  }

  const HRESULT result = UnregisterComServer();
  if (FAILED(result)) {
    fp::LogError(L"tsf", L"COM unregistration failed.");
    return result;
  }

  UnregisterSubstituteKeyboardLayout();

  fp::LogInfo(L"tsf", L"UnregisterServer completed.");
  return S_OK;
}

}  // namespace fp::tsf
