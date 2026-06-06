#include "common/logging.h"

#include "common/path_utils.h"

#include <windows.h>

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fp {
namespace {

std::mutex g_log_mutex;
bool g_log_directory_ready = false;
constexpr std::uint64_t kLogFlushIntervalMs = 1500;
constexpr std::uintmax_t kMaxLogFileSizeBytes = 8ull * 1024ull * 1024ull;
constexpr int kLogBackupCount = 2;

struct LogFileState {
  std::wofstream file;
  std::uint64_t last_flush_ms = 0;
  std::uintmax_t estimated_size = 0;
  bool dirty = false;
};

std::unordered_map<std::wstring, LogFileState> g_log_files;

std::filesystem::path LogPath(const std::filesystem::path& log_dir,
                              std::wstring_view log_name) {
  return log_dir / (std::wstring(log_name) + L".log");
}

std::filesystem::path BackupLogPath(const std::filesystem::path& log_dir,
                                    std::wstring_view log_name,
                                    int index) {
  return log_dir / (std::wstring(log_name) + L".log." + std::to_wstring(index));
}

std::uintmax_t CurrentLogFileSize(const std::filesystem::path& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  return error ? 0 : size;
}

std::uintmax_t EstimateLogEntryBytes(std::wstring_view level,
                                     std::wstring_view message) noexcept {
  constexpr std::uintmax_t kTimestampAndMarkupChars = 36;
  return (kTimestampAndMarkupChars + level.size() + message.size()) * sizeof(wchar_t);
}

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

void RotateLogIfNeeded(const std::filesystem::path& log_dir,
                       std::wstring_view log_name,
                       LogFileState& state,
                       std::uintmax_t pending_bytes = 0) {
  const auto path = LogPath(log_dir, log_name);
  if (!detail::ShouldRotateLog(state.estimated_size + pending_bytes, kMaxLogFileSizeBytes)) {
    return;
  }

  if (state.file.is_open()) {
    state.file.flush();
    state.file.close();
  }
  state.dirty = false;
  state.last_flush_ms = 0;
  state.estimated_size = 0;

  std::error_code error;
  std::filesystem::remove(BackupLogPath(log_dir, log_name, kLogBackupCount), error);
  for (int index = kLogBackupCount - 1; index >= 1; --index) {
    error = {};
    const auto source = BackupLogPath(log_dir, log_name, index);
    if (std::filesystem::exists(source, error)) {
      error = {};
      std::filesystem::rename(source, BackupLogPath(log_dir, log_name, index + 1), error);
    }
  }

  error = {};
  std::filesystem::rename(path, BackupLogPath(log_dir, log_name, 1), error);
}

void WriteLog(std::wstring_view level,
              std::wstring_view component,
              std::wstring_view message) {
  std::lock_guard lock(g_log_mutex);

  const auto log_dir = GetFpLogDirectory();
  if (!g_log_directory_ready) {
    g_log_directory_ready = EnsureDirectory(log_dir);
  }
  if (!g_log_directory_ready) {
    return;
  }

  const std::wstring log_name = SanitizeComponent(component);
  auto [file_iter, inserted] = g_log_files.try_emplace(log_name);
  auto& state = file_iter->second;
  if (inserted || !state.file.is_open()) {
    state.estimated_size = CurrentLogFileSize(LogPath(log_dir, log_name));
    RotateLogIfNeeded(log_dir, log_name, state);
    state.file.open(LogPath(log_dir, log_name), std::ios::app);
    state.estimated_size = CurrentLogFileSize(LogPath(log_dir, log_name));
  }
  if (!state.file) {
    g_log_files.erase(file_iter);
    return;
  }

  const std::uintmax_t entry_size = EstimateLogEntryBytes(level, message);
  RotateLogIfNeeded(log_dir, log_name, state, entry_size);
  if (!state.file.is_open()) {
    state.file.open(LogPath(log_dir, log_name), std::ios::app);
    state.estimated_size = CurrentLogFileSize(LogPath(log_dir, log_name));
  }
  if (!state.file) {
    g_log_files.erase(file_iter);
    return;
  }

  auto& file = state.file;
  file << L'[' << Timestamp() << L"] [" << level << L"] " << message << L'\n';
  state.estimated_size += entry_size;
  state.dirty = true;
  const std::uint64_t now_ms = GetTickCount64();
  if (detail::ShouldFlushLog(level, now_ms, state.last_flush_ms, state.dirty)) {
    file.flush();
    state.last_flush_ms = now_ms;
    state.dirty = false;
  }
}

}  // namespace

namespace detail {

bool ShouldFlushLog(std::wstring_view level,
                    std::uint64_t now_ms,
                    std::uint64_t last_flush_ms,
                    bool dirty) noexcept {
  if (!dirty) {
    return false;
  }
  if (level == L"WARN" || level == L"ERROR") {
    return true;
  }
  if (last_flush_ms == 0) {
    return true;
  }
  return now_ms - last_flush_ms >= kLogFlushIntervalMs;
}

bool ShouldRotateLog(std::uintmax_t file_size, std::uintmax_t max_file_size) noexcept {
  return max_file_size > 0 && file_size >= max_file_size;
}

}  // namespace detail

void LogInfo(std::wstring_view component, std::wstring_view message) {
  WriteLog(L"INFO", component, message);
}

void LogWarning(std::wstring_view component, std::wstring_view message) {
  WriteLog(L"WARN", component, message);
}

void LogError(std::wstring_view component, std::wstring_view message) {
  WriteLog(L"ERROR", component, message);
}

void FlushLogs() {
  std::lock_guard lock(g_log_mutex);
  const std::uint64_t now_ms = GetTickCount64();
  for (auto& [_, state] : g_log_files) {
    if (state.dirty && state.file) {
      state.file.flush();
      state.last_flush_ms = now_ms;
      state.dirty = false;
    }
  }
}

}  // namespace fp
