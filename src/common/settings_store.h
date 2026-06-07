#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fp {

struct SettingUpdate {
  std::wstring key;
  std::wstring value;
};

inline constexpr std::wstring_view kSettingsSchemaVersionKey = L"settings_schema_version";
inline constexpr int kCurrentSettingsSchemaVersion = 1;

std::filesystem::path GetSettingsPath();
std::wstring SanitizeSettingValue(std::wstring_view value);
bool ParseBoolSettingValue(std::wstring_view value, bool default_value);
bool IsTruthySettingValue(std::wstring_view value);
std::vector<std::wstring> ReadSettingLines(const std::filesystem::path& path);
bool WriteSettingLines(const std::filesystem::path& path,
                       const std::vector<std::wstring>& lines);
void UpsertSettingLine(std::vector<std::wstring>* lines,
                       std::wstring_view key,
                       std::wstring_view value);

class SettingsStore {
 public:
  explicit SettingsStore(std::filesystem::path path = GetSettingsPath());

  std::optional<std::wstring> FindString(std::wstring_view key);
  std::wstring ReadString(std::wstring_view key, std::wstring_view default_value = L"");
  std::optional<bool> ReadOptionalBool(std::wstring_view key);
  bool ReadBool(std::wstring_view key, bool default_value);
  int SchemaVersion();
  bool EnsureSchemaVersion();

  bool WriteString(std::wstring_view key, std::wstring_view value);
  bool WriteBool(std::wstring_view key, bool value);
  bool WriteStrings(std::span<const SettingUpdate> updates);

  void Reset();

 private:
  void EnsureLoadedLocked();
  void RebuildIndexLocked();

  std::filesystem::path path_;
  std::filesystem::file_time_type write_time_{};
  bool loaded_ = false;
  std::vector<std::wstring> lines_;
  std::unordered_map<std::wstring, std::vector<size_t>> line_indices_;
  std::mutex mutex_;
};

}  // namespace fp
