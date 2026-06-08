#include "devtools/app_runtime_utils.h"

#include "common/logging.h"
#include "common/path_utils.h"

#include <windows.h>
#include <appmodel.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fp::devtools {
namespace {

constexpr std::wstring_view kWindowsAppRuntimeInstallerName =
    L"windowsappruntimeinstall-x64.exe";
constexpr std::wstring_view kWindowsAppRuntimePackageFamily =
    L"Microsoft.WindowsAppRuntime.1.8_8wekyb3d8bbwe";
constexpr std::wstring_view kWindowsAppRuntimeRequiredVersion =
    L"8000.806.2252.0";

std::wstring ErrorCodeToString(DWORD error) {
  wchar_t buffer[16]{};
  swprintf_s(buffer, L"0x%08lX", static_cast<unsigned long>(error));
  return buffer;
}

bool TryParsePackageVersion(std::wstring_view value, PACKAGE_VERSION& version) {
  version.Version = 0;
  std::array<unsigned long, 4> parts{};
  size_t offset = 0;
  for (size_t index = 0; index < parts.size(); ++index) {
    if (offset >= value.size()) {
      return false;
    }
    size_t dot = value.find(L'.', offset);
    const size_t end = dot == std::wstring_view::npos ? value.size() : dot;
    if (end == offset) {
      return false;
    }
    unsigned long part = 0;
    for (size_t pos = offset; pos < end; ++pos) {
      if (value[pos] < L'0' || value[pos] > L'9') {
        return false;
      }
      part = part * 10 + static_cast<unsigned long>(value[pos] - L'0');
      if (part > 65535) {
        return false;
      }
    }
    parts[index] = part;
    offset = end + 1;
    if (dot == std::wstring_view::npos && index + 1 < parts.size()) {
      return false;
    }
  }
  if (offset != value.size() + 1) {
    return false;
  }

  version.Major = static_cast<USHORT>(parts[0]);
  version.Minor = static_cast<USHORT>(parts[1]);
  version.Build = static_cast<USHORT>(parts[2]);
  version.Revision = static_cast<USHORT>(parts[3]);
  return true;
}

bool PackageVersionAtLeast(PACKAGE_VERSION actual, PACKAGE_VERSION required) {
  return actual.Version >= required.Version;
}

bool IsWindowsAppRuntimeReady() {
  PACKAGE_VERSION required{};
  if (!TryParsePackageVersion(kWindowsAppRuntimeRequiredVersion, required)) {
    return false;
  }

  UINT32 package_count = 0;
  UINT32 buffer_length = 0;
  LONG result = FindPackagesByPackageFamily(std::wstring(kWindowsAppRuntimePackageFamily).c_str(),
                                            PACKAGE_FILTER_HEAD | PACKAGE_FILTER_DIRECT,
                                            &package_count,
                                            nullptr,
                                            &buffer_length,
                                            nullptr,
                                            nullptr);
  if (result != ERROR_INSUFFICIENT_BUFFER || package_count == 0 || buffer_length == 0) {
    return false;
  }

  std::vector<PWSTR> package_full_names(package_count);
  std::vector<wchar_t> buffer(buffer_length);
  result = FindPackagesByPackageFamily(std::wstring(kWindowsAppRuntimePackageFamily).c_str(),
                                       PACKAGE_FILTER_HEAD | PACKAGE_FILTER_DIRECT,
                                       &package_count,
                                       package_full_names.data(),
                                       &buffer_length,
                                       buffer.data(),
                                       nullptr);
  if (result != ERROR_SUCCESS) {
    fp::LogWarning(L"installer",
                   L"FindPackagesByPackageFamily failed: " + ErrorCodeToString(result));
    return false;
  }

  for (UINT32 index = 0; index < package_count; ++index) {
    const PWSTR full_name = package_full_names[index];
    if (full_name == nullptr) {
      continue;
    }

    UINT32 package_id_length = 0;
    result = PackageIdFromFullName(full_name, PACKAGE_INFORMATION_BASIC, &package_id_length, nullptr);
    if (result != ERROR_INSUFFICIENT_BUFFER || package_id_length == 0) {
      continue;
    }

    std::vector<BYTE> package_id_buffer(package_id_length);
    auto* package_id = reinterpret_cast<PACKAGE_ID*>(package_id_buffer.data());
    result = PackageIdFromFullName(
        full_name, PACKAGE_INFORMATION_BASIC, &package_id_length, package_id_buffer.data());
    if (result == ERROR_SUCCESS && PackageVersionAtLeast(package_id->version, required)) {
      return true;
    }
  }
  return false;
}

std::wstring QuoteCommandArgument(const std::filesystem::path& path) {
  std::wstring value = path.wstring();
  std::wstring quoted;
  quoted.reserve(value.size() + 2);
  quoted.push_back(L'"');
  quoted.append(value);
  quoted.push_back(L'"');
  return quoted;
}

int RunProcessAndWait(const std::filesystem::path& executable,
                      std::wstring_view arguments,
                      DWORD* exit_code) {
  if (exit_code != nullptr) {
    *exit_code = static_cast<DWORD>(-1);
  }

  std::wstring command_line = QuoteCommandArgument(executable);
  if (!arguments.empty()) {
    command_line.push_back(L' ');
    command_line.append(arguments);
  }
  std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
  mutable_command.push_back(L'\0');

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESHOWWINDOW;
  startup.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION process{};
  const auto working_dir = executable.parent_path();
  const BOOL created = CreateProcessW(executable.c_str(),
                                      mutable_command.data(),
                                      nullptr,
                                      nullptr,
                                      FALSE,
                                      CREATE_NO_WINDOW,
                                      nullptr,
                                      working_dir.c_str(),
                                      &startup,
                                      &process);
  if (!created) {
    return static_cast<int>(GetLastError());
  }

  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD process_exit_code = static_cast<DWORD>(-1);
  GetExitCodeProcess(process.hProcess, &process_exit_code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  if (exit_code != nullptr) {
    *exit_code = process_exit_code;
  }
  return 0;
}

}  // namespace

int EnsureWindowsAppRuntime() {
  const ULONGLONG start_tick = GetTickCount64();
  if (IsWindowsAppRuntimeReady()) {
    fp::LogInfo(L"installer", L"Windows App Runtime 1.8 is already installed.");
    std::cout << "Windows App Runtime is ready.\n";
    return 0;
  }

  const auto installer = fp::GetSiblingExecutablePath(kWindowsAppRuntimeInstallerName);
  std::error_code error;
  if (!std::filesystem::exists(installer, error)) {
    fp::LogError(L"installer",
                 L"Missing Windows App Runtime installer: " + installer.wstring());
    std::wcerr << L"Missing Windows App Runtime installer: " << installer.wstring() << L"\n";
    return 1;
  }

  DWORD exit_code = static_cast<DWORD>(-1);
  const int launch_error = RunProcessAndWait(installer, L"--quiet", &exit_code);
  if (launch_error != 0) {
    fp::LogError(L"installer",
                 L"Failed to launch Windows App Runtime installer: " +
                     ErrorCodeToString(static_cast<DWORD>(launch_error)));
    std::wcerr << L"Failed to launch Windows App Runtime installer: "
               << ErrorCodeToString(static_cast<DWORD>(launch_error)) << L"\n";
    return 1;
  }

  const ULONGLONG elapsed_ms = GetTickCount64() - start_tick;
  constexpr DWORD kRestartRequiredExitCode = 3010;
  const bool ok = exit_code == 0 || exit_code == ERROR_SUCCESS_REBOOT_REQUIRED ||
                  exit_code == kRestartRequiredExitCode || IsWindowsAppRuntimeReady();
  const std::wstring message =
      std::wstring(L"Windows App Runtime installer completed in ") +
      std::to_wstring(elapsed_ms) + L" ms; exit_code=" + ErrorCodeToString(exit_code) +
      L"; ready=" + (IsWindowsAppRuntimeReady() ? std::wstring(L"yes") : std::wstring(L"no")) +
      L".";
  if (ok) {
    fp::LogInfo(L"installer", message);
    std::cout << "Windows App Runtime installer completed.\n";
    return 0;
  }

  fp::LogError(L"installer", message);
  std::wcerr << L"Windows App Runtime installer failed: " << ErrorCodeToString(exit_code)
             << L"\n";
  return 1;
}

}  // namespace fp::devtools
