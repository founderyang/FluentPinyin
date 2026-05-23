#include "common/logging.h"

#include "common/path_utils.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <cstdio>
#include <string>

namespace fp {
namespace {

std::mutex g_log_mutex;

std::wstring Timestamp() {
  SYSTEMTIME time{};
  GetLocalTime(&time);

  wchar_t buffer[64]{};
  swprintf_s(buffer,
             L"%04hu-%02hu-%02hu %02hu:%02hu:%02hu.%03hu",
             time.wYear,
             time.wMonth,
             time.wDay,
             time.wHour,
             time.wMinute,
             time.wSecond,
             time.wMilliseconds);
  return buffer;
}

std::wstring SanitizeComponent(std::wstring_view component) {
  std::wstring value(component);
  for (wchar_t& ch : value) {
    if (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' || ch == L'?' ||
        ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|') {
      ch = L'_';
    }
  }

  if (value.empty()) {
    value = L"fp";
  }

  return value;
}

void WriteLog(std::wstring_view level,
              std::wstring_view component,
              std::wstring_view message) {
  std::lock_guard lock(g_log_mutex);

  const auto log_dir = GetFpLogDirectory();
  if (!EnsureDirectory(log_dir)) {
    return;
  }

  const auto log_path = log_dir / (SanitizeComponent(component) + L".log");
  std::wofstream file(log_path, std::ios::app);
  if (!file) {
    return;
  }

  file << L'[' << Timestamp() << L"] [" << level << L"] " << message << L'\n';
}

}  // namespace

void LogInfo(std::wstring_view component, std::wstring_view message) {
  WriteLog(L"INFO", component, message);
}

void LogWarning(std::wstring_view component, std::wstring_view message) {
  WriteLog(L"WARN", component, message);
}

void LogError(std::wstring_view component, std::wstring_view message) {
  WriteLog(L"ERROR", component, message);
}

}  // namespace fp
