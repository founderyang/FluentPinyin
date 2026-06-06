#include "common/constants.h"
#include "common/logging.h"
#include "common/path_utils.h"

#include <windows.h>
#include <shellapi.h>
#include <wininet.h>

#include <algorithm>
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

std::string WideToUtf8(std::wstring_view value) {
  if (value.empty()) {
    return {};
  }

  const int required = WideCharToMultiByte(CP_UTF8,
                                           0,
                                           value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr);
  if (required <= 0) {
    return {};
  }

  std::string result(static_cast<size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8,
                      0,
                      value.data(),
                      static_cast<int>(value.size()),
                      result.data(),
                      required,
                      nullptr,
                      nullptr);
  return result;
}

std::wstring Utf8ToWide(std::string_view value) {
  if (value.empty()) {
    return {};
  }

  const int required = MultiByteToWideChar(CP_UTF8,
                                           0,
                                           value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr,
                                           0);
  if (required <= 0) {
    return {};
  }

  std::wstring result(static_cast<size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8,
                      0,
                      value.data(),
                      static_cast<int>(value.size()),
                      result.data(),
                      required);
  return result;
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

std::optional<std::string> ExtractJsonString(std::string_view json, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  const size_t key_pos = json.find(needle);
  if (key_pos == std::string_view::npos) {
    return std::nullopt;
  }

  const size_t colon_pos = json.find(':', key_pos + needle.size());
  if (colon_pos == std::string_view::npos) {
    return std::nullopt;
  }

  const size_t quote_pos = json.find('"', colon_pos + 1);
  if (quote_pos == std::string_view::npos) {
    return std::nullopt;
  }

  std::string value;
  bool escaping = false;
  for (size_t index = quote_pos + 1; index < json.size(); ++index) {
    const char ch = json[index];
    if (escaping) {
      value.push_back(ch);
      escaping = false;
      continue;
    }
    if (ch == '\\') {
      escaping = true;
      continue;
    }
    if (ch == '"') {
      return value;
    }
    value.push_back(ch);
  }

  return std::nullopt;
}

std::optional<std::string> FindAssetUrl(std::string_view json, std::string_view asset_name) {
  size_t position = 0;
  while (true) {
    const size_t name_pos = json.find("\"name\"", position);
    if (name_pos == std::string_view::npos) {
      return std::nullopt;
    }

    const auto name = ExtractJsonString(json.substr(name_pos), "name");
    if (name && *name == asset_name) {
      const auto browser_url = ExtractJsonString(json.substr(name_pos), "browser_download_url");
      if (browser_url) {
        return browser_url;
      }
    }

    position = name_pos + 6;
  }
}

std::optional<ReleaseInfo> CheckRelease(const wchar_t* latest_url,
                                        std::string_view asset_name) {
  const auto body = HttpGet(latest_url);
  if (!body) {
    return std::nullopt;
  }

  const auto tag = ExtractJsonString(*body, "tag_name");
  const auto asset_url = FindAssetUrl(*body, asset_name);
  if (!tag || !asset_url) {
    return std::nullopt;
  }

  return ReleaseInfo{.tag = *tag, .asset_name = std::string(asset_name), .asset_url = *asset_url};
}

std::optional<ReleaseInfo> QueryAppRelease() {
  const std::wstring latest_url(fp::kGitHubLatestReleaseApiUrl);
  return CheckRelease(latest_url.c_str(), WideToUtf8(fp::kReleaseMsiAssetName));
}

bool LaunchInstaller(const std::filesystem::path& installer_path, std::string_view) {
  const std::wstring params = L"/i \"" + installer_path.wstring() + L"\" /passive /norestart";
  return reinterpret_cast<intptr_t>(
             ShellExecuteW(nullptr, L"runas", L"msiexec.exe", params.c_str(), nullptr, SW_SHOWNORMAL)) > 32;
}

std::wstring NormalizeVersionTag(std::string_view tag) {
  std::wstring value = Utf8ToWide(tag);
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
  std::wcout << L"Installer asset: " << Utf8ToWide(app->asset_url) << L"\n";
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
  const auto target = update_dir / Utf8ToWide(release->asset_name);
  std::wcout << L"Downloading FluentPinyin " << Utf8ToWide(release->tag) << L"...\n";
  if (!DownloadFile(Utf8ToWide(release->asset_url), target)) {
    std::wcerr << L"Failed to download installer.\n";
    MessageBoxW(nullptr,
                L"下载安装包失败，请稍后重试。",
                L"流畅拼音 更新",
                MB_OK | MB_ICONERROR);
    return 2;
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

void PrintUsage() {
  std::wcout << L"Usage: fluent-pinyin-updater <check|update>\n";
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
