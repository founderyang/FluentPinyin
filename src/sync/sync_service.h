#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fp::sync {

enum class SyncProvider {
  ObjectStorage,
  WebDav,
};

struct SyncConfig {
  SyncProvider provider = SyncProvider::ObjectStorage;
  bool sync_clipboard = false;
  bool sync_user_data = false;
  bool auto_enabled = false;
  int auto_interval_minutes = 30;
  std::wstring device_id;

  std::wstring object_endpoint;
  std::wstring object_bucket;
  std::wstring object_region;
  std::wstring object_access_key;
  std::wstring object_secret_key;
  std::wstring object_key;

  std::wstring webdav_url;
  std::wstring webdav_username;
  std::wstring webdav_password;

  std::wstring encryption_secret;
};

struct SyncResult {
  bool success = false;
  std::wstring message;
};

struct PackageMetadata {
  std::uint64_t created_unix = 0;
  std::wstring device_id;
  bool has_clipboard = false;
  bool has_user_data = false;
};

std::filesystem::path DefaultSettingsPath();
std::filesystem::path DefaultRimeUserDataPath();
std::filesystem::path DefaultBackupDirectory();
std::wstring NormalizeProviderValue(std::wstring_view value);
std::wstring DefaultObjectKey();
std::wstring NewTimestampedBackupName();

std::wstring ReadSetting(const std::filesystem::path& settings_path,
                         std::wstring_view key,
                         std::wstring_view default_value = L"");
bool WriteSetting(const std::filesystem::path& settings_path,
                  std::wstring_view key,
                  std::wstring_view value);

std::optional<std::wstring> ProtectSecretText(std::wstring_view plaintext);
std::optional<std::wstring> UnprotectSecretText(std::wstring_view protected_text);

SyncConfig LoadConfig(const std::filesystem::path& settings_path = DefaultSettingsPath());
SyncResult ValidateRemoteConfig(const SyncConfig& config);
SyncResult ValidateCryptoConfig(const SyncConfig& config);

SyncResult CreateLocalBackup(const std::filesystem::path& package_path,
                             const SyncConfig& config);
SyncResult RestoreLocalBackup(const std::filesystem::path& package_path,
                              const SyncConfig& config,
                              bool create_pre_restore_backup = true,
                              PackageMetadata* metadata = nullptr);
SyncResult UploadNow(const SyncConfig& config);
SyncResult DownloadNow(const SyncConfig& config,
                       bool create_pre_restore_backup = true,
                       PackageMetadata* metadata = nullptr);
SyncResult RunAutoSync(const std::filesystem::path& settings_path = DefaultSettingsPath());

SyncResult InstallScheduledSync(const std::filesystem::path& sync_exe_path,
                                int interval_minutes);
SyncResult RemoveScheduledSync();

}  // namespace fp::sync
