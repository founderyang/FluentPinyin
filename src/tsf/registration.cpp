#include "tsf/module.h"

#include "common/constants.h"
#include "common/logging.h"
#include "tsf/guids.h"

#include <msctf.h>
#include <windows.h>

#include <cwchar>
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

HRESULT RegisterSubstituteKeyboardLayout() {
  HRESULT result = SetRegistryString(
      HKEY_LOCAL_MACHINE, SubstituteKeyboardLayoutKey(), L"Layout Text", kProfileDescription);
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
  std::wstring module_path(32768, L'\0');
  const DWORD length = GetModuleFileNameW(
      g_module_instance, module_path.data(), static_cast<DWORD>(module_path.size()));
  if (length == 0) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  if (length >= module_path.size()) {
    return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
  }
  module_path.resize(length);

  const std::wstring clsid = GuidToString(kTextServiceClsid);
  const std::wstring clsid_key = L"Software\\Classes\\CLSID\\" + clsid;
  const std::wstring server_key = clsid_key + L"\\InprocServer32";

  HRESULT result =
      SetRegistryString(HKEY_CURRENT_USER,
                        clsid_key,
                        nullptr,
                        std::wstring(fp::kProductName) + L" Text Service");
  if (FAILED(result)) {
    return result;
  }

  result = SetRegistryString(HKEY_CURRENT_USER, server_key, nullptr, module_path);
  if (FAILED(result)) {
    return result;
  }

  return SetRegistryString(HKEY_CURRENT_USER, server_key, L"ThreadingModel", L"Apartment");
}

HRESULT UnregisterComServer() {
  const std::wstring clsid = GuidToString(kTextServiceClsid);
  return DeleteRegistryTree(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" + clsid);
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

  result = profiles->Register(kTextServiceClsid);
  if (SUCCEEDED(result) || result == E_FAIL) {
    result = profiles->AddLanguageProfile(kTextServiceClsid,
                                          kLanguageId,
                                          kProfileGuid,
                                          kProfileDescription,
                                          static_cast<ULONG>(wcslen(kProfileDescription)),
                                          nullptr,
                                          0,
                                          0);
  }

  if (SUCCEEDED(result)) {
    profiles->SubstituteKeyboardLayout(
        kTextServiceClsid, kLanguageId, kProfileGuid, SubstituteKeyboardLayout());

    const HRESULT enable_result =
        profiles->EnableLanguageProfile(kTextServiceClsid, kLanguageId, kProfileGuid, TRUE);
    if (FAILED(enable_result)) {
      fp::LogWarning(L"tsf", L"EnableLanguageProfile failed; continuing registration.");
    }

  }

  profiles->Release();
  if (FAILED(result)) {
    return result;
  }

  ITfCategoryMgr* category_mgr = nullptr;
  result = CoCreateInstance(CLSID_TF_CategoryMgr,
                            nullptr,
                            CLSCTX_INPROC_SERVER,
                            IID_ITfCategoryMgr,
                            reinterpret_cast<void**>(&category_mgr));
  if (FAILED(result)) {
    return result;
  }

  result = category_mgr->RegisterCategory(kTextServiceClsid,
                                          GUID_TFCAT_TIP_KEYBOARD,
                                          kTextServiceClsid);
  category_mgr->Release();
  if (FAILED(result)) {
    return result;
  }

  SetRegistryString(
      HKEY_LOCAL_MACHINE, TipLanguageProfileKey(), L"Display Description", kProfileDescription);
  SetRegistryString(
      HKEY_CURRENT_USER, TipLanguageProfileKey(), L"Display Description", kProfileDescription);
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
    category_mgr->UnregisterCategory(kTextServiceClsid,
                                     GUID_TFCAT_TIP_KEYBOARD,
                                     kTextServiceClsid);
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
