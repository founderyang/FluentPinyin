#include "common/constants.h"
#include "tsf/guids.h"
#include "common/encoding.h"
#include "common/logging.h"
#include "common/path_utils.h"
#include "core/rime_engine.h"

#include <ctffunc.h>
#include <msctf.h>
#include <shellapi.h>
#include <taskschd.h>
#include <tlhelp32.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
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
constexpr wchar_t kUninstallRegistryRoot[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
constexpr std::array<std::wstring_view, 4> kInputRelatedProcessNames{
    L"fluent-pinyin-ui.exe",
    L"fluent-pinyin-settings.exe",
    L"ctfmon.exe",
    L"TextInputHost.exe",
};

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

std::filesystem::path ModuleExecutablePath() {
  std::wstring buffer(MAX_PATH, L'\0');
  DWORD length = 0;
  for (;;) {
    length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      return {};
    }
    if (length < buffer.size() - 1) {
      break;
    }
    buffer.resize(buffer.size() * 2);
  }

  buffer.resize(length);
  return std::filesystem::path(buffer);
}

std::filesystem::path SystemDirectoryPath() {
  std::wstring buffer(MAX_PATH, L'\0');
  const UINT length = GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
  if (length == 0) {
    return L"C:\\Windows\\System32";
  }
  if (length >= buffer.size()) {
    buffer.resize(length + 1);
    const UINT retry_length =
        GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    if (retry_length == 0 || retry_length >= buffer.size()) {
      return L"C:\\Windows\\System32";
    }
    buffer.resize(retry_length);
    return std::filesystem::path(buffer);
  }
  buffer.resize(length);
  return std::filesystem::path(buffer);
}

void UseSafeCurrentDirectory() {
  const auto system_dir = SystemDirectoryPath();
  SetCurrentDirectoryW(system_dir.c_str());
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

bool ReadRegistryString(HKEY root,
                        const wchar_t* subkey,
                        const std::wstring& name,
                        std::wstring* value) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(root, subkey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
    return false;
  }

  DWORD type = 0;
  DWORD bytes = 0;
  LSTATUS status =
      RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &bytes);
  if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0) {
    RegCloseKey(key);
    return false;
  }

  std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
  status = RegQueryValueExW(
      key, name.c_str(), nullptr, &type, reinterpret_cast<BYTE*>(buffer.data()), &bytes);
  RegCloseKey(key);
  if (status != ERROR_SUCCESS) {
    return false;
  }
  buffer.resize(wcsnlen_s(buffer.c_str(), buffer.size()));
  *value = buffer;
  return true;
}

std::vector<std::wstring> FindUninstallKeysByName(std::wstring_view display_name) {
  std::vector<std::wstring> matches;
  HKEY root_key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    kUninstallRegistryRoot,
                    0,
                    KEY_ENUMERATE_SUB_KEYS,
                    &root_key) != ERROR_SUCCESS) {
    return matches;
  }

  DWORD index = 0;
  for (;;) {
    DWORD name_chars = 256;
    std::wstring child(name_chars, L'\0');
    const LSTATUS status = RegEnumKeyExW(
        root_key, index, child.data(), &name_chars, nullptr, nullptr, nullptr, nullptr);
    if (status == ERROR_NO_MORE_ITEMS) {
      break;
    }
    if (status != ERROR_SUCCESS) {
      ++index;
      continue;
    }
    child.resize(name_chars);
    const std::wstring subkey =
        std::wstring(kUninstallRegistryRoot) + L"\\" + child;
    std::wstring value;
    if (ReadRegistryString(HKEY_LOCAL_MACHINE, subkey.c_str(), L"DisplayName", &value) &&
        value == display_name) {
      matches.push_back(subkey);
    }
    ++index;
  }

  RegCloseKey(root_key);
  return matches;
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

bool RemoveFileNowOrOnReboot(const std::filesystem::path& path) {
  if (path.empty()) {
    return true;
  }

  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return true;
  }

  std::filesystem::permissions(path,
                               std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::add,
                               error);
  error.clear();

  for (int attempt = 0; attempt < 8; ++attempt) {
    std::filesystem::remove(path, error);
    if (!std::filesystem::exists(path, error)) {
      return true;
    }
    DeleteFileW(path.c_str());
    if (!std::filesystem::exists(path, error)) {
      return true;
    }
    Sleep(250);
    error.clear();
  }

  RegisterDeleteOnReboot(path);
  return false;
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

  for (int attempt = 0; attempt < 6; ++attempt) {
    std::filesystem::remove_all(path, error);
    if (!std::filesystem::exists(path, error)) {
      return;
    }
    Sleep(250);
    error.clear();
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
    if (std::filesystem::is_regular_file(child, error)) {
      RemoveFileNowOrOnReboot(child);
    } else {
      RegisterDeleteOnReboot(child);
    }
    error.clear();
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
    for (int attempt = 0; attempt < 4; ++attempt) {
      RemoveFontResourceExW(target.c_str(), 0, nullptr);
    }
    DeleteRegistryValue(HKEY_CURRENT_USER, kFontsRegistry, FontRegistryName(font, L"TrueType"));
    DeleteRegistryValue(HKEY_CURRENT_USER, kFontsRegistry, FontRegistryName(font, L"OpenType"));
    RemoveFileNowOrOnReboot(target);
  }
  BroadcastFontChange();
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

std::vector<DWORD> FindAllProcessIds() {
  std::vector<DWORD> ids;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return ids;
  }

  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  if (Process32FirstW(snapshot, &entry)) {
    do {
      ids.push_back(entry.th32ProcessID);
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return ids;
}

struct ProcessBasicInformation {
  PVOID reserved1 = nullptr;
  PVOID peb_base_address = nullptr;
  PVOID reserved2[2]{};
  ULONG_PTR unique_process_id = 0;
  PVOID reserved3 = nullptr;
};

using NtQueryInformationProcessFn = LONG(WINAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);

NtQueryInformationProcessFn NtQueryInformationProcessPtr() {
  static const auto function = reinterpret_cast<NtQueryInformationProcessFn>(
      GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess"));
  return function;
}

template <typename T>
bool ReadRemoteValue(HANDLE process, std::uintptr_t address, T* value) {
  SIZE_T bytes_read = 0;
  return ReadProcessMemory(process,
                           reinterpret_cast<LPCVOID>(address),
                           value,
                           sizeof(T),
                           &bytes_read) &&
         bytes_read == sizeof(T);
}

std::wstring ReadProcessCurrentDirectory(DWORD process_id) {
  static_assert(sizeof(void*) == 8, "FluentPinyin devtools is built for x64.");
  auto* nt_query_information_process = NtQueryInformationProcessPtr();
  if (nt_query_information_process == nullptr || process_id == 0 ||
      process_id == GetCurrentProcessId()) {
    return {};
  }

  HANDLE process =
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, process_id);
  if (process == nullptr) {
    return {};
  }

  ProcessBasicInformation info{};
  ULONG returned = 0;
  const LONG status = nt_query_information_process(process,
                                                   0,
                                                   &info,
                                                   static_cast<ULONG>(sizeof(info)),
                                                   &returned);
  if (status != 0 || info.peb_base_address == nullptr) {
    CloseHandle(process);
    return {};
  }

  constexpr std::uintptr_t kPebProcessParametersOffset = 0x20;
  constexpr std::uintptr_t kCurrentDirectoryDosPathOffset = 0x38;
  std::uintptr_t process_parameters = 0;
  if (!ReadRemoteValue(process,
                       reinterpret_cast<std::uintptr_t>(info.peb_base_address) +
                           kPebProcessParametersOffset,
                       &process_parameters) ||
      process_parameters == 0) {
    CloseHandle(process);
    return {};
  }

  struct RemoteUnicodeString {
    USHORT length = 0;
    USHORT maximum_length = 0;
    ULONG padding = 0;
    std::uintptr_t buffer = 0;
  };
  RemoteUnicodeString current_directory{};
  if (!ReadRemoteValue(process,
                       process_parameters + kCurrentDirectoryDosPathOffset,
                       &current_directory) ||
      current_directory.length == 0 || current_directory.buffer == 0 ||
      current_directory.length > 32766 ||
      current_directory.length > current_directory.maximum_length) {
    CloseHandle(process);
    return {};
  }

  std::wstring value(current_directory.length / sizeof(wchar_t), L'\0');
  SIZE_T bytes_read = 0;
  const bool read_ok =
      ReadProcessMemory(process,
                        reinterpret_cast<LPCVOID>(current_directory.buffer),
                        value.data(),
                        current_directory.length,
                        &bytes_read) &&
      bytes_read == current_directory.length;
  CloseHandle(process);
  if (!read_ok) {
    return {};
  }
  return value;
}

std::wstring NormalizePathForCompare(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::path normalized = std::filesystem::weakly_canonical(path, error);
  if (error) {
    error.clear();
    normalized = std::filesystem::absolute(path, error);
  }
  if (error) {
    normalized = path;
  }

  std::wstring value = normalized.wstring();
  std::replace(value.begin(), value.end(), L'/', L'\\');
  while (value.size() > 3 && (value.back() == L'\\' || value.back() == L'/')) {
    value.pop_back();
  }
  return ToLower(value);
}

bool IsPathWithinDirectory(const std::filesystem::path& path,
                           const std::filesystem::path& directory) {
  const std::wstring value = NormalizePathForCompare(path);
  const std::wstring root = NormalizePathForCompare(directory);
  return value == root ||
         (value.size() > root.size() && value.starts_with(root) &&
          value[root.size()] == L'\\');
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

void CloseProcessGracefully(DWORD process_id, DWORD timeout_ms) {
  if (process_id == 0 || process_id == GetCurrentProcessId()) {
    return;
  }

  CloseWindowsContext context{process_id, false};
  EnumWindows(PostCloseToProcessWindows, reinterpret_cast<LPARAM>(&context));

  HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, process_id);
  if (process == nullptr) {
    return;
  }

  if (WaitForSingleObject(process, timeout_ms) == WAIT_TIMEOUT) {
    TerminateProcess(process, 0);
    WaitForSingleObject(process, 1000);
  }
  CloseHandle(process);
}

void CloseProcessesUsingDirectory(const std::filesystem::path& directory) {
  if (directory.empty()) {
    return;
  }

  for (const DWORD process_id : FindAllProcessIds()) {
    const std::wstring current_directory = ReadProcessCurrentDirectory(process_id);
    if (!current_directory.empty() &&
        IsPathWithinDirectory(std::filesystem::path(current_directory), directory)) {
      CloseProcessGracefully(process_id, 2500);
    }
  }
}

int CloseSettingsProcess() {
  const auto process_ids = FindProcessIds(kSettingsProcessName);
  if (process_ids.empty()) {
    return 0;
  }

  std::vector<HANDLE> process_handles;
  for (const DWORD process_id : process_ids) {
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, process_id);
    if (process != nullptr) {
      CloseWindowsContext context{process_id, false};
      EnumWindows(PostCloseToProcessWindows, reinterpret_cast<LPARAM>(&context));
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

int RestartTextServicesProcess() {
  for (std::wstring_view process_name : kInputRelatedProcessNames) {
    const auto process_ids = FindProcessIds(process_name);
    for (const DWORD process_id : process_ids) {
      if (process_id == GetCurrentProcessId()) {
        continue;
      }
      HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, process_id);
      if (process == nullptr) {
        continue;
      }
      TerminateProcess(process, 0);
      WaitForSingleObject(process, 3000);
      CloseHandle(process);
    }
  }

  std::cout << "Text services restarted.\n";
  return 0;
}

int PrepareInstall() {
  const ULONGLONG start_tick = GetTickCount64();
  RemoveStalePendingDeletes();
  RemoveFontFilesAndRegistry();
  const ULONGLONG elapsed_ms = GetTickCount64() - start_tick;
  fp::LogInfo(L"installer",
              std::wstring(L"prepare-install completed in ") +
                  std::to_wstring(elapsed_ms) + L" ms.");
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

  for (const auto& subkey : FindUninstallKeysByName(L"FluentPinyin")) {
    updated = SetRegistryString(HKEY_LOCAL_MACHINE, subkey.c_str(), L"DisplayIcon", display_icon) ||
              updated;
    updated = SetRegistryString(
                  HKEY_LOCAL_MACHINE, subkey.c_str(), L"InstallLocation", install_location) ||
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

int StartRimeWarmupProcess() {
  const ULONGLONG start_tick = GetTickCount64();
  const auto executable = ModuleExecutablePath();
  if (executable.empty()) {
    fp::LogWarning(L"installer", L"Failed to resolve devtools path for Rime warmup.");
    return 1;
  }

  const auto working_dir = executable.parent_path();
  std::wstring command_line = L"\"" + executable.wstring() + L"\" warmup-rime";
  std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
  mutable_command.push_back(L'\0');

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESHOWWINDOW;
  startup.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessW(executable.c_str(),
                                      mutable_command.data(),
                                      nullptr,
                                      nullptr,
                                      FALSE,
                                      CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS,
                                      nullptr,
                                      working_dir.c_str(),
                                      &startup,
                                      &process);
  if (!created) {
    const DWORD error = GetLastError();
    fp::LogWarning(L"installer",
                   L"Failed to start Rime warmup process: " + std::to_wstring(error));
    std::wcerr << L"Failed to start Rime warmup process: " << error << L"\n";
    return 1;
  }

  const DWORD process_id = process.dwProcessId;
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  const ULONGLONG elapsed_ms = GetTickCount64() - start_tick;
  fp::LogInfo(L"installer",
              L"Started Rime warmup process " + std::to_wstring(process_id) + L" in " +
                  std::to_wstring(elapsed_ms) + L" ms.");
  std::cout << "Rime warmup started in " << elapsed_ms << " ms.\n";
  return 0;
}

int WarmupRime() {
  const ULONGLONG start_tick = GetTickCount64();
  SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
  const BOOL background_mode = SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);

  fp::core::RimeEngine engine;
  fp::core::RimeEngineOptions options;
  options.deploy = false;
  auto status = engine.Initialize(options);
  if (status.initialized && !engine.HasBuiltSchema()) {
    fp::LogWarning(L"installer", L"Rime warmup found missing build cache; redeploying.");
    engine.Shutdown();
    options.deploy = true;
    status = engine.Initialize(options);
  }

  const bool built_schema = status.initialized && engine.HasBuiltSchema();
  if (background_mode) {
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
  }

  const ULONGLONG elapsed_ms = GetTickCount64() - start_tick;
  if (!status.initialized) {
    fp::LogError(L"installer",
                 L"Rime warmup failed in " + std::to_wstring(elapsed_ms) + L" ms: " +
                     status.message);
    std::wcerr << L"Rime warmup failed: " << status.message << L"\n";
    return 1;
  }

  fp::LogInfo(L"installer",
              L"Rime warmup completed in " + std::to_wstring(elapsed_ms) +
                  L" ms; built_schema=" +
                  (built_schema ? std::wstring(L"yes") : std::wstring(L"no")) + L".");
  std::cout << "Rime warmup completed in " << elapsed_ms << " ms.\n";
  return 0;
}

int CleanupInstall(const std::filesystem::path& install_dir,
                   bool skip_install_dir,
                   bool keep_user_data,
                   bool restart_text_services) {
  RemoveStalePendingDeletes();
  CloseSettingsProcess();
  CloseProcessesUsingDirectory(install_dir);
  if (restart_text_services) {
    RestartTextServicesProcess();
  }
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
  DeleteRegistryTree(HKEY_USERS,
                     (std::wstring(L"S-1-5-18\\Software\\Classes\\CLSID\\") + kClsid).c_str());
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
  std::cout << "GetItem(kInputModeLangBarItemGuid): " << fp::WideToUtf8(HresultToString(result))
            << "\n";
  if (SUCCEEDED(result) && input_mode) {
    TF_LANGBARITEMINFO info{};
    DWORD status = 0;
    if (SUCCEEDED(input_mode->GetInfo(&info))) {
      std::cout << "  clsid: " << fp::WideToUtf8(GuidToString(info.clsidService)) << "\n";
      std::cout << "  desc:  " << fp::WideToUtf8(info.szDescription) << "\n";
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
      << "Usage: fluent-pinyin-devtools <profiles|activate|activate-process|activate-session|activate-ms-pinyin-session|activate-ms-pinyin-process|active|shutdown-core|smoke|langbar|toolbar|prepare-install|finalize-install|start-rime-warmup|warmup-rime|cleanup-install|close-settings|restart-text-services>\n";
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  UseSafeCurrentDirectory();
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
  if (command == L"prepare-install") {
    return PrepareInstall();
  }
  if (command == L"finalize-install") {
    return FinalizeInstall();
  }
  if (command == L"start-rime-warmup") {
    return StartRimeWarmupProcess();
  }
  if (command == L"warmup-rime") {
    return WarmupRime();
  }
  if (command == L"close-settings") {
    return CloseSettingsProcess();
  }
  if (command == L"restart-text-services") {
    return RestartTextServicesProcess();
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
  UseSafeCurrentDirectory();
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr) {
    return wmain(0, nullptr);
  }
  const int result = wmain(argc, argv);
  LocalFree(argv);
  return result;
}
