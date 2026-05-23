#include "common/path_utils.h"

#include <windows.h>

#include <cstdlib>
#include <string>
#include <system_error>

namespace fp {
namespace {

std::filesystem::path GetEnvironmentPath(const wchar_t* name) {
  DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
  if (required == 0) {
    return {};
  }

  std::wstring value(required, L'\0');
  DWORD written = GetEnvironmentVariableW(name, value.data(), required);
  if (written == 0 || written >= required) {
    return {};
  }

  value.resize(written);
  return std::filesystem::path(value);
}

std::filesystem::path FallbackTempPath() {
  std::wstring buffer(MAX_PATH, L'\0');
  DWORD length = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
  if (length == 0 || length >= buffer.size()) {
    return std::filesystem::current_path();
  }

  buffer.resize(length);
  return std::filesystem::path(buffer);
}

}  // namespace

std::filesystem::path GetLocalAppDataPath() {
  auto path = GetEnvironmentPath(L"LOCALAPPDATA");
  return path.empty() ? FallbackTempPath() : path;
}

std::filesystem::path GetRoamingAppDataPath() {
  auto path = GetEnvironmentPath(L"APPDATA");
  return path.empty() ? GetLocalAppDataPath() : path;
}

std::filesystem::path GetProgramDataPath() {
  auto path = GetEnvironmentPath(L"PROGRAMDATA");
  return path.empty() ? GetLocalAppDataPath() : path;
}

std::filesystem::path GetFpLocalDataPath() {
  return GetLocalAppDataPath() / L"FluentPinyin";
}

std::filesystem::path GetFpRoamingDataPath() {
  return GetRoamingAppDataPath() / L"FluentPinyin";
}

std::filesystem::path GetFpLogDirectory() {
  return GetFpLocalDataPath() / L"Logs";
}

bool EnsureDirectory(const std::filesystem::path& path) {
  std::error_code error;
  if (std::filesystem::exists(path, error)) {
    return std::filesystem::is_directory(path, error);
  }

  return std::filesystem::create_directories(path, error);
}

}  // namespace fp
