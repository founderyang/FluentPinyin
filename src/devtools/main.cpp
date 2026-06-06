#include "common/constants.h"
#include "tsf/guids.h"
#include "common/path_utils.h"

#include <ctffunc.h>
#include <msctf.h>
#include <taskschd.h>
#include <tlhelp32.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
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

struct FontEntry {
  std::wstring_view file;
  std::wstring_view name;
};

constexpr std::array kFontEntries{
    FontEntry{L"MiSans-Regular.ttf", L"MiSans"},
    FontEntry{L"MiSans-Medium.ttf", L"MiSans Medium"},
    FontEntry{L"MiSans-Semibold.ttf", L"MiSans Semibold"},
    FontEntry{L"MiSansTC-Regular.ttf", L"MiSans TC"},
    FontEntry{L"MiSansTC-Medium.ttf", L"MiSans TC Medium"},
    FontEntry{L"MiSansTC-Semibold.ttf", L"MiSans TC Semibold"},
    FontEntry{L"MiSansL3-Regular.ttf", L"MiSans L3"},
    FontEntry{L"SourceHanSansSC-Regular.otf", L"Source Han Sans SC"},
    FontEntry{L"SourceHanSansTC-Regular.otf", L"Source Han Sans TC"},
    FontEntry{L"PlangothicP1-Regular.ttf", L"Plangothic P1"},
    FontEntry{L"PlangothicP2-Regular.ttf", L"Plangothic P2"},
};

constexpr std::wstring_view kTaskName = L"FluentPinyinAutoSync";
constexpr std::wstring_view kInstallDirName = L"FluentPinyin";
constexpr wchar_t kSettingsProcessName[] = L"fluent-pinyin-settings.exe";

std::wstring ToLower(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  return value;
}

bool EqualsInsensitive(std::wstring_view left, std::wstring_view right) {
  return ToLower(std::wstring(left)) == ToLower(std::wstring(right));
}

bool ContainsInsensitive(std::wstring_view value, std::wstring_view needle) {
  if (needle.empty()) {
    return true;
  }
  return ToLower(std::wstring(value)).find(ToLower(std::wstring(needle))) !=
         std::wstring::npos;
}

bool HasOptionPrefix(std::wstring_view arg) {
  return arg.starts_with(L"--") || arg.starts_with(L"/");
}

std::filesystem::path ModuleDirectory() {
  std::wstring buffer(MAX_PATH, L'\0');
  DWORD length = 0;
  for (;;) {
    length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      return std::filesystem::current_path();
    }
    if (length < buffer.size() - 1) {
      break;
    }
    buffer.resize(buffer.size() * 2);
  }

  buffer.resize(length);
  return std::filesystem::path(buffer).parent_path();
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
  return program_files / std::wstring(kInstallDirName);
}

std::wstring FontRegistryName(const FontEntry& font, std::wstring_view kind) {
  std::wstring name(font.name);
  name.append(L" (");
  name.append(kind);
  name.push_back(L')');
  return name;
}

std::wstring FontRegistryKind(const FontEntry& font) {
  const auto lower = ToLower(std::wstring(font.file));
  return lower.ends_with(L".otf") ? L"OpenType" : L"TrueType";
}

void BroadcastFontChange() {
  SendMessageTimeoutW(HWND_BROADCAST,
                      WM_FONTCHANGE,
                      0,
                      0,
                      SMTO_ABORTIFHUNG | SMTO_NORMAL,
                      3000,
                      nullptr);
}

bool SetRegistryString(HKEY root,
                       const wchar_t* subkey,
                       const std::wstring& name,
                       const std::wstring& value) {
  HKEY key = nullptr;
  const LSTATUS status =
      RegCreateKeyExW(root, subkey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
  if (status != ERROR_SUCCESS) {
    return false;
  }
  const DWORD byte_size = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
  const LSTATUS set_status = RegSetValueExW(key,
                                            name.c_str(),
                                            0,
                                            REG_SZ,
                                            reinterpret_cast<const BYTE*>(value.c_str()),
                                            byte_size);
  RegCloseKey(key);
  return set_status == ERROR_SUCCESS;
}

void DeleteRegistryValue(HKEY root, const wchar_t* subkey, const std::wstring& name) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(root, subkey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
    return;
  }
  RegDeleteValueW(key, name.c_str());
  RegCloseKey(key);
}

void DeleteRegistryTree(HKEY root, const wchar_t* subkey) {
  RegDeleteTreeW(root, subkey);
}

std::wstring RegistryDataToString(DWORD type, const std::vector<BYTE>& data) {
  if (data.empty()) {
    return {};
  }

  if (type == REG_SZ || type == REG_EXPAND_SZ) {
    const auto* text = reinterpret_cast<const wchar_t*>(data.data());
    const size_t chars = data.size() / sizeof(wchar_t);
    return std::wstring(text, wcsnlen_s(text, chars));
  }
  if (type == REG_MULTI_SZ) {
    std::wstring result;
    const auto* text = reinterpret_cast<const wchar_t*>(data.data());
    const size_t chars = data.size() / sizeof(wchar_t);
    size_t index = 0;
    while (index < chars) {
      const size_t start = index;
      while (index < chars && text[index] != L'\0') {
        ++index;
      }
      if (start == index) {
        bool rest_is_empty = true;
        for (size_t rest = index; rest < chars; ++rest) {
          if (text[rest] != L'\0') {
            rest_is_empty = false;
            break;
          }
        }
        if (rest_is_empty) {
          break;
        }
      }
      if (!result.empty()) {
        result.push_back(L'\n');
      }
      result.append(text + start, index - start);
      ++index;
    }
    return result;
  }
  return {};
}

void RemoveRegistryValuesMatching(HKEY root,
                                  const wchar_t* subkey,
                                  std::initializer_list<std::wstring_view> name_needles,
                                  std::initializer_list<std::wstring_view> value_needles) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(root, subkey, 0, KEY_READ | KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
    return;
  }

  DWORD index = 0;
  std::vector<std::wstring> names_to_delete;
  for (;;) {
    DWORD name_chars = 512;
    std::wstring name(name_chars, L'\0');
    DWORD type = 0;
    DWORD data_bytes = 4096;
    std::vector<BYTE> data(data_bytes);
    LSTATUS status = RegEnumValueW(key,
                                   index,
                                   name.data(),
                                   &name_chars,
                                   nullptr,
                                   &type,
                                   data.data(),
                                   &data_bytes);
    if (status == ERROR_MORE_DATA) {
      name_chars = 32767;
      name.assign(name_chars, L'\0');
      data.assign(data_bytes, 0);
      status = RegEnumValueW(key,
                             index,
                             name.data(),
                             &name_chars,
                             nullptr,
                             &type,
                             data.data(),
                             &data_bytes);
    }
    if (status == ERROR_NO_MORE_ITEMS) {
      break;
    }
    if (status != ERROR_SUCCESS) {
      ++index;
      continue;
    }

    name.resize(name_chars);
    data.resize(data_bytes);
    const std::wstring value = RegistryDataToString(type, data);
    bool matched = false;
    for (const auto needle : name_needles) {
      if (!needle.empty() && ContainsInsensitive(name, needle)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      for (const auto needle : value_needles) {
        if (!needle.empty() && ContainsInsensitive(value, needle)) {
          matched = true;
          break;
        }
      }
    }
    if (matched) {
      names_to_delete.push_back(name);
    }
    ++index;
  }

  for (const auto& name : names_to_delete) {
    RegDeleteValueW(key, name.c_str());
  }
  RegCloseKey(key);
}

std::vector<std::wstring> ParseMultiStringWithEmptyItems(const std::vector<BYTE>& bytes) {
  std::vector<std::wstring> entries;
  if (bytes.empty()) {
    return entries;
  }

  const auto* text = reinterpret_cast<const wchar_t*>(bytes.data());
  const size_t chars = bytes.size() / sizeof(wchar_t);
  size_t index = 0;
  while (index < chars) {
    const size_t start = index;
    while (index < chars && text[index] != L'\0') {
      ++index;
    }

    if (start == index && entries.size() % 2 == 0) {
      bool rest_is_empty = true;
      for (size_t rest = index; rest < chars; ++rest) {
        if (text[rest] != L'\0') {
          rest_is_empty = false;
          break;
        }
      }
      if (rest_is_empty) {
        break;
      }
    }

    entries.emplace_back(text + start, index - start);
    ++index;
  }
  return entries;
}

void RemoveStalePendingDeletes() {
  constexpr std::array kNeedles{
      std::wstring_view{L"\\FluentPinyin"},
      std::wstring_view{L"MiSans-Regular.ttf"},
      std::wstring_view{L"MiSans-Medium.ttf"},
      std::wstring_view{L"MiSans-Semibold.ttf"},
      std::wstring_view{L"MiSansTC-Regular.ttf"},
      std::wstring_view{L"MiSansTC-Medium.ttf"},
      std::wstring_view{L"MiSansTC-Semibold.ttf"},
      std::wstring_view{L"MiSansL3-Regular.ttf"},
      std::wstring_view{L"SourceHanSansSC-Regular.otf"},
      std::wstring_view{L"SourceHanSansTC-Regular.otf"},
      std::wstring_view{L"PlangothicP1-Regular.ttf"},
      std::wstring_view{L"PlangothicP2-Regular.ttf"},
  };

  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SYSTEM\\CurrentControlSet\\Control\\Session Manager",
                    0,
                    KEY_QUERY_VALUE | KEY_SET_VALUE,
                    &key) != ERROR_SUCCESS) {
    return;
  }

  DWORD type = 0;
  DWORD bytes = 0;
  if (RegQueryValueExW(key, L"PendingFileRenameOperations", nullptr, &type, nullptr, &bytes) !=
          ERROR_SUCCESS ||
      type != REG_MULTI_SZ || bytes == 0) {
    RegCloseKey(key);
    return;
  }

  std::vector<BYTE> data(bytes);
  if (RegQueryValueExW(
          key, L"PendingFileRenameOperations", nullptr, &type, data.data(), &bytes) !=
      ERROR_SUCCESS) {
    RegCloseKey(key);
    return;
  }
  data.resize(bytes);

  const auto entries = ParseMultiStringWithEmptyItems(data);
  std::vector<std::wstring> filtered;
  bool changed = false;
  for (size_t index = 0; index < entries.size(); index += 2) {
    const auto& source = entries[index];
    const std::wstring target = index + 1 < entries.size() ? entries[index + 1] : L"";
    bool remove = false;
    for (const auto needle : kNeedles) {
      if (ContainsInsensitive(source, needle) || ContainsInsensitive(target, needle)) {
        remove = true;
        changed = true;
        break;
      }
    }
    if (!remove) {
      filtered.push_back(source);
      if (index + 1 < entries.size()) {
        filtered.push_back(target);
      }
    }
  }

  if (!changed) {
    RegCloseKey(key);
    return;
  }
  if (filtered.empty()) {
    RegDeleteValueW(key, L"PendingFileRenameOperations");
    RegCloseKey(key);
    return;
  }

  std::wstring multi;
  for (const auto& entry : filtered) {
    multi.append(entry);
    multi.push_back(L'\0');
  }
  multi.push_back(L'\0');
  RegSetValueExW(key,
                 L"PendingFileRenameOperations",
                 0,
                 REG_MULTI_SZ,
                 reinterpret_cast<const BYTE*>(multi.data()),
                 static_cast<DWORD>(multi.size() * sizeof(wchar_t)));
  RegCloseKey(key);
}

void RegisterDeleteOnReboot(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }
  MoveFileExW(path.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
}

void RemovePathTree(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }

  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return;
  }

  for (auto iterator =
           std::filesystem::recursive_directory_iterator(path,
                                                        std::filesystem::directory_options::
                                                            skip_permission_denied,
                                                        error);
       !error && iterator != std::filesystem::recursive_directory_iterator();
       iterator.increment(error)) {
    std::filesystem::permissions(iterator->path(),
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::add,
                                 error);
    error.clear();
  }

  std::filesystem::remove_all(path, error);
  if (!std::filesystem::exists(path, error)) {
    return;
  }

  std::vector<std::filesystem::path> remaining;
  error.clear();
  for (auto iterator =
           std::filesystem::recursive_directory_iterator(path,
                                                        std::filesystem::directory_options::
                                                            skip_permission_denied,
                                                        error);
       !error && iterator != std::filesystem::recursive_directory_iterator();
       iterator.increment(error)) {
    remaining.push_back(iterator->path());
  }
  std::sort(remaining.begin(),
            remaining.end(),
            [](const auto& left, const auto& right) {
              return left.native().size() > right.native().size();
            });
  for (const auto& child : remaining) {
    RegisterDeleteOnReboot(child);
  }
  RegisterDeleteOnReboot(path);
}

bool IsSafeInstallDirectory(const std::filesystem::path& path) {
  if (path.empty() || !path.has_root_path()) {
    return false;
  }
  return EqualsInsensitive(path.filename().wstring(), kInstallDirName);
}

void RemoveFontFilesAndRegistry() {
  constexpr wchar_t kFontsRegistry[] =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
  const auto target_dir = fp::GetLocalAppDataPath() / L"Microsoft" / L"Windows" / L"Fonts";

  for (const auto& font : kFontEntries) {
    const auto target = target_dir / std::wstring(font.file);
    RemoveFontResourceExW(target.c_str(), 0, nullptr);
    DeleteRegistryValue(HKEY_CURRENT_USER, kFontsRegistry, FontRegistryName(font, L"TrueType"));
    DeleteRegistryValue(HKEY_CURRENT_USER, kFontsRegistry, FontRegistryName(font, L"OpenType"));
    std::error_code error;
    std::filesystem::remove(target, error);
    if (std::filesystem::exists(target, error)) {
      RegisterDeleteOnReboot(target);
    }
  }
  BroadcastFontChange();
}

int InstallFonts(const std::filesystem::path& source_dir) {
  constexpr wchar_t kFontsRegistry[] =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
  RemoveStalePendingDeletes();

  std::error_code error;
  if (!std::filesystem::is_directory(source_dir, error)) {
    std::wcerr << L"Cannot find font directory: " << source_dir.wstring() << L"\n";
    return 1;
  }

  const auto target_dir = fp::GetLocalAppDataPath() / L"Microsoft" / L"Windows" / L"Fonts";
  std::filesystem::create_directories(target_dir, error);
  if (error) {
    std::wcerr << L"Cannot create user font directory: " << target_dir.wstring() << L"\n";
    return 1;
  }

  for (const auto& font : kFontEntries) {
    const auto source = source_dir / std::wstring(font.file);
    const auto target = target_dir / std::wstring(font.file);
    if (!std::filesystem::exists(source, error)) {
      std::wcerr << L"Missing font: " << source.wstring() << L"\n";
      return 1;
    }

    std::filesystem::copy_file(source,
                               target,
                               std::filesystem::copy_options::overwrite_existing,
                               error);
    if (error && !std::filesystem::exists(target, error)) {
      std::wcerr << L"Cannot copy font: " << source.wstring() << L"\n";
      return 1;
    }
    error.clear();

    DeleteRegistryValue(HKEY_CURRENT_USER, kFontsRegistry, FontRegistryName(font, L"TrueType"));
    DeleteRegistryValue(HKEY_CURRENT_USER, kFontsRegistry, FontRegistryName(font, L"OpenType"));
    const auto registry_name = FontRegistryName(font, FontRegistryKind(font));
    if (!SetRegistryString(HKEY_CURRENT_USER, kFontsRegistry, registry_name, target.wstring())) {
      std::wcerr << L"Cannot register font: " << std::wstring(font.name) << L"\n";
      return 1;
    }
    AddFontResourceExW(target.c_str(), 0, nullptr);
  }

  BroadcastFontChange();
  std::cout << "Fonts installed.\n";
  return 0;
}

void RemoveScheduledTask() {
  ComPtr<ITaskService> service;
  HRESULT result = CoCreateInstance(CLSID_TaskScheduler,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_ITaskService,
                                    reinterpret_cast<void**>(service.put()));
  if (FAILED(result) || !service) {
    return;
  }

  VARIANT empty;
  VariantInit(&empty);
  result = service->Connect(empty, empty, empty, empty);
  if (FAILED(result)) {
    return;
  }

  ComPtr<ITaskFolder> root;
  BSTR root_path = SysAllocString(L"\\");
  result = service->GetFolder(root_path, root.put());
  SysFreeString(root_path);
  if (FAILED(result) || !root) {
    return;
  }

  BSTR task_name = SysAllocString(std::wstring(kTaskName).c_str());
  root->DeleteTask(task_name, 0);
  SysFreeString(task_name);
}

std::vector<DWORD> FindProcessIds(std::wstring_view process_name) {
  std::vector<DWORD> ids;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return ids;
  }

  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  if (Process32FirstW(snapshot, &entry)) {
    do {
      if (EqualsInsensitive(entry.szExeFile, process_name)) {
        ids.push_back(entry.th32ProcessID);
      }
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return ids;
}

struct CloseWindowsContext {
  DWORD process_id = 0;
  bool posted = false;
};

BOOL CALLBACK PostCloseToProcessWindows(HWND window, LPARAM parameter) {
  auto* context = reinterpret_cast<CloseWindowsContext*>(parameter);
  DWORD process_id = 0;
  GetWindowThreadProcessId(window, &process_id);
  if (process_id == context->process_id) {
    PostMessageW(window, WM_CLOSE, 0, 0);
    context->posted = true;
  }
  return TRUE;
}

int CloseSettingsProcess() {
  const auto process_ids = FindProcessIds(kSettingsProcessName);
  if (process_ids.empty()) {
    return 0;
  }

  std::vector<HANDLE> process_handles;
  for (const DWORD process_id : process_ids) {
    CloseWindowsContext context{process_id, false};
    EnumWindows(PostCloseToProcessWindows, reinterpret_cast<LPARAM>(&context));

    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, process_id);
    if (process != nullptr) {
      process_handles.push_back(process);
    }
  }

  const ULONGLONG deadline = GetTickCount64() + 3000;
  for (HANDLE process : process_handles) {
    const DWORD remaining =
        GetTickCount64() >= deadline ? 0 : static_cast<DWORD>(deadline - GetTickCount64());
    if (WaitForSingleObject(process, remaining) == WAIT_TIMEOUT) {
      TerminateProcess(process, 0);
      WaitForSingleObject(process, 1000);
    }
    CloseHandle(process);
  }

  std::cout << "Settings app closed.\n";
  return 0;
}

int CleanupInstall(const std::filesystem::path& install_dir,
                   bool skip_install_dir,
                   bool keep_user_data) {
  CloseSettingsProcess();
  RemoveFontFilesAndRegistry();
  RemoveScheduledTask();

  constexpr wchar_t kClsid[] = L"{76e3ad5b-1dd8-4584-b3cd-127df0239720}";
  constexpr wchar_t kProfile[] = L"{21e29f6d-32dc-4f6d-8477-9ed72313c625}";
  constexpr wchar_t kKeyboardLayout[] = L"E0200804";
  const std::wstring user_profile_value = std::wstring(L"0804:") + kClsid + kProfile;

  DeleteRegistryTree(HKEY_LOCAL_MACHINE, L"Software\\FluentPinyin");
  DeleteRegistryTree(HKEY_LOCAL_MACHINE,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FluentPinyin");
  DeleteRegistryTree(HKEY_LOCAL_MACHINE,
                     (std::wstring(L"Software\\Microsoft\\CTF\\TIP\\") + kClsid).c_str());
  DeleteRegistryTree(HKEY_CURRENT_USER,
                     (std::wstring(L"Software\\Microsoft\\CTF\\TIP\\") + kClsid).c_str());
  DeleteRegistryTree(HKEY_CURRENT_USER,
                     (std::wstring(L"Software\\Classes\\CLSID\\") + kClsid).c_str());
  DeleteRegistryTree(HKEY_LOCAL_MACHINE,
                     (std::wstring(L"Software\\Classes\\CLSID\\") + kClsid).c_str());
  DeleteRegistryTree(HKEY_CLASSES_ROOT, (std::wstring(L"CLSID\\") + kClsid).c_str());
  DeleteRegistryTree(HKEY_LOCAL_MACHINE,
                     (std::wstring(L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\") +
                      kKeyboardLayout)
                         .c_str());
  DeleteRegistryValue(HKEY_CURRENT_USER,
                      L"Control Panel\\International\\User Profile\\zh-Hans-CN",
                      user_profile_value);

  const std::wstring normalized_install_dir = install_dir.wstring();
  RemoveRegistryValuesMatching(
      HKEY_CURRENT_USER,
      L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache",
      {normalized_install_dir, L"FluentPinyin", L"fluent-pinyin"},
      {normalized_install_dir, L"FluentPinyin", L"fluent-pinyin"});
  RemoveRegistryValuesMatching(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store",
      {normalized_install_dir, L"FluentPinyin", L"fluent-pinyin"},
      {normalized_install_dir, L"FluentPinyin", L"fluent-pinyin"});

  if (!keep_user_data) {
    RemovePathTree(fp::GetRoamingAppDataPath() / L"FluentPinyin");
    RemovePathTree(fp::GetLocalAppDataPath() / L"FluentPinyin");
    RemovePathTree(fp::GetProgramDataPath() / L"FluentPinyin");
    const auto temp = EnvironmentPath(L"TEMP");
    if (!temp.empty()) {
      RemovePathTree(temp / L"FluentPinyin-update");
    }
  }

  if (!skip_install_dir) {
    if (!IsSafeInstallDirectory(install_dir)) {
      std::wcerr << L"Refusing to remove unexpected install directory: "
                 << install_dir.wstring() << L"\n";
      return 1;
    }
    RemovePathTree(install_dir);
  }

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
      << "Usage: fluent-pinyin-devtools <profiles|activate|activate-process|activate-session|activate-ms-pinyin-session|activate-ms-pinyin-process|active|shutdown-core|smoke|langbar|toolbar|install-fonts|cleanup-install|close-settings>\n";
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
  if (command == L"install-fonts") {
    std::filesystem::path source_dir = ModuleDirectory() / L"fonts";
    if (argc >= 3 && !HasOptionPrefix(argv[2])) {
      source_dir = argv[2];
    }
    return InstallFonts(source_dir);
  }
  if (command == L"close-settings") {
    return CloseSettingsProcess();
  }
  if (command == L"cleanup-install" || command == L"cleanup-uninstall") {
    std::filesystem::path install_dir = DefaultInstallDir();
    bool skip_install_dir = false;
    bool keep_user_data = false;
    for (int index = 2; index < argc; ++index) {
      const std::wstring_view arg = argv[index];
      if (arg == L"--skip-install-dir" || arg == L"/skip-install-dir") {
        skip_install_dir = true;
      } else if (arg == L"--keep-user-data" || arg == L"/keep-user-data") {
        keep_user_data = true;
      } else if (!HasOptionPrefix(arg)) {
        install_dir = arg;
      }
    }
    return CleanupInstall(install_dir, skip_install_dir, keep_user_data);
  }

  PrintUsage();
  return 1;
}
