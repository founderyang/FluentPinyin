#include "sync/sync_service.h"

#include "common/constants.h"
#include "common/encoding.h"
#include "common/path_utils.h"
#include "common/settings_store.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <taskschd.h>
#include <comdef.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")

namespace fp::sync {
namespace {

constexpr std::wstring_view kPackageMagic = L"FPSYNC2";
constexpr std::wstring_view kTaskName = L"FluentPinyinAutoSync";
constexpr std::wstring_view kTaskPath = L"\\FluentPinyinAutoSync";
constexpr std::uint32_t kPackageVersion = 2;
constexpr std::uint32_t kPbkdf2Iterations = 150000;
constexpr std::uint64_t kMaxPackageFileBytes = 256ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaxSingleEntryBytes = 96ull * 1024ull * 1024ull;

constexpr std::array<std::wstring_view, 18> kRimeExcludedFileNames{
    L".build-signature",
    L".build-shared-data-signature",
    L".build-input-signature",
    L"user.yaml",
    L"installation.yaml",
    L"fluentpinyin.userdb.txt",
    L"wanxiang.userdb.txt",
    L"wanxiang_pro.userdb.txt",
    L"predict.userdb.txt",
    L"tips.userdb.txt",
    L"sequence.userdb.txt",
    L"replacer.userdb.txt",
    L"stats.userdb.txt",
    L".DS_Store",
    L"Thumbs.db",
    L"desktop.ini",
    L"rime.log",
    L"wanxiang-lts-zh-hans.gram",
};

constexpr std::array<std::wstring_view, 7> kRimeExcludedTopDirectories{
    L"build",
    L"sync",
    L"logs",
    L"log",
    L"tmp",
    L"temp",
    L"cache",
};

bool IsSecretKey(std::wstring_view key) {
  return key == fp::kSyncObjectSecretKeySetting ||
         key == fp::kSyncWebDavPasswordSetting ||
         key == fp::kSyncEncryptionSecretSetting;
}

std::wstring UrlEncodePathSegment(std::wstring_view value) {
  std::wstring out;
  constexpr wchar_t hex[] = L"0123456789ABCDEF";
  for (const wchar_t ch : value) {
    const bool safe = (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                      (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'_' ||
                      ch == L'.' || ch == L'~';
    if (safe) {
      out.push_back(ch);
      continue;
    }
    const auto byte = static_cast<unsigned char>(ch & 0xFF);
    out.push_back(L'%');
    out.push_back(hex[(byte >> 4) & 0xF]);
    out.push_back(hex[byte & 0xF]);
  }
  return out;
}

std::wstring PercentEncodeUtf8(std::string_view value, bool encode_slash) {
  std::wstring out;
  constexpr wchar_t hex[] = L"0123456789ABCDEF";
  for (const unsigned char ch : value) {
    const bool safe = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                      (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
                      ch == '.' || ch == '~' || (!encode_slash && ch == '/');
    if (safe) {
      out.push_back(static_cast<wchar_t>(ch));
      continue;
    }
    out.push_back(L'%');
    out.push_back(hex[(ch >> 4) & 0xF]);
    out.push_back(hex[ch & 0xF]);
  }
  return out;
}

std::uint64_t UnixNow() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

std::wstring TimestampText() {
  SYSTEMTIME local{};
  GetLocalTime(&local);
  std::wstringstream out;
  out << std::setfill(L'0') << std::setw(4) << local.wYear << std::setw(2) << local.wMonth
      << std::setw(2) << local.wDay << L"-" << std::setw(2) << local.wHour
      << std::setw(2) << local.wMinute << std::setw(2) << local.wSecond;
  return out.str();
}

std::vector<std::uint8_t> RandomBytes(size_t count) {
  std::vector<std::uint8_t> bytes(count);
  if (!bytes.empty()) {
    BCryptGenRandom(nullptr,
                    bytes.data(),
                    static_cast<ULONG>(bytes.size()),
                    BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  }
  return bytes;
}

std::wstring DeviceName() {
  DWORD required = 0;
  GetComputerNameExW(ComputerNameDnsHostname, nullptr, &required);
  if (required == 0) {
    return L"windows";
  }
  std::wstring name(required, L'\0');
  if (!GetComputerNameExW(ComputerNameDnsHostname, name.data(), &required) || required == 0) {
    return L"windows";
  }
  name.resize(required);
  for (wchar_t& ch : name) {
    if (!((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
          (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'_')) {
      ch = L'-';
    }
  }
  return name.empty() ? L"windows" : name;
}

std::wstring ReadClipboardText() {
  if (!OpenClipboard(nullptr)) {
    return {};
  }

  std::wstring text;
  HANDLE data = GetClipboardData(CF_UNICODETEXT);
  if (data != nullptr) {
    const wchar_t* locked = static_cast<const wchar_t*>(GlobalLock(data));
    if (locked != nullptr) {
      text = locked;
      GlobalUnlock(data);
    }
  }
  CloseClipboard();
  return text;
}

bool WriteClipboardText(std::wstring_view text) {
  if (!OpenClipboard(nullptr)) {
    return false;
  }
  EmptyClipboard();
  const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (data == nullptr) {
    CloseClipboard();
    return false;
  }
  void* locked = GlobalLock(data);
  if (locked == nullptr) {
    GlobalFree(data);
    CloseClipboard();
    return false;
  }
  std::memcpy(locked, text.data(), text.size() * sizeof(wchar_t));
  static_cast<wchar_t*>(locked)[text.size()] = L'\0';
  GlobalUnlock(data);
  if (SetClipboardData(CF_UNICODETEXT, data) == nullptr) {
    GlobalFree(data);
    CloseClipboard();
    return false;
  }
  CloseClipboard();
  return true;
}

bool ReadBinaryFile(const std::filesystem::path& path,
                    std::vector<std::uint8_t>* out,
                    std::uint64_t max_bytes = kMaxSingleEntryBytes) {
  out->clear();
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return false;
  }
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > max_bytes) {
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return false;
  }
  out->resize(static_cast<size_t>(size));
  if (!out->empty()) {
    input.read(reinterpret_cast<char*>(out->data()), static_cast<std::streamsize>(out->size()));
  }
  return input.good() || input.eof();
}

bool WriteBinaryFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& data) {
  if (!fp::EnsureDirectory(path.parent_path())) {
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return false;
  }
  if (!data.empty()) {
    output.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
  }
  return output.good();
}

void AppendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
  }
}

void AppendU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
  }
}

bool ReadU32(const std::vector<std::uint8_t>& data, size_t* offset, std::uint32_t* value) {
  if (*offset + 4 > data.size()) {
    return false;
  }
  std::uint32_t result = 0;
  for (int shift = 0; shift < 32; shift += 8) {
    result |= static_cast<std::uint32_t>(data[(*offset)++]) << shift;
  }
  *value = result;
  return true;
}

bool ReadU64(const std::vector<std::uint8_t>& data, size_t* offset, std::uint64_t* value) {
  if (*offset + 8 > data.size()) {
    return false;
  }
  std::uint64_t result = 0;
  for (int shift = 0; shift < 64; shift += 8) {
    result |= static_cast<std::uint64_t>(data[(*offset)++]) << shift;
  }
  *value = result;
  return true;
}

bool AppendString(std::vector<std::uint8_t>& out, std::string_view value) {
  if (value.size() > UINT32_MAX) {
    return false;
  }
  AppendU32(out, static_cast<std::uint32_t>(value.size()));
  out.insert(out.end(), value.begin(), value.end());
  return true;
}

bool ReadString(const std::vector<std::uint8_t>& data, size_t* offset, std::string* value) {
  std::uint32_t size = 0;
  if (!ReadU32(data, offset, &size) || *offset + size > data.size()) {
    return false;
  }
  value->assign(reinterpret_cast<const char*>(data.data() + *offset), size);
  *offset += size;
  return true;
}

struct PlainEntry {
  std::string path;
  std::vector<std::uint8_t> data;
};

struct PlainPackage {
  PackageMetadata metadata;
  std::vector<PlainEntry> entries;
  std::wstring clipboard_text;
};

bool IsExcludedRimeFile(const std::filesystem::path& relative) {
  if (relative.empty()) {
    return true;
  }
  auto it = relative.begin();
  if (it != relative.end()) {
    const std::wstring first = fp::ToLowerInvariant(it->wstring());
    if (std::find(kRimeExcludedTopDirectories.begin(),
                  kRimeExcludedTopDirectories.end(),
                  first) != kRimeExcludedTopDirectories.end()) {
      return true;
    }
  }
  const std::wstring filename = fp::ToLowerInvariant(relative.filename().wstring());
  if (std::find(kRimeExcludedFileNames.begin(),
                kRimeExcludedFileNames.end(),
                filename) != kRimeExcludedFileNames.end()) {
    return true;
  }
  if (filename.ends_with(L".tmp") || filename.ends_with(L".bak") ||
      filename.ends_with(L".log") || filename.ends_with(L".userdb.kct") ||
      filename.ends_with(L".userdb.snapshot") || filename.ends_with(L".gram")) {
    return true;
  }
  return false;
}

std::vector<std::wstring> ReadSettingsLines(const std::filesystem::path& settings_path) {
  return fp::ReadSettingLines(settings_path);
}

bool WriteSettingsLines(const std::filesystem::path& settings_path,
                        const std::vector<std::wstring>& lines) {
  return fp::WriteSettingLines(settings_path, lines);
}

std::optional<std::vector<std::uint8_t>> ReadSettingsForPortablePackage(
    const std::filesystem::path& settings_path) {
  auto lines = ReadSettingsLines(settings_path);
  std::vector<std::wstring> portable_lines;
  std::map<std::wstring, std::wstring> decrypted_secrets;

  for (const auto& line : lines) {
    const size_t equals = line.find(L'=');
    if (equals == std::wstring::npos) {
      portable_lines.push_back(line);
      continue;
    }
    const std::wstring key = line.substr(0, equals);
    const std::wstring value = line.substr(equals + 1);
    if (key.ends_with(L"_protected")) {
      const std::wstring plain_key = key.substr(0, key.size() - 10);
      if (IsSecretKey(plain_key)) {
        if (const auto plain = UnprotectSecretText(value)) {
          decrypted_secrets[plain_key] = *plain;
        }
      }
      continue;
    }
    if (IsSecretKey(key)) {
      if (!value.empty()) {
        decrypted_secrets[key] = value;
      }
      continue;
    }
    portable_lines.push_back(line);
  }

  for (const auto& [key, value] : decrypted_secrets) {
    portable_lines.push_back(key + L"=" + value);
  }

  const std::string utf8 = [&]() {
    std::wstring text;
    for (const auto& line : portable_lines) {
      text += line;
      text += L"\n";
    }
    return fp::WideToUtf8(text);
  }();
  return std::vector<std::uint8_t>(utf8.begin(), utf8.end());
}

bool WriteSettingsFromPortablePackage(const std::vector<std::uint8_t>& data) {
  const std::string utf8(data.begin(), data.end());
  const std::wstring text = fp::Utf8ToWideStrict(utf8);
  std::wstringstream stream(text);
  std::wstring line;
  std::vector<std::wstring> lines;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == L'\r') {
      line.pop_back();
    }
    const size_t equals = line.find(L'=');
    if (equals == std::wstring::npos) {
      if (!line.empty()) {
        lines.push_back(line);
      }
      continue;
    }
    const std::wstring key = line.substr(0, equals);
    const std::wstring value = line.substr(equals + 1);
    if (IsSecretKey(key)) {
      if (const auto protected_value = ProtectSecretText(value)) {
        lines.push_back(key + L"=");
        lines.push_back(key + L"_protected=" + *protected_value);
      } else {
        lines.push_back(line);
      }
      continue;
    }
    if (!key.ends_with(L"_protected")) {
      lines.push_back(line);
    }
  }
  return WriteSettingsLines(DefaultSettingsPath(), lines);
}

bool IsSafeRelativeUtf8Path(std::string_view path) {
  if (path.empty() || path.size() > 4096 || path.front() == '/' || path.front() == '\\') {
    return false;
  }
  if (path.find('\0') != std::string_view::npos || path.find(':') != std::string_view::npos) {
    return false;
  }
  std::string normalized(path);
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  size_t start = 0;
  while (start <= normalized.size()) {
    const size_t slash = normalized.find('/', start);
    const size_t end = slash == std::string::npos ? normalized.size() : slash;
    const std::string_view segment(normalized.data() + start, end - start);
    if (segment.empty() || segment == "." || segment == "..") {
      return false;
    }
    if (slash == std::string::npos) {
      break;
    }
    start = slash + 1;
  }
  return true;
}

std::optional<std::filesystem::path> SafeDestination(const std::filesystem::path& root,
                                                     std::string_view relative_utf8) {
  if (!IsSafeRelativeUtf8Path(relative_utf8)) {
    return std::nullopt;
  }
  std::filesystem::path relative = fp::Utf8ToWideStrict(relative_utf8);
  if (relative.empty() || relative.is_absolute()) {
    return std::nullopt;
  }
  for (const auto& part : relative) {
    if (part == L"." || part == L"..") {
      return std::nullopt;
    }
  }
  return root / relative;
}

std::optional<PlainPackage> CollectPlainPackage(const SyncConfig& config) {
  PlainPackage package;
  package.metadata.created_unix = UnixNow();
  package.metadata.device_id = config.device_id.empty() ? DeviceName() : config.device_id;
  package.metadata.has_clipboard = config.sync_clipboard;
  package.metadata.has_user_data = config.sync_user_data;

  if (config.sync_clipboard) {
    package.clipboard_text = ReadClipboardText();
  }

  if (config.sync_user_data) {
    const auto settings_path = DefaultSettingsPath();
    std::vector<std::uint8_t> data;
    if (const auto settings_data = ReadSettingsForPortablePackage(settings_path)) {
      package.entries.push_back({"settings/settings.ini", *settings_data});
    }

    const auto rime_dir = DefaultRimeUserDataPath();
    std::error_code error;
    if (std::filesystem::exists(rime_dir, error)) {
      for (const auto& item : std::filesystem::recursive_directory_iterator(
               rime_dir, std::filesystem::directory_options::skip_permission_denied, error)) {
        if (error) {
          return std::nullopt;
        }
        if (!item.is_regular_file(error)) {
          continue;
        }
        const auto relative = std::filesystem::relative(item.path(), rime_dir, error);
        if (error || IsExcludedRimeFile(relative)) {
          continue;
        }
        if (!ReadBinaryFile(item.path(), &data)) {
          return std::nullopt;
        }
        std::string path = "rime/" + fp::WideToUtf8(relative.generic_wstring());
        package.entries.push_back({std::move(path), std::move(data)});
      }
    }
  }

  return package;
}

std::vector<std::uint8_t> SerializePlainPackage(const PlainPackage& package) {
  std::vector<std::uint8_t> data;
  AppendString(data, "FPSYNC-PLAIN");
  AppendU32(data, kPackageVersion);
  AppendU64(data, package.metadata.created_unix);
  AppendString(data, fp::WideToUtf8(package.metadata.device_id));
  data.push_back(package.metadata.has_clipboard ? 1 : 0);
  data.push_back(package.metadata.has_user_data ? 1 : 0);
  AppendString(data, fp::WideToUtf8(package.clipboard_text));
  AppendU32(data, static_cast<std::uint32_t>(package.entries.size()));
  for (const auto& entry : package.entries) {
    AppendString(data, entry.path);
    AppendU64(data, static_cast<std::uint64_t>(entry.data.size()));
    data.insert(data.end(), entry.data.begin(), entry.data.end());
  }
  return data;
}

std::optional<PlainPackage> DeserializePlainPackage(const std::vector<std::uint8_t>& data) {
  size_t offset = 0;
  std::string magic;
  if (!ReadString(data, &offset, &magic) || magic != "FPSYNC-PLAIN") {
    return std::nullopt;
  }
  std::uint32_t version = 0;
  if (!ReadU32(data, &offset, &version) || version != kPackageVersion) {
    return std::nullopt;
  }

  PlainPackage package;
  std::string device_id;
  std::string clipboard;
  if (!ReadU64(data, &offset, &package.metadata.created_unix) ||
      !ReadString(data, &offset, &device_id) || offset + 2 > data.size()) {
    return std::nullopt;
  }
  package.metadata.device_id = fp::Utf8ToWideStrict(device_id);
  package.metadata.has_clipboard = data[offset++] != 0;
  package.metadata.has_user_data = data[offset++] != 0;
  if (!ReadString(data, &offset, &clipboard)) {
    return std::nullopt;
  }
  package.clipboard_text = fp::Utf8ToWideStrict(clipboard);
  std::uint32_t entry_count = 0;
  if (!ReadU32(data, &offset, &entry_count) || entry_count > 10000) {
    return std::nullopt;
  }
  for (std::uint32_t index = 0; index < entry_count; ++index) {
    std::string path;
    std::uint64_t size = 0;
    if (!ReadString(data, &offset, &path) || !ReadU64(data, &offset, &size) ||
        size > kMaxSingleEntryBytes || offset + size > data.size() ||
        !IsSafeRelativeUtf8Path(path)) {
      return std::nullopt;
    }
    PlainEntry entry;
    entry.path = std::move(path);
    entry.data.assign(data.begin() + static_cast<std::ptrdiff_t>(offset),
                      data.begin() + static_cast<std::ptrdiff_t>(offset + size));
    offset += static_cast<size_t>(size);
    package.entries.push_back(std::move(entry));
  }
  if (offset != data.size()) {
    return std::nullopt;
  }
  return package;
}

std::optional<std::vector<std::uint8_t>> Sha256(std::string_view data) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
    return std::nullopt;
  }
  DWORD object_length = 0;
  DWORD written = 0;
  BCryptGetProperty(alg,
                    BCRYPT_OBJECT_LENGTH,
                    reinterpret_cast<PUCHAR>(&object_length),
                    sizeof(object_length),
                    &written,
                    0);
  std::vector<std::uint8_t> object(object_length);
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (BCryptCreateHash(alg, &hash, object.data(), object_length, nullptr, 0, 0) != 0) {
    BCryptCloseAlgorithmProvider(alg, 0);
    return std::nullopt;
  }
  BCryptHashData(hash,
                 reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
                 static_cast<ULONG>(data.size()),
                 0);
  std::vector<std::uint8_t> digest(32);
  const NTSTATUS status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
  BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(alg, 0);
  if (status != 0) {
    return std::nullopt;
  }
  return digest;
}

std::optional<std::vector<std::uint8_t>> HmacSha256(const std::vector<std::uint8_t>& key,
                                                    std::string_view data) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
    return std::nullopt;
  }
  DWORD object_length = 0;
  DWORD written = 0;
  BCryptGetProperty(alg,
                    BCRYPT_OBJECT_LENGTH,
                    reinterpret_cast<PUCHAR>(&object_length),
                    sizeof(object_length),
                    &written,
                    0);
  std::vector<std::uint8_t> object(object_length);
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (BCryptCreateHash(alg,
                       &hash,
                       object.data(),
                       object_length,
                       const_cast<PUCHAR>(key.data()),
                       static_cast<ULONG>(key.size()),
                       0) != 0) {
    BCryptCloseAlgorithmProvider(alg, 0);
    return std::nullopt;
  }
  BCryptHashData(hash,
                 reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
                 static_cast<ULONG>(data.size()),
                 0);
  std::vector<std::uint8_t> digest(32);
  const NTSTATUS status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
  BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(alg, 0);
  if (status != 0) {
    return std::nullopt;
  }
  return digest;
}

std::string HexLower(const std::vector<std::uint8_t>& data) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (const auto byte : data) {
    out << std::setw(2) << static_cast<int>(byte);
  }
  return out.str();
}

std::vector<std::uint8_t> HmacRaw(const std::vector<std::uint8_t>& key,
                                  std::string_view data) {
  auto digest = HmacSha256(key, data);
  return digest.value_or(std::vector<std::uint8_t>{});
}

std::vector<std::uint8_t> SigningKey(std::string_view secret,
                                     std::string_view date,
                                     std::string_view region,
                                     std::string_view service) {
  std::vector<std::uint8_t> key;
  const std::string aws_secret = "AWS4" + std::string(secret);
  key.assign(aws_secret.begin(), aws_secret.end());
  auto k_date = HmacRaw(key, date);
  auto k_region = HmacRaw(k_date, region);
  auto k_service = HmacRaw(k_region, service);
  return HmacRaw(k_service, "aws4_request");
}

bool DeriveAesKey(std::wstring_view secret,
                  const std::vector<std::uint8_t>& salt,
                  std::vector<std::uint8_t>* key) {
  key->assign(32, 0);
  BCRYPT_ALG_HANDLE alg = nullptr;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
    return false;
  }
  const std::string secret_utf8 = fp::WideToUtf8(secret);
  const NTSTATUS status = BCryptDeriveKeyPBKDF2(alg,
                                                reinterpret_cast<PUCHAR>(const_cast<char*>(secret_utf8.data())),
                                                static_cast<ULONG>(secret_utf8.size()),
                                                const_cast<PUCHAR>(salt.data()),
                                                static_cast<ULONG>(salt.size()),
                                                kPbkdf2Iterations,
                                                key->data(),
                                                static_cast<ULONG>(key->size()),
                                                0);
  BCryptCloseAlgorithmProvider(alg, 0);
  return status == 0;
}

std::optional<std::vector<std::uint8_t>> EncryptPackageBytes(const std::vector<std::uint8_t>& plain,
                                                             std::wstring_view secret) {
  const auto salt = RandomBytes(16);
  const auto nonce = RandomBytes(12);
  std::vector<std::uint8_t> key;
  if (secret.empty() || !DeriveAesKey(secret, salt, &key)) {
    return std::nullopt;
  }

  BCRYPT_ALG_HANDLE alg = nullptr;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) {
    return std::nullopt;
  }
  const wchar_t chaining[] = BCRYPT_CHAIN_MODE_GCM;
  if (BCryptSetProperty(alg,
                        BCRYPT_CHAINING_MODE,
                        reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(chaining)),
                        static_cast<ULONG>(sizeof(chaining)),
                        0) != 0) {
    BCryptCloseAlgorithmProvider(alg, 0);
    return std::nullopt;
  }

  BCRYPT_KEY_HANDLE aes_key = nullptr;
  if (BCryptGenerateSymmetricKey(alg,
                                 &aes_key,
                                 nullptr,
                                 0,
                                 key.data(),
                                 static_cast<ULONG>(key.size()),
                                 0) != 0) {
    BCryptCloseAlgorithmProvider(alg, 0);
    return std::nullopt;
  }

  std::vector<std::uint8_t> cipher(plain.size());
  std::array<std::uint8_t, 16> tag{};
  BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth{};
  BCRYPT_INIT_AUTH_MODE_INFO(auth);
  auth.pbNonce = const_cast<PUCHAR>(nonce.data());
  auth.cbNonce = static_cast<ULONG>(nonce.size());
  auth.pbTag = tag.data();
  auth.cbTag = static_cast<ULONG>(tag.size());

  ULONG bytes_done = 0;
  const NTSTATUS status = BCryptEncrypt(aes_key,
                                        const_cast<PUCHAR>(plain.data()),
                                        static_cast<ULONG>(plain.size()),
                                        &auth,
                                        nullptr,
                                        0,
                                        cipher.data(),
                                        static_cast<ULONG>(cipher.size()),
                                        &bytes_done,
                                        0);
  BCryptDestroyKey(aes_key);
  BCryptCloseAlgorithmProvider(alg, 0);
  if (status != 0 || bytes_done != cipher.size()) {
    return std::nullopt;
  }

  std::vector<std::uint8_t> package;
  AppendString(package, fp::WideToUtf8(kPackageMagic));
  AppendU32(package, kPackageVersion);
  AppendU32(package, kPbkdf2Iterations);
  AppendU32(package, static_cast<std::uint32_t>(salt.size()));
  package.insert(package.end(), salt.begin(), salt.end());
  AppendU32(package, static_cast<std::uint32_t>(nonce.size()));
  package.insert(package.end(), nonce.begin(), nonce.end());
  AppendU32(package, static_cast<std::uint32_t>(tag.size()));
  package.insert(package.end(), tag.begin(), tag.end());
  AppendU64(package, static_cast<std::uint64_t>(cipher.size()));
  package.insert(package.end(), cipher.begin(), cipher.end());
  return package;
}

std::optional<std::vector<std::uint8_t>> DecryptPackageBytes(const std::vector<std::uint8_t>& package,
                                                             std::wstring_view secret) {
  size_t offset = 0;
  std::string magic;
  std::uint32_t version = 0;
  std::uint32_t iterations = 0;
  std::uint32_t salt_size = 0;
  if (!ReadString(package, &offset, &magic) || magic != fp::WideToUtf8(kPackageMagic) ||
      !ReadU32(package, &offset, &version) || version != kPackageVersion ||
      !ReadU32(package, &offset, &iterations) || iterations != kPbkdf2Iterations ||
      !ReadU32(package, &offset, &salt_size) || salt_size != 16 ||
      offset + salt_size > package.size()) {
    return std::nullopt;
  }
  std::vector<std::uint8_t> salt(package.begin() + static_cast<std::ptrdiff_t>(offset),
                                 package.begin() + static_cast<std::ptrdiff_t>(offset + salt_size));
  offset += salt_size;
  std::uint32_t nonce_size = 0;
  if (!ReadU32(package, &offset, &nonce_size) || nonce_size != 12 ||
      offset + nonce_size > package.size()) {
    return std::nullopt;
  }
  std::vector<std::uint8_t> nonce(package.begin() + static_cast<std::ptrdiff_t>(offset),
                                  package.begin() + static_cast<std::ptrdiff_t>(offset + nonce_size));
  offset += nonce_size;
  std::uint32_t tag_size = 0;
  if (!ReadU32(package, &offset, &tag_size) || tag_size != 16 ||
      offset + tag_size > package.size()) {
    return std::nullopt;
  }
  std::vector<std::uint8_t> tag(package.begin() + static_cast<std::ptrdiff_t>(offset),
                                package.begin() + static_cast<std::ptrdiff_t>(offset + tag_size));
  offset += tag_size;
  std::uint64_t cipher_size = 0;
  if (!ReadU64(package, &offset, &cipher_size) || cipher_size > kMaxPackageFileBytes ||
      offset + cipher_size != package.size()) {
    return std::nullopt;
  }
  std::vector<std::uint8_t> cipher(package.begin() + static_cast<std::ptrdiff_t>(offset),
                                   package.end());

  std::vector<std::uint8_t> key;
  if (secret.empty() || !DeriveAesKey(secret, salt, &key)) {
    return std::nullopt;
  }

  BCRYPT_ALG_HANDLE alg = nullptr;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) {
    return std::nullopt;
  }
  const wchar_t chaining[] = BCRYPT_CHAIN_MODE_GCM;
  if (BCryptSetProperty(alg,
                        BCRYPT_CHAINING_MODE,
                        reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(chaining)),
                        static_cast<ULONG>(sizeof(chaining)),
                        0) != 0) {
    BCryptCloseAlgorithmProvider(alg, 0);
    return std::nullopt;
  }
  BCRYPT_KEY_HANDLE aes_key = nullptr;
  if (BCryptGenerateSymmetricKey(alg,
                                 &aes_key,
                                 nullptr,
                                 0,
                                 key.data(),
                                 static_cast<ULONG>(key.size()),
                                 0) != 0) {
    BCryptCloseAlgorithmProvider(alg, 0);
    return std::nullopt;
  }

  std::vector<std::uint8_t> plain(cipher.size());
  BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth{};
  BCRYPT_INIT_AUTH_MODE_INFO(auth);
  auth.pbNonce = nonce.data();
  auth.cbNonce = static_cast<ULONG>(nonce.size());
  auth.pbTag = tag.data();
  auth.cbTag = static_cast<ULONG>(tag.size());

  ULONG bytes_done = 0;
  const NTSTATUS status = BCryptDecrypt(aes_key,
                                        cipher.data(),
                                        static_cast<ULONG>(cipher.size()),
                                        &auth,
                                        nullptr,
                                        0,
                                        plain.data(),
                                        static_cast<ULONG>(plain.size()),
                                        &bytes_done,
                                        0);
  BCryptDestroyKey(aes_key);
  BCryptCloseAlgorithmProvider(alg, 0);
  if (status != 0 || bytes_done != plain.size()) {
    return std::nullopt;
  }
  return plain;
}

std::optional<std::vector<std::uint8_t>> BuildEncryptedPackage(const SyncConfig& config) {
  const auto plain_package = CollectPlainPackage(config);
  if (!plain_package) {
    return std::nullopt;
  }
  return EncryptPackageBytes(SerializePlainPackage(*plain_package), config.encryption_secret);
}

bool RestorePlainPackage(const PlainPackage& package) {
  if (package.metadata.has_clipboard) {
    WriteClipboardText(package.clipboard_text);
  }

  if (!package.metadata.has_user_data) {
    return true;
  }

  for (const auto& entry : package.entries) {
    if (entry.path.starts_with("settings/")) {
      if (entry.path != "settings/settings.ini") {
        continue;
      }
      if (!WriteSettingsFromPortablePackage(entry.data)) {
        return false;
      }
      continue;
    }
    if (entry.path.starts_with("rime/")) {
      const std::string_view relative(entry.path.data() + 5, entry.path.size() - 5);
      const auto destination = SafeDestination(DefaultRimeUserDataPath(), relative);
      if (!destination || IsExcludedRimeFile(std::filesystem::path(fp::Utf8ToWideStrict(relative)))) {
        continue;
      }
      if (!WriteBinaryFile(*destination, entry.data)) {
        return false;
      }
    }
  }
  return true;
}

std::optional<PlainPackage> DecryptFileToPlainPackage(const std::filesystem::path& path,
                                                      const SyncConfig& config) {
  std::vector<std::uint8_t> encrypted;
  if (!ReadBinaryFile(path, &encrypted, kMaxPackageFileBytes)) {
    return std::nullopt;
  }
  const auto plain = DecryptPackageBytes(encrypted, config.encryption_secret);
  if (!plain) {
    return std::nullopt;
  }
  return DeserializePlainPackage(*plain);
}

struct HttpResult {
  bool success = false;
  DWORD status = 0;
  std::vector<std::uint8_t> body;
  std::wstring error;
};

struct ParsedUrl {
  std::wstring host;
  INTERNET_PORT port = 0;
  std::wstring path;
  bool secure = true;
};

bool PlainHttpDevExceptionEnabled() {
  wchar_t buffer[16]{};
  const DWORD length = GetEnvironmentVariableW(L"FLUENT_PINYIN_SYNC_ALLOW_HTTP_LOCAL",
                                               buffer,
                                               static_cast<DWORD>(std::size(buffer)));
  if (length == 0 || length >= std::size(buffer)) {
    return false;
  }
  const std::wstring value = fp::ToLowerInvariant(std::wstring(buffer, length));
  return value == L"1" || value == L"true" || value == L"yes";
}

bool IsLoopbackHost(std::wstring_view host) {
  const std::wstring normalized = fp::ToLowerInvariant(std::wstring(host));
  return normalized == L"localhost" || normalized == L"127.0.0.1" ||
         normalized == L"::1" || normalized == L"[::1]";
}

std::optional<ParsedUrl> ParseUrl(std::wstring_view url) {
  std::wstring copy(url);
  URL_COMPONENTSW components{};
  components.dwStructSize = sizeof(components);
  components.dwSchemeLength = static_cast<DWORD>(-1);
  components.dwHostNameLength = static_cast<DWORD>(-1);
  components.dwUrlPathLength = static_cast<DWORD>(-1);
  components.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(copy.c_str(), static_cast<DWORD>(copy.size()), 0, &components)) {
    return std::nullopt;
  }
  ParsedUrl parsed;
  parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
  parsed.port = components.nPort;
  parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
  if (components.nScheme != INTERNET_SCHEME_HTTP &&
      components.nScheme != INTERNET_SCHEME_HTTPS) {
    return std::nullopt;
  }
  parsed.path.assign(components.lpszUrlPath, components.dwUrlPathLength);
  if (components.dwExtraInfoLength > 0) {
    parsed.path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
  }
  if (parsed.path.empty()) {
    parsed.path = L"/";
  }
  return parsed;
}

bool AllowsRemoteTransport(const ParsedUrl& url) {
  return url.secure || (PlainHttpDevExceptionEnabled() && IsLoopbackHost(url.host));
}

HttpResult HttpRequest(std::wstring_view method,
                       const ParsedUrl& url,
                       const std::vector<std::uint8_t>& body,
                       const std::vector<std::wstring>& headers,
                       std::wstring_view username = L"",
                       std::wstring_view password = L"") {
  HttpResult result;
  HINTERNET session = WinHttpOpen(L"FluentPinyin Sync/1.0",
                                  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS,
                                  0);
  if (session == nullptr) {
    result.error = L"无法初始化 WinHTTP。";
    return result;
  }
  HINTERNET connect = WinHttpConnect(session, url.host.c_str(), url.port, 0);
  if (connect == nullptr) {
    result.error = L"无法连接同步服务器。";
    WinHttpCloseHandle(session);
    return result;
  }
  HINTERNET request = WinHttpOpenRequest(connect,
                                         std::wstring(method).c_str(),
                                         url.path.c_str(),
                                         nullptr,
                                         WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         url.secure ? WINHTTP_FLAG_SECURE : 0);
  if (request == nullptr) {
    result.error = L"无法创建同步请求。";
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
  }
  DWORD timeout = 30000;
  WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
  WinHttpSetOption(request, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
  WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

  if (!username.empty()) {
    WinHttpSetCredentials(request,
                          WINHTTP_AUTH_TARGET_SERVER,
                          WINHTTP_AUTH_SCHEME_BASIC,
                          std::wstring(username).c_str(),
                          std::wstring(password).c_str(),
                          nullptr);
  }

  std::wstring header_text;
  for (const auto& header : headers) {
    header_text += header;
    header_text += L"\r\n";
  }
  const BOOL sent = WinHttpSendRequest(request,
                                       header_text.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                                                           : header_text.c_str(),
                                       header_text.empty() ? 0 : static_cast<DWORD>(header_text.size()),
                                       body.empty() ? WINHTTP_NO_REQUEST_DATA
                                                    : const_cast<std::uint8_t*>(body.data()),
                                       static_cast<DWORD>(body.size()),
                                       static_cast<DWORD>(body.size()),
                                       0);
  if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
    result.error = L"同步请求发送失败。";
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
  }

  DWORD status = 0;
  DWORD status_size = sizeof(status);
  WinHttpQueryHeaders(request,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX,
                      &status,
                      &status_size,
                      WINHTTP_NO_HEADER_INDEX);
  result.status = status;
  DWORD available = 0;
  while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
    const size_t old_size = result.body.size();
    result.body.resize(old_size + available);
    DWORD read = 0;
    if (!WinHttpReadData(request, result.body.data() + old_size, available, &read)) {
      result.error = L"读取同步响应失败。";
      break;
    }
    result.body.resize(old_size + read);
  }
  result.success = status >= 200 && status < 300;
  if (!result.success && result.error.empty()) {
    result.error = L"同步服务器返回 HTTP " + std::to_wstring(status) + L"。";
  }
  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
  return result;
}

std::wstring JoinRemotePath(std::wstring_view base_url, std::wstring_view key) {
  std::wstring result = fp::TrimWhitespace(base_url);
  while (!result.empty() && result.back() == L'/') {
    result.pop_back();
  }
  std::wstring key_text(key);
  while (!key_text.empty() && (key_text.front() == L'/' || key_text.front() == L'\\')) {
    key_text.erase(key_text.begin());
  }
  std::replace(key_text.begin(), key_text.end(), L'\\', L'/');
  if (key_text.empty()) {
    return result;
  }
  return result + L"/" + key_text;
}

std::wstring BasicAuthHeader(std::wstring_view username, std::wstring_view password) {
  const std::string auth = fp::WideToUtf8(username) + ":" + fp::WideToUtf8(password);
  DWORD required = 0;
  CryptBinaryToStringA(reinterpret_cast<const BYTE*>(auth.data()),
                       static_cast<DWORD>(auth.size()),
                       CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                       nullptr,
                       &required);
  std::string encoded(required, '\0');
  if (required == 0 ||
      !CryptBinaryToStringA(reinterpret_cast<const BYTE*>(auth.data()),
                            static_cast<DWORD>(auth.size()),
                            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                            encoded.data(),
                            &required)) {
    return {};
  }
  encoded.resize(required > 0 && encoded[required - 1] == '\0' ? required - 1 : required);
  return L"Authorization: Basic " + fp::Utf8ToWideStrict(encoded);
}

SyncResult WebDavUpload(const SyncConfig& config, const std::vector<std::uint8_t>& package) {
  const std::wstring url_text = JoinRemotePath(config.webdav_url, config.object_key);
  const auto url = ParseUrl(url_text);
  if (!url) {
    return {false, L"WebDAV 地址无效。"};
  }
  if (!AllowsRemoteTransport(*url)) {
    return {false, L"WebDAV 地址必须使用 HTTPS。"};
  }
  std::vector<std::wstring> headers{
      L"Content-Type: application/octet-stream",
      L"User-Agent: FluentPinyin Sync",
  };
  if (!config.webdav_username.empty()) {
    headers.push_back(BasicAuthHeader(config.webdav_username, config.webdav_password));
  }
  const auto result = HttpRequest(L"PUT", *url, package, headers);
  if (!result.success) {
    return {false, result.error.empty() ? L"WebDAV 上传失败。" : result.error};
  }
  return {true, L"已上传加密同步包到 WebDAV。"};
}

std::optional<std::vector<std::uint8_t>> WebDavDownload(const SyncConfig& config,
                                                       std::wstring* error) {
  const std::wstring url_text = JoinRemotePath(config.webdav_url, config.object_key);
  const auto url = ParseUrl(url_text);
  if (!url) {
    *error = L"WebDAV 地址无效。";
    return std::nullopt;
  }
  if (!AllowsRemoteTransport(*url)) {
    *error = L"WebDAV 地址必须使用 HTTPS。";
    return std::nullopt;
  }
  std::vector<std::wstring> headers{L"User-Agent: FluentPinyin Sync"};
  if (!config.webdav_username.empty()) {
    headers.push_back(BasicAuthHeader(config.webdav_username, config.webdav_password));
  }
  const auto result = HttpRequest(L"GET", *url, {}, headers);
  if (!result.success) {
    *error = result.error.empty() ? L"WebDAV 下载失败。" : result.error;
    return std::nullopt;
  }
  return result.body;
}

struct S3Request {
  std::wstring method;
  std::wstring url;
  std::vector<std::wstring> headers;
};

std::optional<S3Request> BuildS3Request(const SyncConfig& config,
                                        std::wstring_view method,
                                        const std::vector<std::uint8_t>& body) {
  std::wstring endpoint = fp::TrimWhitespace(config.object_endpoint);
  while (!endpoint.empty() && endpoint.back() == L'/') {
    endpoint.pop_back();
  }
  std::wstring key = fp::TrimWhitespace(config.object_key);
  while (!key.empty() && (key.front() == L'/' || key.front() == L'\\')) {
    key.erase(key.begin());
  }
  std::replace(key.begin(), key.end(), L'\\', L'/');
  if (endpoint.empty() || config.object_bucket.empty() || key.empty() ||
      config.object_access_key.empty() || config.object_secret_key.empty()) {
    return std::nullopt;
  }

  const std::wstring encoded_bucket = UrlEncodePathSegment(config.object_bucket);
  const std::string key_utf8 = fp::WideToUtf8(key);
  const std::wstring encoded_key = PercentEncodeUtf8(key_utf8, false);
  const std::wstring path = L"/" + encoded_bucket + L"/" + encoded_key;
  const std::wstring url_text = endpoint + path;
  const auto url = ParseUrl(url_text);
  if (!url) {
    return std::nullopt;
  }
  if (!AllowsRemoteTransport(*url)) {
    return std::nullopt;
  }

  SYSTEMTIME utc{};
  GetSystemTime(&utc);
  std::ostringstream amz_date;
  amz_date << std::setfill('0') << std::setw(4) << utc.wYear << std::setw(2) << utc.wMonth
           << std::setw(2) << utc.wDay << "T" << std::setw(2) << utc.wHour
           << std::setw(2) << utc.wMinute << std::setw(2) << utc.wSecond << "Z";
  std::ostringstream short_date;
  short_date << std::setfill('0') << std::setw(4) << utc.wYear << std::setw(2) << utc.wMonth
             << std::setw(2) << utc.wDay;

  const auto payload_hash = Sha256(std::string_view(
      reinterpret_cast<const char*>(body.data()), body.size()));
  if (!payload_hash) {
    return std::nullopt;
  }
  const std::string payload_hex = HexLower(*payload_hash);
  const std::string host = fp::WideToUtf8(url->host);
  const std::string region = fp::WideToUtf8(config.object_region.empty() ? L"auto" : config.object_region);
  const std::string amz_date_text = amz_date.str();
  const std::string short_date_text = short_date.str();
  const std::string canonical_uri = fp::WideToUtf8(url->path);
  const std::string canonical_headers =
      "host:" + host + "\n" +
      "x-amz-content-sha256:" + payload_hex + "\n" +
      "x-amz-date:" + amz_date_text + "\n";
  const std::string signed_headers = "host;x-amz-content-sha256;x-amz-date";
  const std::string canonical_request =
      fp::WideToUtf8(method) + "\n" + canonical_uri + "\n\n" + canonical_headers + "\n" +
      signed_headers + "\n" + payload_hex;
  const auto canonical_hash = Sha256(canonical_request);
  if (!canonical_hash) {
    return std::nullopt;
  }
  const std::string scope = short_date_text + "/" + region + "/s3/aws4_request";
  const std::string string_to_sign =
      "AWS4-HMAC-SHA256\n" + amz_date_text + "\n" + scope + "\n" + HexLower(*canonical_hash);
  const auto signing_key = SigningKey(fp::WideToUtf8(config.object_secret_key),
                                      short_date_text,
                                      region,
                                      "s3");
  const auto signature = HmacSha256(signing_key, string_to_sign);
  if (!signature) {
    return std::nullopt;
  }
  const std::string authorization =
      "AWS4-HMAC-SHA256 Credential=" + fp::WideToUtf8(config.object_access_key) + "/" + scope +
      ", SignedHeaders=" + signed_headers + ", Signature=" + HexLower(*signature);

  S3Request request;
  request.method = std::wstring(method);
  request.url = url_text;
  request.headers = {
      L"Host: " + url->host,
      L"x-amz-content-sha256: " + fp::Utf8ToWideStrict(payload_hex),
      L"x-amz-date: " + fp::Utf8ToWideStrict(amz_date_text),
      L"Authorization: " + fp::Utf8ToWideStrict(authorization),
      L"Content-Type: application/octet-stream",
      L"User-Agent: FluentPinyin Sync",
  };
  return request;
}

SyncResult S3Upload(const SyncConfig& config, const std::vector<std::uint8_t>& package) {
  const auto request = BuildS3Request(config, L"PUT", package);
  if (!request) {
    return {false, L"对象存储配置无效。"};
  }
  const auto url = ParseUrl(request->url);
  if (!url) {
    return {false, L"对象存储地址无效。"};
  }
  const auto result = HttpRequest(request->method, *url, package, request->headers);
  if (!result.success) {
    return {false, result.error.empty() ? L"对象存储上传失败。" : result.error};
  }
  return {true, L"已上传加密同步包到对象存储。"};
}

std::optional<std::vector<std::uint8_t>> S3Download(const SyncConfig& config,
                                                   std::wstring* error) {
  const auto request = BuildS3Request(config, L"GET", {});
  if (!request) {
    *error = L"对象存储配置无效。";
    return std::nullopt;
  }
  const auto url = ParseUrl(request->url);
  if (!url) {
    *error = L"对象存储地址无效。";
    return std::nullopt;
  }
  const auto result = HttpRequest(request->method, *url, {}, request->headers);
  if (!result.success) {
    *error = result.error.empty() ? L"对象存储下载失败。" : result.error;
    return std::nullopt;
  }
  return result.body;
}

std::wstring ReadSecretSetting(const std::filesystem::path& settings_path,
                               std::wstring_view key) {
  const std::wstring protected_key = std::wstring(key) + L"_protected";
  const std::wstring protected_value = ReadSetting(settings_path, protected_key, L"");
  if (!protected_value.empty()) {
    if (const auto plain = UnprotectSecretText(protected_value)) {
      return *plain;
    }
  }
  return ReadSetting(settings_path, key, L"");
}

bool WriteSecretSetting(const std::filesystem::path& settings_path,
                        std::wstring_view key,
                        std::wstring_view value) {
  const auto protected_value = ProtectSecretText(value);
  if (!protected_value) {
    return WriteSetting(settings_path, key, value);
  }
  const bool ok = WriteSetting(settings_path, std::wstring(key) + L"_protected", *protected_value);
  WriteSetting(settings_path, key, L"");
  return ok;
}

std::wstring XmlEscape(std::wstring_view value) {
  std::wstring out;
  for (const wchar_t ch : value) {
    switch (ch) {
      case L'&':
        out += L"&amp;";
        break;
      case L'<':
        out += L"&lt;";
        break;
      case L'>':
        out += L"&gt;";
        break;
      case L'"':
        out += L"&quot;";
        break;
      case L'\'':
        out += L"&apos;";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  return out;
}

std::wstring IsoDurationMinutes(int minutes) {
  minutes = std::clamp(minutes,
                       fp::kMinSyncAutoIntervalMinutes,
                       fp::kMaxSyncAutoIntervalMinutes);
  return L"PT" + std::to_wstring(minutes) + L"M";
}

}  // namespace

std::filesystem::path DefaultSettingsPath() {
  return fp::GetSettingsPath();
}

std::filesystem::path DefaultRimeUserDataPath() {
  return fp::GetFpRoamingDataPath() / L"Rime";
}

std::filesystem::path DefaultBackupDirectory() {
  return fp::GetFpRoamingDataPath() / L"Backups";
}

std::wstring NormalizeProviderValue(std::wstring_view value) {
  const std::wstring normalized = fp::ToLowerInvariant(fp::TrimWhitespace(value));
  if (normalized == L"webdav") {
    return L"webdav";
  }
  return L"object";
}

std::wstring DefaultObjectKey() {
  return std::wstring(fp::kDefaultSyncObjectKey);
}

std::wstring NewTimestampedBackupName() {
  return L"fluent-pinyin-backup-" + TimestampText() + L".fpsync";
}

bool IsSecureRemoteUrl(std::wstring_view url) {
  const auto parsed = ParseUrl(fp::TrimWhitespace(url));
  return parsed.has_value() && AllowsRemoteTransport(*parsed);
}

std::wstring ReadSetting(const std::filesystem::path& settings_path,
                         std::wstring_view key,
                         std::wstring_view default_value) {
  fp::SettingsStore store(settings_path);
  return store.ReadString(key, default_value);
}

bool WriteSetting(const std::filesystem::path& settings_path,
                  std::wstring_view key,
                  std::wstring_view value) {
  fp::SettingsStore store(settings_path);
  return store.WriteString(key, value);
}

std::optional<std::wstring> ProtectSecretText(std::wstring_view plaintext) {
  const std::wstring text(plaintext);
  DATA_BLOB input{};
  input.pbData = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(text.data()));
  input.cbData = static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t));
  DATA_BLOB output{};
  if (!CryptProtectData(&input, L"FluentPinyin sync secret", nullptr, nullptr, nullptr, 0, &output)) {
    return std::nullopt;
  }
  DWORD required = 0;
  CryptBinaryToStringW(output.pbData,
                       output.cbData,
                       CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                       nullptr,
                       &required);
  std::wstring encoded(required, L'\0');
  const BOOL ok = required > 0 &&
                  CryptBinaryToStringW(output.pbData,
                                       output.cbData,
                                       CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                                       encoded.data(),
                                       &required);
  LocalFree(output.pbData);
  if (!ok) {
    return std::nullopt;
  }
  encoded.resize(required > 0 && encoded[required - 1] == L'\0' ? required - 1 : required);
  return encoded;
}

std::optional<std::wstring> UnprotectSecretText(std::wstring_view protected_text) {
  std::wstring text(protected_text);
  DWORD binary_size = 0;
  if (!CryptStringToBinaryW(text.c_str(),
                            static_cast<DWORD>(text.size()),
                            CRYPT_STRING_BASE64,
                            nullptr,
                            &binary_size,
                            nullptr,
                            nullptr) ||
      binary_size == 0) {
    return std::nullopt;
  }
  std::vector<BYTE> binary(binary_size);
  if (!CryptStringToBinaryW(text.c_str(),
                            static_cast<DWORD>(text.size()),
                            CRYPT_STRING_BASE64,
                            binary.data(),
                            &binary_size,
                            nullptr,
                            nullptr)) {
    return std::nullopt;
  }
  DATA_BLOB input{binary_size, binary.data()};
  DATA_BLOB output{};
  if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
    return std::nullopt;
  }
  std::wstring result(reinterpret_cast<wchar_t*>(output.pbData));
  LocalFree(output.pbData);
  return result;
}

SyncConfig LoadConfig(const std::filesystem::path& settings_path) {
  SyncConfig config;
  config.provider = NormalizeProviderValue(ReadSetting(settings_path,
                                                       fp::kSyncProviderSetting,
                                                       fp::kSyncProviderObject)) ==
                            std::wstring(fp::kSyncProviderWebDav)
                        ? SyncProvider::WebDav
                        : SyncProvider::ObjectStorage;
  const auto read_bool = [&](std::wstring_view key, bool default_value) {
    const std::wstring value =
        fp::ToLowerInvariant(ReadSetting(settings_path, key, default_value ? L"1" : L"0"));
    return value == L"1" || value == L"true" || value == L"yes";
  };
  const auto read_int = [&](std::wstring_view key, int default_value) {
    try {
      return std::stoi(ReadSetting(settings_path, key, std::to_wstring(default_value)));
    } catch (...) {
      return default_value;
    }
  };
  config.sync_clipboard = read_bool(fp::kSyncClipboardSetting, false);
  config.sync_user_data = read_bool(fp::kSyncUserDataSetting, false);
  config.auto_enabled = read_bool(fp::kSyncAutoEnabledSetting, false);
  config.auto_interval_minutes =
      std::clamp(read_int(fp::kSyncAutoIntervalMinutesSetting,
                          fp::kDefaultSyncAutoIntervalMinutes),
                 fp::kMinSyncAutoIntervalMinutes,
                 fp::kMaxSyncAutoIntervalMinutes);
  config.device_id = ReadSetting(settings_path, fp::kSyncDeviceIdSetting, DeviceName());
  config.object_endpoint =
      ReadSetting(settings_path, fp::kSyncObjectEndpointSetting, L"");
  config.object_bucket =
      ReadSetting(settings_path, fp::kSyncObjectBucketSetting, L"");
  config.object_region =
      ReadSetting(settings_path, fp::kSyncObjectRegionSetting, fp::kSyncObjectRegionAuto);
  config.object_access_key =
      ReadSetting(settings_path, fp::kSyncObjectAccessKeySetting, L"");
  config.object_secret_key =
      ReadSecretSetting(settings_path, fp::kSyncObjectSecretKeySetting);
  config.object_key =
      ReadSetting(settings_path, fp::kSyncObjectKeySetting, DefaultObjectKey());
  config.webdav_url = ReadSetting(settings_path, fp::kSyncWebDavUrlSetting, L"");
  config.webdav_username =
      ReadSetting(settings_path, fp::kSyncWebDavUsernameSetting, L"");
  config.webdav_password =
      ReadSecretSetting(settings_path, fp::kSyncWebDavPasswordSetting);
  config.encryption_secret =
      ReadSecretSetting(settings_path, fp::kSyncEncryptionSecretSetting);
  return config;
}

SyncResult ValidateRemoteConfig(const SyncConfig& config) {
  if (config.provider == SyncProvider::WebDav) {
    if (fp::TrimWhitespace(config.webdav_url).empty()) {
      return {false, L"请先配置 WebDAV 地址。"};
    }
    if (!IsSecureRemoteUrl(config.webdav_url)) {
      return {false, L"WebDAV 地址必须使用 HTTPS。"};
    }
    return {true, L""};
  }
  if (fp::TrimWhitespace(config.object_endpoint).empty() ||
      fp::TrimWhitespace(config.object_bucket).empty() ||
      fp::TrimWhitespace(config.object_access_key).empty() ||
      fp::TrimWhitespace(config.object_secret_key).empty()) {
    return {false, L"请先配置对象存储 Endpoint、Bucket、Access Key 和 Secret Key。"};
  }
  if (!IsSecureRemoteUrl(config.object_endpoint)) {
    return {false, L"对象存储 Endpoint 必须使用 HTTPS。"};
  }
  return {true, L""};
}

SyncResult ValidateCryptoConfig(const SyncConfig& config) {
  if (fp::TrimWhitespace(config.encryption_secret).size() < 8) {
    return {false, L"请先设置至少 8 个字符的同步加密口令。"};
  }
  if (!config.sync_clipboard && !config.sync_user_data) {
    return {false, L"请至少开启一种同步内容。"};
  }
  return {true, L""};
}

SyncResult CreateLocalBackup(const std::filesystem::path& package_path,
                             const SyncConfig& config) {
  const auto crypto = ValidateCryptoConfig(config);
  if (!crypto.success) {
    return crypto;
  }
  const auto package = BuildEncryptedPackage(config);
  if (!package) {
    return {false, L"创建加密备份包失败。"};
  }
  if (!WriteBinaryFile(package_path, *package)) {
    return {false, L"写入备份文件失败。"};
  }
  return {true, L"备份已保存：" + package_path.wstring()};
}

SyncResult RestoreLocalBackup(const std::filesystem::path& package_path,
                              const SyncConfig& config,
                              bool create_pre_restore_backup,
                              PackageMetadata* metadata) {
  if (fp::TrimWhitespace(config.encryption_secret).size() < 8) {
    return {false, L"请先输入用于解密的同步加密口令。"};
  }
  if (create_pre_restore_backup) {
    SyncConfig backup_config = config;
    backup_config.sync_clipboard = false;
    backup_config.sync_user_data = true;
    const auto backup_path = DefaultBackupDirectory() /
                             (L"before-restore-" + TimestampText() + L".fpsync");
    CreateLocalBackup(backup_path, backup_config);
  }
  const auto plain = DecryptFileToPlainPackage(package_path, config);
  if (!plain) {
    return {false, L"解密或读取备份包失败，请检查加密口令。"};
  }
  if (!RestorePlainPackage(*plain)) {
    return {false, L"恢复用户数据失败。"};
  }
  if (metadata != nullptr) {
    *metadata = plain->metadata;
  }
  return {true, L"恢复完成，输入配置将在刷新后生效。"};
}

SyncResult UploadNow(const SyncConfig& config) {
  const auto crypto = ValidateCryptoConfig(config);
  if (!crypto.success) {
    return crypto;
  }
  const auto remote = ValidateRemoteConfig(config);
  if (!remote.success) {
    return remote;
  }
  const auto package = BuildEncryptedPackage(config);
  if (!package) {
    return {false, L"创建加密同步包失败。"};
  }
  if (config.provider == SyncProvider::WebDav) {
    return WebDavUpload(config, *package);
  }
  return S3Upload(config, *package);
}

SyncResult DownloadNow(const SyncConfig& config,
                       bool create_pre_restore_backup,
                       PackageMetadata* metadata) {
  if (fp::TrimWhitespace(config.encryption_secret).size() < 8) {
    return {false, L"请先输入用于解密的同步加密口令。"};
  }
  const auto remote = ValidateRemoteConfig(config);
  if (!remote.success) {
    return remote;
  }
  std::wstring error;
  const auto package = config.provider == SyncProvider::WebDav ? WebDavDownload(config, &error)
                                                               : S3Download(config, &error);
  if (!package) {
    return {false, error.empty() ? L"下载同步包失败。" : error};
  }
  const auto temp_path = fp::GetFpLocalDataPath() / L"Temp" /
                         (L"download-" + TimestampText() + L".fpsync");
  if (!WriteBinaryFile(temp_path, *package)) {
    return {false, L"写入临时同步包失败。"};
  }
  return RestoreLocalBackup(temp_path, config, create_pre_restore_backup, metadata);
}

SyncResult RunAutoSync(const std::filesystem::path& settings_path) {
  const auto config = LoadConfig(settings_path);
  if (!config.auto_enabled) {
    return {true, L"自动同步未开启。"};
  }
  const auto crypto = ValidateCryptoConfig(config);
  if (!crypto.success) {
    return crypto;
  }
  const auto remote_config = ValidateRemoteConfig(config);
  if (!remote_config.success) {
    return remote_config;
  }

  std::uint64_t last_remote_unix = 0;
  try {
    last_remote_unix = std::stoull(ReadSetting(settings_path, L"sync_last_auto_remote_unix", L"0"));
  } catch (...) {
    last_remote_unix = 0;
  }

  std::wstring download_error;
  const auto remote_package =
      config.provider == SyncProvider::WebDav ? WebDavDownload(config, &download_error)
                                              : S3Download(config, &download_error);
  if (remote_package) {
    const auto plain_bytes = DecryptPackageBytes(*remote_package, config.encryption_secret);
    const auto plain = plain_bytes ? DeserializePlainPackage(*plain_bytes) : std::nullopt;
    if (!plain) {
      return {false, L"自动同步下载到的远端包无法解密。"};
    }
    if (plain->metadata.created_unix > last_remote_unix) {
      WriteSetting(settings_path,
                   L"sync_last_auto_remote_unix",
                   std::to_wstring(plain->metadata.created_unix));
      if (plain->metadata.device_id != config.device_id) {
        SyncConfig backup_config = config;
        backup_config.sync_clipboard = false;
        backup_config.sync_user_data = true;
        const auto backup_path = DefaultBackupDirectory() /
                                 (L"before-auto-sync-" + TimestampText() + L".fpsync");
        CreateLocalBackup(backup_path, backup_config);
        if (!RestorePlainPackage(*plain)) {
          return {false, L"自动同步恢复远端数据失败。"};
        }
      }
    }
  } else if (download_error.find(L"HTTP 404") == std::wstring::npos) {
    return {false, download_error.empty() ? L"自动同步下载远端包失败。" : download_error};
  }

  auto result = UploadNow(LoadConfig(settings_path));
  if (!result.success) {
    return result;
  }
  WriteSetting(settings_path, L"sync_last_auto_upload_unix", std::to_wstring(UnixNow()));
  return result;
}

SyncResult InstallScheduledSync(const std::filesystem::path& sync_exe_path,
                                int interval_minutes) {
  interval_minutes = std::clamp(interval_minutes,
                                fp::kMinSyncAutoIntervalMinutes,
                                fp::kMaxSyncAutoIntervalMinutes);
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool co_initialized = SUCCEEDED(hr);
  if (hr == RPC_E_CHANGED_MODE) {
    hr = S_OK;
  }
  if (FAILED(hr)) {
    return {false, L"初始化计划任务组件失败。"};
  }

  ITaskService* service = nullptr;
  hr = CoCreateInstance(CLSID_TaskScheduler,
                        nullptr,
                        CLSCTX_INPROC_SERVER,
                        IID_ITaskService,
                        reinterpret_cast<void**>(&service));
  if (FAILED(hr) || service == nullptr) {
    if (co_initialized) {
      CoUninitialize();
    }
    return {false, L"无法打开计划任务服务。"};
  }
  hr = service->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
  if (FAILED(hr)) {
    service->Release();
    if (co_initialized) {
      CoUninitialize();
    }
    return {false, L"连接计划任务服务失败。"};
  }

  ITaskFolder* root = nullptr;
  hr = service->GetFolder(_bstr_t(L"\\"), &root);
  if (FAILED(hr) || root == nullptr) {
    service->Release();
    if (co_initialized) {
      CoUninitialize();
    }
    return {false, L"无法打开计划任务根目录。"};
  }

  ITaskDefinition* task = nullptr;
  hr = service->NewTask(0, &task);
  if (FAILED(hr) || task == nullptr) {
    root->Release();
    service->Release();
    if (co_initialized) {
      CoUninitialize();
    }
    return {false, L"创建计划任务失败。"};
  }

  IRegistrationInfo* registration = nullptr;
  if (SUCCEEDED(task->get_RegistrationInfo(&registration)) && registration != nullptr) {
    registration->put_Author(_bstr_t(L"FluentPinyin"));
    registration->put_Description(_bstr_t(L"定时上传流畅拼音加密同步包。"));
    registration->Release();
  }
  IPrincipal* principal = nullptr;
  if (SUCCEEDED(task->get_Principal(&principal)) && principal != nullptr) {
    principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
    principal->put_RunLevel(TASK_RUNLEVEL_LUA);
    principal->Release();
  }
  ITaskSettings* settings = nullptr;
  if (SUCCEEDED(task->get_Settings(&settings)) && settings != nullptr) {
    settings->put_StartWhenAvailable(VARIANT_TRUE);
    settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
    settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
    settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW);
    settings->put_ExecutionTimeLimit(_bstr_t(L"PT5M"));
    settings->Release();
  }

  ITriggerCollection* triggers = nullptr;
  hr = task->get_Triggers(&triggers);
  if (SUCCEEDED(hr) && triggers != nullptr) {
    ITrigger* trigger_base = nullptr;
    hr = triggers->Create(TASK_TRIGGER_TIME, &trigger_base);
    if (SUCCEEDED(hr) && trigger_base != nullptr) {
      ITimeTrigger* trigger = nullptr;
      if (SUCCEEDED(trigger_base->QueryInterface(IID_ITimeTrigger,
                                                 reinterpret_cast<void**>(&trigger))) &&
          trigger != nullptr) {
        SYSTEMTIME local{};
        GetLocalTime(&local);
        std::wstringstream start;
        start << std::setfill(L'0') << std::setw(4) << local.wYear << L"-" << std::setw(2)
              << local.wMonth << L"-" << std::setw(2) << local.wDay << L"T" << std::setw(2)
              << local.wHour << L":" << std::setw(2) << local.wMinute << L":" << std::setw(2)
              << local.wSecond;
        trigger->put_StartBoundary(_bstr_t(start.str().c_str()));
        IRepetitionPattern* repetition = nullptr;
        if (SUCCEEDED(trigger->get_Repetition(&repetition)) && repetition != nullptr) {
          repetition->put_Interval(_bstr_t(IsoDurationMinutes(interval_minutes).c_str()));
          repetition->put_Duration(_bstr_t(L"P1D"));
          repetition->Release();
        }
        trigger->Release();
      }
      trigger_base->Release();
    }
    triggers->Release();
  }
  if (FAILED(hr)) {
    task->Release();
    root->Release();
    service->Release();
    if (co_initialized) {
      CoUninitialize();
    }
    return {false, L"创建计划任务触发器失败。"};
  }

  IActionCollection* actions = nullptr;
  hr = task->get_Actions(&actions);
  if (SUCCEEDED(hr) && actions != nullptr) {
    IAction* action_base = nullptr;
    hr = actions->Create(TASK_ACTION_EXEC, &action_base);
    if (SUCCEEDED(hr) && action_base != nullptr) {
      IExecAction* action = nullptr;
      if (SUCCEEDED(action_base->QueryInterface(IID_IExecAction,
                                                reinterpret_cast<void**>(&action))) &&
          action != nullptr) {
        action->put_Path(_bstr_t(sync_exe_path.wstring().c_str()));
        action->put_Arguments(_bstr_t(L"--auto-sync"));
        action->put_WorkingDirectory(_bstr_t(sync_exe_path.parent_path().wstring().c_str()));
        action->Release();
      }
      action_base->Release();
    }
    actions->Release();
  }
  if (FAILED(hr)) {
    task->Release();
    root->Release();
    service->Release();
    if (co_initialized) {
      CoUninitialize();
    }
    return {false, L"创建计划任务操作失败。"};
  }

  IRegisteredTask* registered_task = nullptr;
  hr = root->RegisterTaskDefinition(_bstr_t(kTaskName.data()),
                                    task,
                                    TASK_CREATE_OR_UPDATE,
                                    _variant_t(),
                                    _variant_t(),
                                    TASK_LOGON_INTERACTIVE_TOKEN,
                                    _variant_t(L""),
                                    &registered_task);
  if (registered_task != nullptr) {
    registered_task->Release();
  }
  task->Release();
  root->Release();
  service->Release();
  if (co_initialized) {
    CoUninitialize();
  }
  if (FAILED(hr)) {
    return {false, L"注册计划任务失败。"};
  }
  return {true, L"已启用定时自动同步。"};
}

SyncResult RemoveScheduledSync() {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool co_initialized = SUCCEEDED(hr);
  if (hr == RPC_E_CHANGED_MODE) {
    hr = S_OK;
  }
  if (FAILED(hr)) {
    return {false, L"初始化计划任务组件失败。"};
  }
  ITaskService* service = nullptr;
  hr = CoCreateInstance(CLSID_TaskScheduler,
                        nullptr,
                        CLSCTX_INPROC_SERVER,
                        IID_ITaskService,
                        reinterpret_cast<void**>(&service));
  if (FAILED(hr) || service == nullptr) {
    if (co_initialized) {
      CoUninitialize();
    }
    return {false, L"无法打开计划任务服务。"};
  }
  hr = service->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
  ITaskFolder* root = nullptr;
  if (SUCCEEDED(hr)) {
    hr = service->GetFolder(_bstr_t(L"\\"), &root);
  }
  if (SUCCEEDED(hr) && root != nullptr) {
    hr = root->DeleteTask(_bstr_t(kTaskName.data()), 0);
    root->Release();
  }
  service->Release();
  if (co_initialized) {
    CoUninitialize();
  }
  if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
    return {false, L"关闭计划任务失败。"};
  }
  return {true, L"已关闭定时自动同步。"};
}

}  // namespace fp::sync
