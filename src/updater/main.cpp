#include "common/constants.h"
#include "common/encoding.h"
#include "common/logging.h"
#include "common/path_utils.h"
#include "updater/release_json.h"

#include <windows.h>
#include <shellapi.h>
#include <softpub.h>
#include <wininet.h>
#include <wintrust.h>
#include <mscat.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using fp::updater::ReleaseInfo;

constexpr DWORD kHttpConnectTimeoutMs = 10000;
constexpr DWORD kHttpSendTimeoutMs = 15000;
constexpr DWORD kHttpReceiveTimeoutMs = 30000;
constexpr std::wstring_view kExpectedPublisherSubject = L"FluentPinyin";
constexpr std::array<std::wstring_view, 1> kExpectedPublisherThumbprints{
    L""
};

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

std::optional<ReleaseInfo> CheckRelease(const wchar_t* latest_url,
                                        std::string_view asset_name) {
  const auto body = HttpGet(latest_url);
  return body ? fp::updater::ParseReleaseInfo(*body, asset_name) : std::nullopt;
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

std::wstring CertificateThumbprint(PCCERT_CONTEXT cert) {
  if (cert == nullptr) {
    return {};
  }
  DWORD size = 0;
  if (!CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, nullptr, &size) ||
      size == 0) {
    return {};
  }
  std::vector<BYTE> hash(size);
  if (!CertGetCertificateContextProperty(cert,
                                         CERT_SHA1_HASH_PROP_ID,
                                         hash.data(),
                                         &size)) {
    return {};
  }
  constexpr wchar_t hex[] = L"0123456789ABCDEF";
  std::wstring text;
  text.reserve(hash.size() * 2);
  for (const BYTE byte : hash) {
    text.push_back(hex[(byte >> 4) & 0x0F]);
    text.push_back(hex[byte & 0x0F]);
  }
  return text;
}

std::wstring CertificateSubject(PCCERT_CONTEXT cert) {
  if (cert == nullptr) {
    return {};
  }
  DWORD required = CertGetNameStringW(cert,
                                      CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                      0,
                                      nullptr,
                                      nullptr,
                                      0);
  if (required <= 1) {
    return {};
  }
  std::wstring subject(required, L'\0');
  CertGetNameStringW(cert,
                     CERT_NAME_SIMPLE_DISPLAY_TYPE,
                     0,
                     nullptr,
                     subject.data(),
                     required);
  subject.resize(subject.size() > 0 && subject.back() == L'\0' ? subject.size() - 1
                                                               : subject.size());
  return subject;
}

std::wstring NormalizeThumbprint(std::wstring_view value) {
  std::wstring normalized;
  normalized.reserve(value.size());
  for (const wchar_t ch : value) {
    if (ch == L' ' || ch == L':' || ch == L'-') {
      continue;
    }
    normalized.push_back(static_cast<wchar_t>(std::towupper(ch)));
  }
  return normalized;
}

PCCERT_CONTEXT PrimarySignerCertificate(const WINTRUST_DATA& trust_data) {
  CRYPT_PROVIDER_DATA* provider =
      WTHelperProvDataFromStateData(trust_data.hWVTStateData);
  if (provider == nullptr) {
    return nullptr;
  }
  CRYPT_PROVIDER_SGNR* signer = WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0);
  if (signer == nullptr) {
    return nullptr;
  }
  CRYPT_PROVIDER_CERT* cert = WTHelperGetProvCertFromChain(signer, 0);
  return cert == nullptr ? nullptr : cert->pCert;
}

bool ExpectedThumbprintsConfigured() {
  return std::any_of(kExpectedPublisherThumbprints.begin(),
                     kExpectedPublisherThumbprints.end(),
                     [](std::wstring_view thumbprint) {
                       return !thumbprint.empty();
                     });
}

bool VerifyInstallerPublisher(const WINTRUST_DATA& trust_data,
                              const std::filesystem::path& installer_path) {
  PCCERT_CONTEXT cert = PrimarySignerCertificate(trust_data);
  if (cert == nullptr) {
    fp::LogWarning(L"updater",
                   L"Installer publisher certificate was not available: " +
                       installer_path.wstring());
    return false;
  }

  const std::wstring thumbprint = CertificateThumbprint(cert);
  const std::wstring subject = CertificateSubject(cert);
  const bool thumbprints_configured = ExpectedThumbprintsConfigured();
  if (thumbprints_configured) {
    const std::wstring actual_thumbprint = NormalizeThumbprint(thumbprint);
    const bool matched = std::any_of(
        kExpectedPublisherThumbprints.begin(),
        kExpectedPublisherThumbprints.end(),
        [&actual_thumbprint](std::wstring_view expected) {
          return !expected.empty() &&
                 actual_thumbprint == NormalizeThumbprint(expected);
        });
    if (!matched) {
      fp::LogWarning(L"updater",
                     L"Installer publisher thumbprint mismatch. subject=" + subject +
                         L", thumbprint=" + thumbprint);
      return false;
    }
    fp::LogInfo(L"updater",
                L"Installer publisher thumbprint verified: " + thumbprint);
    return true;
  }

  if (subject.find(kExpectedPublisherSubject) == std::wstring::npos) {
    fp::LogWarning(L"updater",
                   L"Installer publisher subject mismatch. subject=" + subject +
                       L", thumbprint=" + thumbprint);
    return false;
  }
  fp::LogInfo(L"updater",
              L"Installer publisher subject verified without thumbprint pin: " +
                  subject);
  return true;
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
  const LONG status = WinVerifyTrust(nullptr, &policy, &trust_data);

  if (status == ERROR_SUCCESS) {
    const bool publisher_ok = VerifyInstallerPublisher(trust_data, installer_path);
    trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policy, &trust_data);
    if (publisher_ok) {
      fp::LogInfo(L"updater",
                  L"Installer signature and publisher verified: " + installer_path.wstring());
      return true;
    }
    return false;
  }

  trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(nullptr, &policy, &trust_data);
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
  const auto release = fp::updater::ParseReleaseInfo(*body, fp::WideToUtf8(expected_asset_name));
  if (!release) {
    const auto diagnostics =
        fp::updater::InspectReleaseJson(*body, fp::WideToUtf8(expected_asset_name));
    const bool has_root = diagnostics.root_object;
    const bool has_tag = diagnostics.tag;
    const bool has_asset = diagnostics.asset;
    std::wcerr << L"Failed to parse release JSON. root=" << (has_root ? L"yes" : L"no")
               << L", tag=" << (has_tag ? L"yes" : L"no")
               << L", asset=" << (has_asset ? L"yes" : L"no") << L"\n";
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
