#include "sync/sync_service.h"

#include "common/constants.h"

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

std::wstring ReadFileText(const std::filesystem::path& path) {
  std::wifstream input(path);
  std::wstring text;
  std::wstring line;
  while (std::getline(input, line)) {
    text += line;
    text += L"\n";
  }
  return text;
}

void TestReadWriteSettings(const std::filesystem::path& settings_path) {
  Expect(fp::sync::ReadSetting(settings_path, L"missing", L"default") == L"default",
         "ReadSetting returns default for a missing file/key");

  Expect(fp::sync::WriteSetting(settings_path, fp::kSyncProviderSetting, fp::kSyncProviderWebDav),
         "WriteSetting creates settings file");
  Expect(fp::sync::WriteSetting(settings_path, fp::kSyncProviderSetting, fp::kSyncProviderObject),
         "WriteSetting updates existing keys");
  Expect(fp::sync::WriteSetting(settings_path, fp::kSyncObjectBucketSetting, L"bucket-a"),
         "WriteSetting appends new keys");

  Expect(fp::sync::ReadSetting(settings_path, fp::kSyncProviderSetting, L"") ==
             std::wstring(fp::kSyncProviderObject),
         "ReadSetting returns updated value");
  Expect(fp::sync::ReadSetting(settings_path, fp::kSyncObjectBucketSetting, L"") == L"bucket-a",
         "ReadSetting returns appended value");

  const std::wstring text = ReadFileText(settings_path);
  Expect(text.find(L"sync_provider=webdav") == std::wstring::npos,
         "WriteSetting replaces old key values");
}

void TestSettingValueSanitization(const std::filesystem::path& settings_path) {
  Expect(fp::sync::WriteSetting(settings_path, fp::kSyncDeviceIdSetting, L"desk\r\nline2"),
         "WriteSetting accepts values with line breaks");
  Expect(fp::sync::ReadSetting(settings_path, fp::kSyncDeviceIdSetting, L"") == L"desk,,line2",
         "WriteSetting sanitizes CR/LF to keep settings.ini line-oriented");

  const std::wstring text = ReadFileText(settings_path);
  Expect(text.find(L"line2=") == std::wstring::npos,
         "Sanitized values do not create accidental keys");
}

void TestLoadConfig(const std::filesystem::path& settings_path) {
  fp::sync::WriteSetting(settings_path, fp::kSyncProviderSetting, L" WebDAV ");
  fp::sync::WriteSetting(settings_path, fp::kSyncClipboardSetting, L"yes");
  fp::sync::WriteSetting(settings_path, fp::kSyncUserDataSetting, L"0");
  fp::sync::WriteSetting(settings_path, fp::kSyncAutoEnabledSetting, L"true");
  fp::sync::WriteSetting(settings_path, fp::kSyncAutoIntervalMinutesSetting, L"2");
  fp::sync::WriteSetting(settings_path,
                         fp::kSyncWebDavUrlSetting,
                         L"https://example.invalid/sync");
  fp::sync::WriteSetting(settings_path, fp::kSyncWebDavUsernameSetting, L"user");
  fp::sync::WriteSetting(settings_path, fp::kSyncEncryptionSecretSetting, L"plain-secret");

  const fp::sync::SyncConfig config = fp::sync::LoadConfig(settings_path);
  Expect(config.provider == fp::sync::SyncProvider::WebDav,
         "LoadConfig normalizes provider values");
  Expect(config.sync_clipboard, "LoadConfig parses truthy bool values");
  Expect(!config.sync_user_data, "LoadConfig parses false bool values");
  Expect(config.auto_enabled, "LoadConfig parses auto sync bool");
  Expect(config.auto_interval_minutes == 5,
         "LoadConfig clamps auto sync interval to the minimum");
  Expect(config.webdav_url == L"https://example.invalid/sync",
         "LoadConfig reads WebDAV URL");
  Expect(config.webdav_username == L"user",
         "LoadConfig reads WebDAV username");
  Expect(config.encryption_secret == L"plain-secret",
         "LoadConfig reads plaintext secret fallback");
}

void TestProviderNormalization() {
  Expect(fp::sync::NormalizeProviderValue(L" webdav ") == L"webdav",
         "NormalizeProviderValue accepts WebDAV with whitespace");
  Expect(fp::sync::NormalizeProviderValue(L"OBJECT") == L"object",
         "NormalizeProviderValue defaults object storage values to object");
  Expect(fp::sync::NormalizeProviderValue(L"") == L"object",
         "NormalizeProviderValue defaults empty values to object");
}

void TestHttpsOnlyRemoteValidation() {
  fp::sync::SyncConfig webdav;
  webdav.provider = fp::sync::SyncProvider::WebDav;
  webdav.webdav_url = L"http://example.invalid/dav";
  Expect(!fp::sync::ValidateRemoteConfig(webdav).success,
         "ValidateRemoteConfig rejects plaintext WebDAV URLs");

  webdav.webdav_url = L"https://example.invalid/dav";
  Expect(fp::sync::ValidateRemoteConfig(webdav).success,
         "ValidateRemoteConfig accepts HTTPS WebDAV URLs");

  fp::sync::SyncConfig object;
  object.provider = fp::sync::SyncProvider::ObjectStorage;
  object.object_endpoint = L"http://s3.example.invalid";
  object.object_bucket = L"bucket";
  object.object_access_key = L"access";
  object.object_secret_key = L"secret";
  Expect(!fp::sync::ValidateRemoteConfig(object).success,
         "ValidateRemoteConfig rejects plaintext object storage endpoints");

  object.object_endpoint = L"https://s3.example.invalid";
  Expect(fp::sync::ValidateRemoteConfig(object).success,
         "ValidateRemoteConfig accepts HTTPS object storage endpoints");
}

void TestLoopbackHttpDevelopmentException() {
  SetEnvironmentVariableW(L"FLUENT_PINYIN_SYNC_ALLOW_HTTP_LOCAL", nullptr);
  Expect(!fp::sync::IsSecureRemoteUrl(L"http://localhost:8080/sync"),
         "Loopback HTTP is rejected unless the development exception is enabled");
  SetEnvironmentVariableW(L"FLUENT_PINYIN_SYNC_ALLOW_HTTP_LOCAL", L"1");
  Expect(fp::sync::IsSecureRemoteUrl(L"http://127.0.0.1:8080/sync"),
         "Loopback HTTP is accepted with the explicit development exception");
  Expect(!fp::sync::IsSecureRemoteUrl(L"http://example.invalid/sync"),
         "Non-loopback HTTP is rejected even with the development exception");
  SetEnvironmentVariableW(L"FLUENT_PINYIN_SYNC_ALLOW_HTTP_LOCAL", nullptr);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 2) {
    std::wcerr << L"Usage: sync_settings <work-dir>\n";
    return 1;
  }

  const std::filesystem::path work_dir = argv[1];
  std::error_code error;
  std::filesystem::remove_all(work_dir, error);
  std::filesystem::create_directories(work_dir, error);
  if (error) {
    std::wcerr << L"Failed to create work dir: " << error.message().c_str() << L"\n";
    return 2;
  }

  const auto settings_path = work_dir / L"settings.ini";
  TestProviderNormalization();
  TestHttpsOnlyRemoteValidation();
  TestLoopbackHttpDevelopmentException();
  TestReadWriteSettings(settings_path);
  TestSettingValueSanitization(settings_path);
  TestLoadConfig(settings_path);

  if (g_failures != 0) {
    std::cerr << g_failures << " sync settings test failure(s)\n";
    return 3;
  }
  std::cout << "Sync settings tests passed\n";
  return 0;
}
