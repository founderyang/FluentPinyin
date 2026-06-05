#include "common/constants.h"
#include "common/logging.h"
#include "common/path_utils.h"

#include <windows.h>
#include <shellapi.h>
#include <wininet.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

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
  auto release = CheckRelease(latest_url.c_str(), WideToUtf8(fp::kReleaseSetupAssetName));
  if (release) {
    return release;
  }
  return CheckRelease(latest_url.c_str(), WideToUtf8(fp::kReleaseMsiAssetName));
}

bool LaunchInstaller(const std::filesystem::path& installer_path, std::string_view asset_name) {
  if (asset_name.ends_with(".msi")) {
    const std::wstring params = L"/i \"" + installer_path.wstring() + L"\" /qn";
    return reinterpret_cast<intptr_t>(
               ShellExecuteW(nullptr, L"runas", L"msiexec.exe", params.c_str(), nullptr, SW_SHOWNORMAL)) > 32;
  }

  return reinterpret_cast<intptr_t>(
             ShellExecuteW(nullptr, L"runas", installer_path.c_str(), L"/S", nullptr, SW_SHOWNORMAL)) > 32;
}

int CheckUpdates() {
  const auto app = QueryAppRelease();
  if (!app) {
    std::wcerr << L"Update check failed.\n";
    return 1;
  }

  std::wcout << L"FluentPinyin current: " << fp::kProductVersion << L"\n";
  std::wcout << L"FluentPinyin latest: " << Utf8ToWide(app->tag) << L"\n";
  std::wcout << L"Installer asset: " << Utf8ToWide(app->asset_url) << L"\n";
  return 0;
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
              L"最新安装包已启动。",
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
