#include "common/settings_store.h"

#include "common/encoding.h"
#include "common/path_utils.h"

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>

namespace fp {
namespace {

std::wstring NormalizeSettingLine(std::wstring line) {
  if (!line.empty() && line.front() == L'\ufeff') {
    line.erase(line.begin());
  }
  if (line.size() >= 3 && line[0] == L'\u00EF' && line[1] == L'\u00BB' &&
      line[2] == L'\u00BF') {
    line.erase(0, 3);
  }
  if (!line.empty() && line.back() == L'\r') {
    line.pop_back();
  }
  return line;
}

std::wstring DecodeMultiByteSetting(UINT code_page,
                                    DWORD flags,
                                    const char* data,
                                    size_t length) {
  if (data == nullptr || length == 0 ||
      length > static_cast<size_t>((std::numeric_limits<int>::max)())) {
    return {};
  }

  const int byte_count = static_cast<int>(length);
  const int wide_count = MultiByteToWideChar(code_page, flags, data, byte_count, nullptr, 0);
  if (wide_count <= 0) {
    return {};
  }

  std::wstring text(static_cast<size_t>(wide_count), L'\0');
  const int written =
      MultiByteToWideChar(code_page, flags, data, byte_count, text.data(), wide_count);
  if (written <= 0) {
    return {};
  }
  text.resize(static_cast<size_t>(written));
  return text;
}

std::wstring DecodeSettingsBytes(const std::vector<char>& bytes) {
  if (bytes.empty()) {
    return {};
  }

  auto byte_at = [&bytes](size_t index) {
    return static_cast<unsigned char>(bytes[index]);
  };

  if (bytes.size() >= 2 && byte_at(0) == 0xFF && byte_at(1) == 0xFE) {
    std::wstring text;
    text.reserve((bytes.size() - 2) / 2);
    for (size_t index = 2; index + 1 < bytes.size(); index += 2) {
      text.push_back(static_cast<wchar_t>(byte_at(index) | (byte_at(index + 1) << 8)));
    }
    return text;
  }

  if (bytes.size() >= 2 && byte_at(0) == 0xFE && byte_at(1) == 0xFF) {
    std::wstring text;
    text.reserve((bytes.size() - 2) / 2);
    for (size_t index = 2; index + 1 < bytes.size(); index += 2) {
      text.push_back(static_cast<wchar_t>((byte_at(index) << 8) | byte_at(index + 1)));
    }
    return text;
  }

  size_t offset = 0;
  if (bytes.size() >= 3 && byte_at(0) == 0xEF && byte_at(1) == 0xBB && byte_at(2) == 0xBF) {
    offset = 3;
  }

  const char* data = bytes.data() + offset;
  const size_t length = bytes.size() - offset;
  std::wstring text = DecodeMultiByteSetting(CP_UTF8, MB_ERR_INVALID_CHARS, data, length);
  if (!text.empty() || length == 0) {
    return text;
  }
  return DecodeMultiByteSetting(CP_ACP, 0, data, length);
}

std::string EncodeSettingsUtf8(std::wstring_view text) {
  return WideToUtf8(text);
}

std::filesystem::file_time_type FileWriteTime(const std::filesystem::path& path) {
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return std::filesystem::file_time_type{};
  }
  const auto write_time = std::filesystem::last_write_time(path, error);
  return error ? std::filesystem::file_time_type{} : write_time;
}

std::filesystem::path TemporarySettingsPath(const std::filesystem::path& path) {
  const std::wstring suffix =
      L"." + std::to_wstring(GetCurrentProcessId()) + L"." +
      std::to_wstring(GetCurrentThreadId()) + L".tmp";
  return path.parent_path() / (path.filename().wstring() + suffix);
}

}  // namespace

std::filesystem::path GetSettingsPath() {
  return GetFpRoamingDataPath() / L"settings.ini";
}

std::wstring SanitizeSettingValue(std::wstring_view value) {
  std::wstring sanitized(value);
  std::replace(sanitized.begin(), sanitized.end(), L'\r', L',');
  std::replace(sanitized.begin(), sanitized.end(), L'\n', L',');
  return sanitized;
}

bool ParseBoolSettingValue(std::wstring_view value, bool default_value) {
  if (value == L"1" || value == L"true" || value == L"True" ||
      value == L"TRUE" || value == L"on" || value == L"yes") {
    return true;
  }
  if (value == L"0" || value == L"false" || value == L"False" ||
      value == L"FALSE" || value == L"off" || value == L"no") {
    return false;
  }
  return default_value;
}

bool IsTruthySettingValue(std::wstring_view value) {
  return ParseBoolSettingValue(value, false);
}

std::vector<std::wstring> ReadSettingLines(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }

  input.seekg(0, std::ios::end);
  const std::streamoff size = input.tellg();
  if (size <= 0) {
    return {};
  }
  input.seekg(0, std::ios::beg);

  std::vector<char> bytes(static_cast<size_t>(size));
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  bytes.resize(static_cast<size_t>(std::max<std::streamsize>(0, input.gcount())));

  std::vector<std::wstring> lines;
  const std::wstring text = DecodeSettingsBytes(bytes);
  std::wistringstream stream(text);
  std::wstring line;
  while (std::getline(stream, line)) {
    lines.push_back(NormalizeSettingLine(std::move(line)));
  }
  return lines;
}

bool WriteSettingLines(const std::filesystem::path& path,
                       const std::vector<std::wstring>& lines) {
  if (!EnsureDirectory(path.parent_path())) {
    return false;
  }

  std::wstring content;
  for (const auto& line : lines) {
    content += line;
    content += L"\n";
  }
  const std::string bytes = EncodeSettingsUtf8(content);

  const auto temp_path = TemporarySettingsPath(path);
  {
    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output) {
      return false;
    }
    if (!bytes.empty()) {
      output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    output.close();
    if (!output) {
      std::error_code remove_error;
      std::filesystem::remove(temp_path, remove_error);
      return false;
    }
  }

  if (!MoveFileExW(temp_path.c_str(),
                   path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::error_code cleanup_error;
    std::filesystem::remove(temp_path, cleanup_error);
    return false;
  }
  return true;
}

void UpsertSettingLine(std::vector<std::wstring>* lines,
                       std::wstring_view key,
                       std::wstring_view value) {
  if (lines == nullptr) {
    return;
  }
  const std::wstring prefix = std::wstring(key) + L"=";
  const std::wstring line_value = prefix + std::wstring(value);
  bool replaced = false;
  for (auto& line : *lines) {
    if (line.starts_with(prefix)) {
      line = line_value;
      replaced = true;
    }
  }
  if (!replaced) {
    lines->push_back(line_value);
  }
}

SettingsStore::SettingsStore(std::filesystem::path path) : path_(std::move(path)) {}

std::optional<std::wstring> SettingsStore::FindString(std::wstring_view key) {
  std::lock_guard lock(mutex_);
  EnsureLoadedLocked();
  const auto found = line_indices_.find(std::wstring(key));
  if (found != line_indices_.end() && !found->second.empty()) {
    const std::wstring& line = lines_[found->second.front()];
    const size_t equals = line.find(L'=');
    if (equals != std::wstring::npos) {
      return line.substr(equals + 1);
    }
  }
  return std::nullopt;
}

std::wstring SettingsStore::ReadString(std::wstring_view key,
                                       std::wstring_view default_value) {
  const std::optional<std::wstring> value = FindString(key);
  return value.has_value() ? *value : std::wstring(default_value);
}

std::optional<bool> SettingsStore::ReadOptionalBool(std::wstring_view key) {
  const std::optional<std::wstring> value = FindString(key);
  if (!value.has_value()) {
    return std::nullopt;
  }
  return IsTruthySettingValue(*value);
}

bool SettingsStore::ReadBool(std::wstring_view key, bool default_value) {
  const std::optional<std::wstring> value = FindString(key);
  return value.has_value() ? ParseBoolSettingValue(*value, default_value) : default_value;
}

bool SettingsStore::WriteString(std::wstring_view key, std::wstring_view value) {
  const SettingUpdate update{std::wstring(key), SanitizeSettingValue(value)};
  return WriteStrings(std::span<const SettingUpdate>(&update, 1));
}

bool SettingsStore::WriteBool(std::wstring_view key, bool value) {
  return WriteString(key, value ? L"1" : L"0");
}

bool SettingsStore::WriteStrings(std::span<const SettingUpdate> updates) {
  std::lock_guard lock(mutex_);
  EnsureLoadedLocked();
  for (const auto& update : updates) {
    UpsertSettingLine(&lines_, update.key, SanitizeSettingValue(update.value));
  }
  if (!WriteSettingLines(path_, lines_)) {
    return false;
  }
  write_time_ = FileWriteTime(path_);
  loaded_ = true;
  RebuildIndexLocked();
  return true;
}

void SettingsStore::Reset() {
  std::lock_guard lock(mutex_);
  write_time_ = {};
  loaded_ = false;
  lines_.clear();
  line_indices_.clear();
}

void SettingsStore::EnsureLoadedLocked() {
  const auto write_time = FileWriteTime(path_);
  if (loaded_ && write_time_ == write_time) {
    return;
  }

  write_time_ = write_time;
  loaded_ = true;
  lines_ = ReadSettingLines(path_);
  RebuildIndexLocked();
}

void SettingsStore::RebuildIndexLocked() {
  line_indices_.clear();
  for (size_t index = 0; index < lines_.size(); ++index) {
    const std::wstring& line = lines_[index];
    const size_t equals = line.find(L'=');
    if (equals == std::wstring::npos) {
      continue;
    }
    line_indices_[line.substr(0, equals)].push_back(index);
  }
}

}  // namespace fp
