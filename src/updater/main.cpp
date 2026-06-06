#include "common/constants.h"
#include "common/encoding.h"
#include "common/logging.h"
#include "common/path_utils.h"

#include <windows.h>
#include <shellapi.h>
#include <softpub.h>
#include <wininet.h>
#include <wintrust.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct ReleaseInfo {
  std::string tag;
  std::string asset_name;
  std::string asset_url;
};

constexpr DWORD kHttpConnectTimeoutMs = 10000;
constexpr DWORD kHttpSendTimeoutMs = 15000;
constexpr DWORD kHttpReceiveTimeoutMs = 30000;

bool SetInternetTimeouts(HINTERNET handle) {
  if (handle == nullptr) {
    return false;
  }
  DWORD connect_timeout = kHttpConnectTimeoutMs;
  DWORD send_timeout = kHttpSendTimeoutMs;
  DWORD receive_timeout = kHttpReceiveTimeoutMs;
  const BOOL connect_ok = InternetSetOptionW(handle,
                                             INTERNET_OPTION_CONNECT_TIMEOUT,
                                             &connect_timeout,
                                             sizeof(connect_timeout));
  const BOOL send_ok = InternetSetOptionW(handle,
                                          INTERNET_OPTION_SEND_TIMEOUT,
                                          &send_timeout,
                                          sizeof(send_timeout));
  const BOOL receive_ok = InternetSetOptionW(handle,
                                             INTERNET_OPTION_RECEIVE_TIMEOUT,
                                             &receive_timeout,
                                             sizeof(receive_timeout));
  return connect_ok != FALSE && send_ok != FALSE && receive_ok != FALSE;
}

std::optional<std::string> HttpGet(const wchar_t* url) {
  HINTERNET internet = InternetOpenW(L"FluentPinyin updater",
                                     INTERNET_OPEN_TYPE_PRECONFIG,
                                     nullptr,
                                     nullptr,
                                     0);
  if (internet == nullptr) {
    return std::nullopt;
  }
  SetInternetTimeouts(internet);

  HINTERNET request = InternetOpenUrlW(internet,
                                       url,
                                       L"Accept: application/vnd.github+json\r\n"
                                       L"User-Agent: FluentPinyin\r\n",
                                       0,
                                       INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE,
                                       0);
  if (request == nullptr) {
    InternetCloseHandle(internet);
    return std::nullopt;
  }
  SetInternetTimeouts(request);

  std::string body;
  char buffer[8192]{};
  DWORD bytes_read = 0;
  while (InternetReadFile(request, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
    body.append(buffer, buffer + bytes_read);
  }

  InternetCloseHandle(request);
  InternetCloseHandle(internet);
  return body;
}

bool DownloadFile(const std::wstring& url, const std::filesystem::path& target_path) {
  fp::EnsureDirectory(target_path.parent_path());

  HINTERNET internet = InternetOpenW(L"FluentPinyin updater",
                                     INTERNET_OPEN_TYPE_PRECONFIG,
                                     nullptr,
                                     nullptr,
                                     0);
  if (internet == nullptr) {
    return false;
  }
  SetInternetTimeouts(internet);

  HINTERNET request = InternetOpenUrlW(internet,
                                       url.c_str(),
                                       L"User-Agent: FluentPinyin\r\n",
                                       0,
                                       INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE,
                                       0);
  if (request == nullptr) {
    InternetCloseHandle(internet);
    return false;
  }
  SetInternetTimeouts(request);

  std::ofstream file(target_path, std::ios::binary);
  if (!file) {
    InternetCloseHandle(request);
    InternetCloseHandle(internet);
    return false;
  }

  char buffer[8192]{};
  DWORD bytes_read = 0;
  while (InternetReadFile(request, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
    file.write(buffer, static_cast<std::streamsize>(bytes_read));
    if (!file) {
      InternetCloseHandle(request);
      InternetCloseHandle(internet);
      return false;
    }
  }

  InternetCloseHandle(request);
  InternetCloseHandle(internet);
  return file.good();
}

struct JsonValueSpan {
  size_t start = 0;
  size_t end = 0;
};

bool IsJsonWhitespace(char value) {
  return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

size_t SkipJsonWhitespace(std::string_view json, size_t position) {
  while (position < json.size() && IsJsonWhitespace(json[position])) {
    ++position;
  }
  return position;
}

int JsonHexDigit(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return 10 + value - 'a';
  }
  if (value >= 'A' && value <= 'F') {
    return 10 + value - 'A';
  }
  return -1;
}

bool ReadJsonHexQuad(std::string_view json, size_t* position, std::uint32_t* value) {
  if (*position + 4 > json.size()) {
    return false;
  }
  std::uint32_t result = 0;
  for (int count = 0; count < 4; ++count) {
    const int digit = JsonHexDigit(json[*position + count]);
    if (digit < 0) {
      return false;
    }
    result = (result << 4) | static_cast<std::uint32_t>(digit);
  }
  *position += 4;
  *value = result;
  return true;
}

bool AppendUtf8CodePoint(std::uint32_t code_point, std::string* output) {
  if (code_point <= 0x7F) {
    output->push_back(static_cast<char>(code_point));
    return true;
  }
  if (code_point <= 0x7FF) {
    output->push_back(static_cast<char>(0xC0 | (code_point >> 6)));
    output->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    return true;
  }
  if (code_point >= 0xD800 && code_point <= 0xDFFF) {
    return false;
  }
  if (code_point <= 0xFFFF) {
    output->push_back(static_cast<char>(0xE0 | (code_point >> 12)));
    output->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    output->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    return true;
  }
  if (code_point <= 0x10FFFF) {
    output->push_back(static_cast<char>(0xF0 | (code_point >> 18)));
    output->push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
    output->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    output->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    return true;
  }
  return false;
}

bool ParseJsonString(std::string_view json, size_t* position, std::string* value) {
  if (*position >= json.size() || json[*position] != '"') {
    return false;
  }
  value->clear();
  ++*position;

  while (*position < json.size()) {
    const unsigned char ch = static_cast<unsigned char>(json[(*position)++]);
    if (ch == '"') {
      return true;
    }
    if (ch < 0x20) {
      return false;
    }
    if (ch != '\\') {
      value->push_back(static_cast<char>(ch));
      continue;
    }
    if (*position >= json.size()) {
      return false;
    }

    const char escape = json[(*position)++];
    switch (escape) {
      case '"':
      case '\\':
      case '/':
        value->push_back(escape);
        break;
      case 'b':
        value->push_back('\b');
        break;
      case 'f':
        value->push_back('\f');
        break;
      case 'n':
        value->push_back('\n');
        break;
      case 'r':
        value->push_back('\r');
        break;
      case 't':
        value->push_back('\t');
        break;
      case 'u': {
        std::uint32_t code_unit = 0;
        if (!ReadJsonHexQuad(json, position, &code_unit)) {
          return false;
        }
        if (code_unit >= 0xD800 && code_unit <= 0xDBFF) {
          if (*position + 2 > json.size() || json[*position] != '\\' ||
              json[*position + 1] != 'u') {
            return false;
          }
          *position += 2;
          std::uint32_t low = 0;
          if (!ReadJsonHexQuad(json, position, &low) || low < 0xDC00 || low > 0xDFFF) {
            return false;
          }
          const std::uint32_t high_offset = code_unit - 0xD800;
          const std::uint32_t low_offset = low - 0xDC00;
          code_unit = 0x10000 + ((high_offset << 10) | low_offset);
        }
        if (!AppendUtf8CodePoint(code_unit, value)) {
          return false;
        }
        break;
      }
      default:
        return false;
    }
  }
  return false;
}

bool SkipJsonValue(std::string_view json, size_t* position);

bool SkipJsonLiteral(std::string_view json, size_t* position, std::string_view literal) {
  if (json.substr(*position, literal.size()) != literal) {
    return false;
  }
  *position += literal.size();
  return true;
}

bool SkipJsonNumber(std::string_view json, size_t* position) {
  size_t cursor = *position;
  if (cursor < json.size() && json[cursor] == '-') {
    ++cursor;
  }
  if (cursor >= json.size()) {
    return false;
  }
  if (json[cursor] == '0') {
    ++cursor;
  } else if (std::isdigit(static_cast<unsigned char>(json[cursor])) != 0) {
    while (cursor < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[cursor])) != 0) {
      ++cursor;
    }
  } else {
    return false;
  }
  if (cursor < json.size() && json[cursor] == '.') {
    ++cursor;
    const size_t fraction_start = cursor;
    while (cursor < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[cursor])) != 0) {
      ++cursor;
    }
    if (cursor == fraction_start) {
      return false;
    }
  }
  if (cursor < json.size() && (json[cursor] == 'e' || json[cursor] == 'E')) {
    ++cursor;
    if (cursor < json.size() && (json[cursor] == '+' || json[cursor] == '-')) {
      ++cursor;
    }
    const size_t exponent_start = cursor;
    while (cursor < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[cursor])) != 0) {
      ++cursor;
    }
    if (cursor == exponent_start) {
      return false;
    }
  }
  *position = cursor;
  return true;
}

bool SkipJsonArray(std::string_view json, size_t* position) {
  if (*position >= json.size() || json[*position] != '[') {
    return false;
  }
  ++*position;
  *position = SkipJsonWhitespace(json, *position);
  if (*position < json.size() && json[*position] == ']') {
    ++*position;
    return true;
  }
  while (*position < json.size()) {
    if (!SkipJsonValue(json, position)) {
      return false;
    }
    *position = SkipJsonWhitespace(json, *position);
    if (*position < json.size() && json[*position] == ',') {
      ++*position;
      *position = SkipJsonWhitespace(json, *position);
      continue;
    }
    if (*position < json.size() && json[*position] == ']') {
      ++*position;
      return true;
    }
    return false;
  }
  return false;
}

bool SkipJsonObject(std::string_view json, size_t* position) {
  if (*position >= json.size() || json[*position] != '{') {
    return false;
  }
  ++*position;
  *position = SkipJsonWhitespace(json, *position);
  if (*position < json.size() && json[*position] == '}') {
    ++*position;
    return true;
  }
  while (*position < json.size()) {
    std::string key;
    if (!ParseJsonString(json, position, &key)) {
      return false;
    }
    *position = SkipJsonWhitespace(json, *position);
    if (*position >= json.size() || json[*position] != ':') {
      return false;
    }
    ++*position;
    if (!SkipJsonValue(json, position)) {
      return false;
    }
    *position = SkipJsonWhitespace(json, *position);
    if (*position < json.size() && json[*position] == ',') {
      ++*position;
      *position = SkipJsonWhitespace(json, *position);
      continue;
    }
    if (*position < json.size() && json[*position] == '}') {
      ++*position;
      return true;
    }
    return false;
  }
  return false;
}

bool SkipJsonValue(std::string_view json, size_t* position) {
  *position = SkipJsonWhitespace(json, *position);
  if (*position >= json.size()) {
    return false;
  }
  switch (json[*position]) {
    case '"': {
      std::string ignored;
      return ParseJsonString(json, position, &ignored);
    }
    case '{':
      return SkipJsonObject(json, position);
    case '[':
      return SkipJsonArray(json, position);
    case 't':
      return SkipJsonLiteral(json, position, "true");
    case 'f':
      return SkipJsonLiteral(json, position, "false");
    case 'n':
      return SkipJsonLiteral(json, position, "null");
    default:
      return json[*position] == '-' ||
                     std::isdigit(static_cast<unsigned char>(json[*position])) != 0
                 ? SkipJsonNumber(json, position)
                 : false;
  }
}

std::optional<JsonValueSpan> FindJsonObjectMember(std::string_view json,
                                                  size_t object_start,
                                                  std::string_view key) {
  size_t position = SkipJsonWhitespace(json, object_start);
  if (position >= json.size() || json[position] != '{') {
    return std::nullopt;
  }
  ++position;
  position = SkipJsonWhitespace(json, position);
  while (position < json.size() && json[position] != '}') {
    std::string member_key;
    if (!ParseJsonString(json, &position, &member_key)) {
      return std::nullopt;
    }
    position = SkipJsonWhitespace(json, position);
    if (position >= json.size() || json[position] != ':') {
      return std::nullopt;
    }
    ++position;
    position = SkipJsonWhitespace(json, position);
    const size_t value_start = position;
    size_t value_end = position;
    if (!SkipJsonValue(json, &value_end)) {
      return std::nullopt;
    }
    if (member_key == key) {
      return JsonValueSpan{.start = value_start, .end = value_end};
    }
    position = SkipJsonWhitespace(json, value_end);
    if (position < json.size() && json[position] == ',') {
      ++position;
      position = SkipJsonWhitespace(json, position);
      continue;
    }
    if (position < json.size() && json[position] == '}') {
      break;
    }
    return std::nullopt;
  }
  return std::nullopt;
}

std::optional<std::string> ExtractJsonString(std::string_view json,
                                             size_t object_start,
                                             std::string_view key) {
  const auto span = FindJsonObjectMember(json, object_start, key);
  if (!span) {
    return std::nullopt;
  }
  size_t position = SkipJsonWhitespace(json, span->start);
  std::string value;
  if (!ParseJsonString(json, &position, &value)) {
    return std::nullopt;
  }
  position = SkipJsonWhitespace(json, position);
  return position == span->end ? std::optional<std::string>(std::move(value)) : std::nullopt;
}

std::optional<std::string> ExtractAssetDownloadUrl(std::string_view json,
                                                   size_t object_start,
                                                   std::string_view asset_name) {
  size_t position = SkipJsonWhitespace(json, object_start);
  if (position >= json.size() || json[position] != '{') {
    return std::nullopt;
  }
  ++position;
  position = SkipJsonWhitespace(json, position);

  std::optional<std::string> name;
  std::optional<std::string> download_url;
  while (position < json.size() && json[position] != '}') {
    std::string member_key;
    if (!ParseJsonString(json, &position, &member_key)) {
      return std::nullopt;
    }
    position = SkipJsonWhitespace(json, position);
    if (position >= json.size() || json[position] != ':') {
      return std::nullopt;
    }
    ++position;
    position = SkipJsonWhitespace(json, position);

    if (member_key == "name" || member_key == "browser_download_url") {
      std::string value;
      if (!ParseJsonString(json, &position, &value)) {
        return std::nullopt;
      }
      if (member_key == "name") {
        name = std::move(value);
      } else {
        download_url = std::move(value);
      }
    } else if (!SkipJsonValue(json, &position)) {
      return std::nullopt;
    }

    position = SkipJsonWhitespace(json, position);
    if (position < json.size() && json[position] == ',') {
      ++position;
      position = SkipJsonWhitespace(json, position);
      continue;
    }
    if (position < json.size() && json[position] == '}') {
      break;
    }
    return std::nullopt;
  }

  if (name && *name == asset_name && download_url) {
    return download_url;
  }
  return std::nullopt;
}

std::optional<std::string> FindAssetUrl(std::string_view json,
                                        size_t root_object_start,
                                        std::string_view asset_name) {
  const auto assets = FindJsonObjectMember(json, root_object_start, "assets");
  if (!assets) {
    return std::nullopt;
  }
  size_t position = SkipJsonWhitespace(json, assets->start);
  if (position >= json.size() || json[position] != '[') {
    return std::nullopt;
  }
  ++position;
  position = SkipJsonWhitespace(json, position);
  while (position < json.size() && json[position] != ']') {
    const size_t item_start = position;
    size_t item_end = position;
    if (!SkipJsonValue(json, &item_end)) {
      return std::nullopt;
    }
    if (item_start < json.size() && json[item_start] == '{') {
      if (const auto url = ExtractAssetDownloadUrl(json, item_start, asset_name)) {
        return url;
      }
    }
    position = SkipJsonWhitespace(json, item_end);
    if (position < json.size() && json[position] == ',') {
      ++position;
      position = SkipJsonWhitespace(json, position);
      continue;
    }
    if (position < json.size() && json[position] == ']') {
      break;
    }
    return std::nullopt;
  }
  return std::nullopt;
}

std::optional<ReleaseInfo> ParseReleaseInfo(std::string_view json, std::string_view asset_name) {
  size_t root = 0;
  if (json.size() >= 3 && static_cast<unsigned char>(json[0]) == 0xEF &&
      static_cast<unsigned char>(json[1]) == 0xBB &&
      static_cast<unsigned char>(json[2]) == 0xBF) {
    root = 3;
  }
  root = SkipJsonWhitespace(json, root);
  if (root >= json.size() || json[root] != '{') {
    return std::nullopt;
  }
  const auto tag = ExtractJsonString(json, root, "tag_name");
  const auto asset_url = FindAssetUrl(json, root, asset_name);
  if (!tag || !asset_url) {
    return std::nullopt;
  }
  return ReleaseInfo{.tag = *tag, .asset_name = std::string(asset_name), .asset_url = *asset_url};
}

std::optional<ReleaseInfo> CheckRelease(const wchar_t* latest_url,
                                        std::string_view asset_name) {
  const auto body = HttpGet(latest_url);
  return body ? ParseReleaseInfo(*body, asset_name) : std::nullopt;
}

std::optional<ReleaseInfo> QueryAppRelease() {
  const std::wstring latest_url(fp::kGitHubLatestReleaseApiUrl);
  return CheckRelease(latest_url.c_str(), fp::WideToUtf8(fp::kReleaseMsiAssetName));
}

bool LaunchInstaller(const std::filesystem::path& installer_path, std::string_view) {
  const std::wstring params = L"/i \"" + installer_path.wstring() + L"\" /passive /norestart";
  return reinterpret_cast<intptr_t>(
             ShellExecuteW(nullptr, L"runas", L"msiexec.exe", params.c_str(), nullptr, SW_SHOWNORMAL)) > 32;
}

bool VerifyInstallerSignature(const std::filesystem::path& installer_path) {
  WINTRUST_FILE_INFO file_info{};
  file_info.cbStruct = sizeof(file_info);
  const std::wstring path = installer_path.wstring();
  file_info.pcwszFilePath = path.c_str();

  WINTRUST_DATA trust_data{};
  trust_data.cbStruct = sizeof(trust_data);
  trust_data.dwUIChoice = WTD_UI_NONE;
  trust_data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
  trust_data.dwUnionChoice = WTD_CHOICE_FILE;
  trust_data.dwStateAction = WTD_STATEACTION_VERIFY;
  trust_data.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
  trust_data.pFile = &file_info;

  GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  LONG status = WinVerifyTrust(nullptr, &policy, &trust_data);

  trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(nullptr, &policy, &trust_data);

  if (status == ERROR_SUCCESS) {
    fp::LogInfo(L"updater", L"Installer signature verified: " + installer_path.wstring());
    return true;
  }

  fp::LogWarning(L"updater",
                 L"Installer signature verification failed (" +
                     std::to_wstring(static_cast<unsigned long>(status)) + L"): " +
                     installer_path.wstring());
  return false;
}

std::wstring NormalizeVersionTag(std::string_view tag) {
  std::wstring value = fp::Utf8ToWide(tag);
  if (!value.empty() && (value.front() == L'v' || value.front() == L'V')) {
    value.erase(value.begin());
  }
  return value;
}

std::vector<int> ParseVersionParts(std::wstring_view value) {
  std::vector<int> parts;
  std::wstring current;
  for (const wchar_t ch : value) {
    if (ch >= L'0' && ch <= L'9') {
      current.push_back(ch);
      continue;
    }
    if (!current.empty()) {
      parts.push_back(std::stoi(current));
      current.clear();
    }
  }
  if (!current.empty()) {
    parts.push_back(std::stoi(current));
  }
  return parts;
}

int CompareVersions(std::wstring_view left, std::wstring_view right) {
  const auto left_parts = ParseVersionParts(left);
  const auto right_parts = ParseVersionParts(right);
  const size_t count = std::max(left_parts.size(), right_parts.size());
  for (size_t index = 0; index < count; ++index) {
    const int left_value = index < left_parts.size() ? left_parts[index] : 0;
    const int right_value = index < right_parts.size() ? right_parts[index] : 0;
    if (left_value < right_value) {
      return -1;
    }
    if (left_value > right_value) {
      return 1;
    }
  }
  return 0;
}

int CheckUpdates() {
  const auto app = QueryAppRelease();
  if (!app) {
    std::wcerr << L"Update check failed.\n";
    return 1;
  }

  std::wcout << L"FluentPinyin current: " << fp::kProductVersion << L"\n";
  std::wcout << L"FluentPinyin latest: " << NormalizeVersionTag(app->tag) << L"\n";
  std::wcout << L"Installer asset: " << fp::Utf8ToWide(app->asset_url) << L"\n";
  return CompareVersions(fp::kProductVersion, NormalizeVersionTag(app->tag)) < 0 ? 2 : 0;
}

int UpdateApp() {
  const auto release = QueryAppRelease();
  if (!release) {
    std::wcerr << L"Update check failed.\n";
    MessageBoxW(nullptr,
                L"未找到可用的 GitHub Release 安装包。",
                L"流畅拼音 更新",
                MB_OK | MB_ICONWARNING);
    return 1;
  }

  const std::wstring latest_version = NormalizeVersionTag(release->tag);
  if (CompareVersions(fp::kProductVersion, latest_version) >= 0) {
    MessageBoxW(nullptr,
                L"当前已是最新版本。",
                L"流畅拼音 更新",
                MB_OK | MB_ICONINFORMATION);
    return 0;
  }

  const std::wstring confirm_message =
      L"发现新版本 " + latest_version + L"，是否下载安装？";
  const int confirm = MessageBoxW(nullptr,
                                  confirm_message.c_str(),
                                  L"流畅拼音 更新",
                                  MB_YESNO | MB_ICONQUESTION);
  if (confirm != IDYES) {
    return 0;
  }

  const auto update_dir = fp::GetFpLocalDataPath() / L"Updates";
  const auto target = update_dir / fp::Utf8ToWide(release->asset_name);
  std::wcout << L"Downloading FluentPinyin " << fp::Utf8ToWide(release->tag) << L"...\n";
  if (!DownloadFile(fp::Utf8ToWide(release->asset_url), target)) {
    std::wcerr << L"Failed to download installer.\n";
    MessageBoxW(nullptr,
                L"下载安装包失败，请稍后重试。",
                L"流畅拼音 更新",
                MB_OK | MB_ICONERROR);
    return 2;
  }

  if (!VerifyInstallerSignature(target)) {
    std::wcerr << L"Installer signature verification failed.\n";
    MessageBoxW(nullptr,
                L"安装包签名验证失败，已停止更新。",
                L"流畅拼音 更新",
                MB_OK | MB_ICONERROR);
    return 4;
  }

  if (!LaunchInstaller(target, release->asset_name)) {
    std::wcerr << L"Failed to launch installer.\n";
    MessageBoxW(nullptr,
                L"安装包已下载，但启动失败。",
                L"流畅拼音 更新",
                MB_OK | MB_ICONERROR);
    return 3;
  }

  std::wcout << L"Installer launched: " << target.wstring() << L"\n";
  MessageBoxW(nullptr,
              L"安装程序已启动。",
              L"流畅拼音 更新",
              MB_OK | MB_ICONINFORMATION);
  return 0;
}

int ParseReleaseJsonForTest(const std::filesystem::path& json_path,
                            std::wstring_view expected_asset_name) {
  const auto body = [&]() -> std::optional<std::string> {
    std::ifstream file(json_path, std::ios::binary);
    if (!file) {
      return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  }();
  if (!body) {
    std::wcerr << L"Failed to read release JSON: " << json_path.wstring() << L"\n";
    return 1;
  }
  const auto release = ParseReleaseInfo(*body, fp::WideToUtf8(expected_asset_name));
  if (!release) {
    size_t root = 0;
    if (body->size() >= 3 && static_cast<unsigned char>((*body)[0]) == 0xEF &&
        static_cast<unsigned char>((*body)[1]) == 0xBB &&
        static_cast<unsigned char>((*body)[2]) == 0xBF) {
      root = 3;
    }
    root = SkipJsonWhitespace(*body, root);
    const bool has_root = root < body->size() && (*body)[root] == '{';
    const auto tag = has_root ? ExtractJsonString(*body, root, "tag_name") : std::nullopt;
    const auto asset =
        has_root ? FindAssetUrl(*body, root, fp::WideToUtf8(expected_asset_name)) : std::nullopt;
    std::wcerr << L"Failed to parse release JSON. root=" << (has_root ? L"yes" : L"no")
               << L", tag=" << (tag ? L"yes" : L"no")
               << L", asset=" << (asset ? L"yes" : L"no") << L"\n";
    return 2;
  }
  std::wcout << L"tag=" << fp::Utf8ToWide(release->tag) << L"\n";
  std::wcout << L"asset=" << fp::Utf8ToWide(release->asset_name) << L"\n";
  std::wcout << L"url=" << fp::Utf8ToWide(release->asset_url) << L"\n";
  return 0;
}

void PrintUsage() {
  std::wcout << L"Usage: fluent-pinyin-updater <check|update|parse-release-json>\n";
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  fp::LogInfo(L"updater", L"fluent-pinyin-updater started.");

  if (argc == 2 && std::wstring_view(argv[1]) == L"check") {
    return CheckUpdates();
  }
  if (argc == 2 && std::wstring_view(argv[1]) == L"update") {
    return UpdateApp();
  }
  if ((argc == 3 || argc == 4) && std::wstring_view(argv[1]) == L"parse-release-json") {
    const std::wstring_view asset_name = argc == 4 ? std::wstring_view(argv[3])
                                                   : fp::kReleaseMsiAssetName;
    return ParseReleaseJsonForTest(argv[2], asset_name);
  }
  PrintUsage();
  return argc == 1 ? 0 : 1;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr) {
    return wmain(0, nullptr);
  }
  const int result = wmain(argc, argv);
  LocalFree(argv);
  return result;
}
