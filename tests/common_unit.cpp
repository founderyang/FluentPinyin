#include "common/encoding.h"
#include "common/logging.h"
#include "common/path_utils.h"
#include "common/settings_store.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <windows.h>

namespace {

int g_failures = 0;

std::filesystem::path g_isolated_appdata_root;

void Expect(bool condition, std::string_view message) {
  if (condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAIL: " << message << "\n";
}

void UseIsolatedAppData() {
  g_isolated_appdata_root = std::filesystem::current_path() /
                            (L"common-unit-appdata-" +
                             std::to_wstring(GetCurrentProcessId()));
  const auto local = g_isolated_appdata_root / L"Local";
  const auto roaming = g_isolated_appdata_root / L"Roaming";
  const auto program_data = g_isolated_appdata_root / L"ProgramData";
  std::error_code error;
  std::filesystem::remove_all(g_isolated_appdata_root, error);
  std::filesystem::create_directories(local, error);
  std::filesystem::create_directories(roaming, error);
  std::filesystem::create_directories(program_data, error);
  SetEnvironmentVariableW(L"LOCALAPPDATA", local.c_str());
  SetEnvironmentVariableW(L"APPDATA", roaming.c_str());
  SetEnvironmentVariableW(L"PROGRAMDATA", program_data.c_str());
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
  Expect(store.SchemaVersion() == 0,
         "SettingsStore treats missing schema version as version zero");
  Expect(store.EnsureSchemaVersion(),
         "SettingsStore writes the current schema version for legacy files");
  Expect(store.SchemaVersion() == fp::kCurrentSettingsSchemaVersion,
         "SettingsStore reads the current schema version");
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
  const fp::SettingUpdate batch[] = {
      {L"delta", L"four"},
      {L"epsilon", L"five\nsix"},
  };
  Expect(store.WriteStrings(batch), "SettingsStore writes batched updates atomically");
  Expect(store.ReadString(L"gamma", L"") == L"line1,,line2",
         "SettingsStore sanitizes CR/LF on write");
  Expect(store.ReadString(L"delta", L"") == L"four",
         "SettingsStore reads values written in a batch");
  Expect(store.ReadString(L"epsilon", L"") == L"five,six",
         "SettingsStore sanitizes batched values");
  Expect(!store.ReadBool(L"beta", true),
         "SettingsStore reads updated bool values from cache");
  Expect(store.ReadString(fp::kSettingsSchemaVersionKey, L"") ==
             std::to_wstring(fp::kCurrentSettingsSchemaVersion),
         "SettingsStore preserves schema version after writes");

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
  UseIsolatedAppData();
  TestEncodingRoundTrip();
  TestEncodingInvalidInput();
  TestPaths();
  TestLogFlushPolicy();
  TestSettingsStoreReadWrite();
  std::error_code cleanup_error;
  std::filesystem::remove_all(g_isolated_appdata_root, cleanup_error);
  if (g_failures != 0) {
    std::cerr << g_failures << " common unit test failure(s)\n";
    return 1;
  }
  std::cout << "Common unit tests passed\n";
  return 0;
}
