#include "devtools/process_utils.h"

#include "common/encoding.h"
#include "common/logging.h"

#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace fp::devtools {
namespace {

constexpr wchar_t kSettingsProcessName[] = L"fluent-pinyin-settings.exe";
constexpr std::wstring_view kInputRelatedProcessNames[]{
    L"fluent-pinyin-ui.exe",
    L"fluent-pinyin-settings.exe",
    L"ctfmon.exe",
    L"TextInputHost.exe",
};

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

}  // namespace

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
      if (fp::EqualsInsensitive(entry.szExeFile, process_name)) {
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

std::vector<DWORD> FindProcessIdsLoadingModules(
    const std::vector<std::wstring_view>& module_names) {
  std::vector<DWORD> ids;
  for (const DWORD process_id : FindAllProcessIds()) {
    if (process_id == 0 || process_id == GetCurrentProcessId()) {
      continue;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                                               process_id);
    if (snapshot == INVALID_HANDLE_VALUE) {
      continue;
    }
    MODULEENTRY32W module{};
    module.dwSize = sizeof(module);
    bool matched = false;
    if (Module32FirstW(snapshot, &module)) {
      do {
        for (std::wstring_view name : module_names) {
          if (fp::EqualsInsensitive(module.szModule, name)) {
            matched = true;
            break;
          }
        }
      } while (!matched && Module32NextW(snapshot, &module));
    }
    CloseHandle(snapshot);
    if (matched) {
      ids.push_back(process_id);
    }
  }
  return ids;
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
  return fp::ToLowerInvariant(std::move(value));
}

bool IsPathWithinDirectory(const std::filesystem::path& path,
                           const std::filesystem::path& directory) {
  const std::wstring value = NormalizePathForCompare(path);
  const std::wstring root = NormalizePathForCompare(directory);
  return value == root ||
         (value.size() > root.size() && value.starts_with(root) &&
          value[root.size()] == L'\\');
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

int CloseLegacyInputHosts() {
  const auto process_ids = FindProcessIdsLoadingModules({
      L"fluent-pinyin-core.dll",
      L"fluent-pinyin-tsf.dll",
      L"rime.dll",
  });
  int closed = 0;
  for (const DWORD process_id : process_ids) {
    CloseProcessGracefully(process_id, 3000);
    ++closed;
  }
  fp::LogInfo(L"installer",
              L"legacy input host cleanup requested for " + std::to_wstring(closed) +
                  L" process(es).");
  std::cout << "Legacy input hosts closed: " << closed << "\n";
  return 0;
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
  CloseLegacyInputHosts();

  std::cout << "Text services restarted.\n";
  return 0;
}

}  // namespace fp::devtools
