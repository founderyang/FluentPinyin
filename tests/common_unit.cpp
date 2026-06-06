#include "common/encoding.h"
#include "common/logging.h"
#include "common/path_utils.h"
#include "common/settings_store.h"

#include <filesystem>
#include <fstream>
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
  using fp::detail::ShouldRotateLog;

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
  Expect(!ShouldRotateLog(1024, 2048), "small log files do not rotate");
  Expect(ShouldRotateLog(2048, 2048), "log files rotate at the configured cap");
  Expect(!ShouldRotateLog(2048, 0), "zero log rotation cap disables rotation");
}

void WriteBytes(const std::filesystem::path& path, const std::string& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void TestSettingsStoreReadWrite() {
  const auto test_dir = fp::GetFpLogDirectory() / L"common-unit-settings";
  std::error_code error;
  std::filesystem::remove_all(test_dir, error);
  Expect(fp::EnsureDirectory(test_dir), "settings test directory is created");

  const auto settings_path = test_dir / L"settings.ini";
  const std::string utf8_bom = "\xEF\xBB\xBF";
  WriteBytes(settings_path,
             utf8_bom + "alpha=one\r\nbeta=true\nalpha=two\nbroken\n");

  fp::SettingsStore store(settings_path);
  Expect(store.ReadString(L"alpha", L"missing") == L"one",
         "SettingsStore reads the first value for duplicate keys");
  Expect(store.ReadBool(L"beta", false),
         "SettingsStore parses truthy bool values");
  Expect(store.ReadString(L"missing", L"default") == L"default",
         "SettingsStore returns defaults for missing values");

  Expect(store.WriteString(L"gamma", L"line1\r\nline2"),
         "SettingsStore writes sanitized strings");
  Expect(store.WriteBool(L"beta", false),
         "SettingsStore writes bool values");
  Expect(store.ReadString(L"gamma", L"") == L"line1,,line2",
         "SettingsStore sanitizes CR/LF on write");
  Expect(!store.ReadBool(L"beta", true),
         "SettingsStore reads updated bool values from cache");

  const auto lines = fp::ReadSettingLines(settings_path);
  bool saw_gamma = false;
  bool saw_accidental_key = false;
  for (const auto& line : lines) {
    saw_gamma = saw_gamma || line == L"gamma=line1,,line2";
    saw_accidental_key = saw_accidental_key || line.starts_with(L"line2=");
  }
  Expect(saw_gamma, "ReadSettingLines sees sanitized values");
  Expect(!saw_accidental_key, "Sanitized values do not create new keys");

  std::filesystem::remove_all(test_dir, error);
}

}  // namespace

int main() {
  TestEncodingRoundTrip();
  TestEncodingInvalidInput();
  TestPaths();
  TestLogFlushPolicy();
  TestSettingsStoreReadWrite();
  if (g_failures != 0) {
    std::cerr << g_failures << " common unit test failure(s)\n";
    return 1;
  }
  std::cout << "Common unit tests passed\n";
  return 0;
}
