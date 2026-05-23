#include "common/constants.h"
#include "common/logging.h"
#include "common/path_utils.h"

#include <windows.h>
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
  std::string asset_url;
};

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

  return ReleaseInfo{.tag = *tag, .asset_url = *asset_url};
}

struct UpdateInfo {
  ReleaseInfo librime;
  ReleaseInfo frost;
};

std::optional<UpdateInfo> QueryUpdates() {
  const auto librime =
      CheckRelease(L"https://api.github.com/repos/rime/librime/releases/latest",
                   "rime-de4700e-Windows-msvc-x64.7z");
  const auto frost =
      CheckRelease(L"https://api.github.com/repos/gaboolic/rime-frost/releases/latest",
                   "rime-frost-schemas.zip");

  if (!librime || !frost) {
    return std::nullopt;
  }

  return UpdateInfo{.librime = *librime, .frost = *frost};
}

int CheckUpdates() {
  const auto updates = QueryUpdates();
  if (!updates) {
    std::wcerr << L"Update check failed.\n";
    return 1;
  }

  std::wcout << L"FluentPinyin " << fp::kProductVersion << L"\n";
  std::wcout << L"librime latest: " << Utf8ToWide(updates->librime.tag) << L"\n";
  std::wcout << L"librime asset: " << Utf8ToWide(updates->librime.asset_url) << L"\n";
  std::wcout << L"Frost latest: " << Utf8ToWide(updates->frost.tag) << L"\n";
  std::wcout << L"Frost asset: " << Utf8ToWide(updates->frost.asset_url) << L"\n";
  return 0;
}

int DownloadUpdates() {
  const auto updates = QueryUpdates();
  if (!updates) {
    std::wcerr << L"Update check failed.\n";
    return 1;
  }

  const auto package_dir = fp::GetFpLocalDataPath() / L"Packages" / L"downloads";
  const auto librime_target = package_dir / L"rime-de4700e-Windows-msvc-x64.7z";
  const auto frost_target = package_dir / L"rime-frost-schemas.zip";

  std::wcout << L"Downloading librime " << Utf8ToWide(updates->librime.tag) << L"...\n";
  if (!DownloadFile(Utf8ToWide(updates->librime.asset_url), librime_target)) {
    std::wcerr << L"Failed to download librime.\n";
    return 2;
  }

  std::wcout << L"Downloading Frost " << Utf8ToWide(updates->frost.tag) << L"...\n";
  if (!DownloadFile(Utf8ToWide(updates->frost.asset_url), frost_target)) {
    std::wcerr << L"Failed to download Frost.\n";
    return 3;
  }

  std::wcout << L"Downloaded packages to " << package_dir.wstring() << L"\n";
  return 0;
}

void PrintUsage() {
  std::wcout << L"Usage: fp-updater <check|download>\n";
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  fp::LogInfo(L"updater", L"fp-updater started.");

  if (argc == 2 && std::wstring_view(argv[1]) == L"check") {
    return CheckUpdates();
  }
  if (argc == 2 && std::wstring_view(argv[1]) == L"download") {
    return DownloadUpdates();
  }

  PrintUsage();
  return argc == 1 ? 0 : 1;
}
