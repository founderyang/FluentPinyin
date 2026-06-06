#include "common/encoding.h"
#include "common/logging.h"
#include "common/path_utils.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int g_failures = 0;

void Expect(bool condition, std::string_view message) {
  if (condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAIL: " << message << "\n";
}

void TestEncodingRoundTrip() {
  const std::wstring wide = L"FluentPinyin 流畅拼音 \U0001F680";
  const std::string utf8 = fp::WideToUtf8(wide);
  Expect(!utf8.empty(), "WideToUtf8 returns text for non-empty input");
  Expect(fp::Utf8ToWide(utf8) == wide, "Utf8ToWide round trips Chinese and non-BMP text");
  Expect(fp::Utf8ToWideStrict(utf8) == wide,
         "Utf8ToWideStrict round trips valid Chinese and non-BMP text");
}

void TestEncodingInvalidInput() {
  const std::string invalid = std::string("ok") + static_cast<char>(0xC0) +
                              static_cast<char>(0xAF);
  Expect(fp::Utf8ToWideStrict(invalid).empty(), "Utf8ToWideStrict rejects invalid UTF-8");
}

void TestPaths() {
  const auto local = fp::GetFpLocalDataPath();
  const auto roaming = fp::GetFpRoamingDataPath();
  const auto logs = fp::GetFpLogDirectory();
  Expect(!local.empty(), "GetFpLocalDataPath is not empty");
  Expect(!roaming.empty(), "GetFpRoamingDataPath is not empty");
  Expect(logs.filename() == L"Logs", "GetFpLogDirectory ends in Logs");

  const auto test_dir = logs / L"common-unit";
  Expect(fp::EnsureDirectory(test_dir), "EnsureDirectory creates test directory");
  Expect(std::filesystem::is_directory(test_dir), "EnsureDirectory target is a directory");
  std::error_code error;
  std::filesystem::remove(test_dir, error);
}

void TestLogFlushPolicy() {
  using fp::detail::ShouldFlushLog;

  Expect(ShouldFlushLog(L"INFO", 100, 0, true),
         "first dirty info log flushes to create visible output");
  Expect(!ShouldFlushLog(L"INFO", 500, 100, true),
         "frequent info logs skip immediate flush");
  Expect(ShouldFlushLog(L"INFO", 1700, 100, true),
         "info logs flush after the interval");
  Expect(ShouldFlushLog(L"WARN", 500, 100, true),
         "warnings flush immediately");
  Expect(ShouldFlushLog(L"ERROR", 500, 100, true),
         "errors flush immediately");
  Expect(!ShouldFlushLog(L"ERROR", 500, 100, false),
         "clean log state does not flush");
}

}  // namespace

int main() {
  TestEncodingRoundTrip();
  TestEncodingInvalidInput();
  TestPaths();
  TestLogFlushPolicy();
  if (g_failures != 0) {
    std::cerr << g_failures << " common unit test failure(s)\n";
    return 1;
  }
  std::cout << "Common unit tests passed\n";
  return 0;
}
