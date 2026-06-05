#include "common/constants.h"
#include "tsf/guids.h"

#include <ctffunc.h>
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

std::string WideToUtf8(std::wstring_view value) {
  if (value.empty()) {
    return {};
  }

  const int required = WideCharToMultiByte(CP_UTF8,
                                           0,
                                           value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr);
  if (required <= 0) {
    return {};
  }

  std::string result(static_cast<size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8,
                      0,
                      value.data(),
                      static_cast<int>(value.size()),
                      result.data(),
                      required,
                      nullptr,
                      nullptr);
  return result;
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
    std::cout << "  clsid:       " << WideToUtf8(GuidToString(profile.clsid)) << "\n";
    std::cout << "  profile:     " << WideToUtf8(GuidToString(profile.guidProfile)) << "\n";
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
  std::cout << "  clsid:   " << WideToUtf8(GuidToString(profile.clsid)) << "\n";
  std::cout << "  profile: " << WideToUtf8(GuidToString(profile.guidProfile)) << "\n";
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

int BroadcastRegisteredMessage(std::wstring_view message_name, const char* label) {
  const UINT message = RegisterWindowMessageW(std::wstring(message_name).c_str());
  if (message == 0) {
    std::wcerr << L"RegisterWindowMessage failed.\n";
    return 1;
  }

  SendMessageTimeoutW(HWND_BROADCAST,
                      message,
                      0,
                      0,
                      SMTO_ABORTIFHUNG | SMTO_NORMAL,
                      3000,
                      nullptr);
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

  ComPtr<ITfLangBarItem> input_mode;
  result = manager->GetItem(fp::tsf::kInputModeLangBarItemGuid, input_mode.put());
  std::cout << "GetItem(kInputModeLangBarItemGuid): " << WideToUtf8(HresultToString(result))
            << "\n";
  if (SUCCEEDED(result) && input_mode) {
    TF_LANGBARITEMINFO info{};
    DWORD status = 0;
    if (SUCCEEDED(input_mode->GetInfo(&info))) {
      std::cout << "  clsid: " << WideToUtf8(GuidToString(info.clsidService)) << "\n";
      std::cout << "  desc:  " << WideToUtf8(info.szDescription) << "\n";
      std::cout << "  style: 0x" << std::hex << info.dwStyle << std::dec << "\n";
    }
    if (SUCCEEDED(input_mode->GetStatus(&status))) {
      std::cout << "  status: 0x" << std::hex << status << std::dec << "\n";
    }
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
      << "Usage: fluent-pinyin-devtools <profiles|activate|activate-process|activate-session|activate-ms-pinyin-session|activate-ms-pinyin-process|active|shutdown-core|smoke|langbar|toolbar>\n";
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
    return BroadcastRegisteredMessage(fp::kShutdownInputCoreMessageName, "Input core shutdown");
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

  PrintUsage();
  return 1;
}
