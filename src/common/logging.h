#pragma once

#include <cstdint>
#include <string_view>

namespace fp {

namespace detail {

[[nodiscard]] bool ShouldFlushLog(std::wstring_view level,
                                  std::uint64_t now_ms,
                                  std::uint64_t last_flush_ms,
                                  bool dirty) noexcept;
[[nodiscard]] bool ShouldRotateLog(std::uintmax_t file_size,
                                   std::uintmax_t max_file_size) noexcept;

}  // namespace detail

void LogInfo(std::wstring_view component, std::wstring_view message);
void LogWarning(std::wstring_view component, std::wstring_view message);
void LogError(std::wstring_view component, std::wstring_view message);
void FlushLogs();

}  // namespace fp
