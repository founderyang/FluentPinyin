#include "common/constants.h"
#include "tsf/guids.h"

#include <msctf.h>
#include <windows.h>

#include <iomanip>
#include <iostream>
#include <string>

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
    std::wcout << fp::kProductName << L" profile found\n";
    std::wcout << L"  clsid:       " << GuidToString(profile.clsid) << L"\n";
    std::wcout << L"  profile:     " << GuidToString(profile.guidProfile) << L"\n";
    std::wcout << L"  langid:      0x" << std::hex << std::setw(4) << std::setfill(L'0')
               << profile.langid << std::dec << L"\n";
    std::wcout << L"  type:        " << profile.dwProfileType << L"\n";
    std::wcout << L"  substitute:  0x" << std::hex
               << reinterpret_cast<ULONG_PTR>(profile.hklSubstitute) << std::dec << L"\n";
    std::wcout << L"  hkl:         0x" << std::hex << reinterpret_cast<ULONG_PTR>(profile.hkl)
               << std::dec << L"\n";
    std::wcout << L"  flags:       0x" << std::hex << profile.dwFlags << std::dec << L"\n";
    std::wcout << L"  enabled:     "
               << ((profile.dwFlags & TF_IPP_FLAG_ENABLED) ? L"yes" : L"no") << L"\n";
    std::wcout << L"  active:      "
               << ((profile.dwFlags & TF_IPP_FLAG_ACTIVE) ? L"yes" : L"no") << L"\n";
  }

  if (!found) {
    std::wcerr << fp::kProductName << L" profile was not returned by TSF EnumProfiles.\n";
    return 2;
  }

  return 0;
}

int ActivateProfile() {
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
                                    TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
  if (FAILED(result)) {
    std::wcerr << L"ActivateProfile failed: " << HresultToString(result) << L"\n";
    return 1;
  }

  std::wcout << fp::kProductName << L" profile activation requested.\n";
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

  std::wcout << L"Active keyboard TIP\n";
  std::wcout << L"  clsid:   " << GuidToString(profile.clsid) << L"\n";
  std::wcout << L"  profile: " << GuidToString(profile.guidProfile) << L"\n";
  std::wcout << L"  langid:  0x" << std::hex << std::setw(4) << std::setfill(L'0')
             << profile.langid << std::dec << L"\n";
  std::wcout << L"  product: " << (IsFpProfile(profile) ? fp::kProductName : L"other")
             << L"\n";
  return IsFpProfile(profile) ? 0 : 2;
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

  std::wcout << fp::kProductName << L" service smoke test passed.\n";
  return 0;
}

void PrintUsage() {
  std::wcout << L"Usage: fp-devtools <profiles|activate|active|smoke>\n";
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
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
    return ActivateProfile();
  }
  if (command == L"active") {
    return ActiveProfile();
  }
  if (command == L"smoke") {
    return SmokeTestService();
  }

  PrintUsage();
  return 1;
}
