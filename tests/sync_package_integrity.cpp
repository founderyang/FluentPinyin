#include "sync/sync_service.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> ReadAllBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file),
                                   std::istreambuf_iterator<char>());
}

bool WriteAllBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& data) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  return file.good();
}

bool SetClipboardText(std::wstring_view text) {
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
  void* target = GlobalLock(data);
  if (target == nullptr) {
    GlobalFree(data);
    CloseClipboard();
    return false;
  }
  std::memcpy(target, text.data(), text.size() * sizeof(wchar_t));
  static_cast<wchar_t*>(target)[text.size()] = L'\0';
  GlobalUnlock(data);
  if (SetClipboardData(CF_UNICODETEXT, data) == nullptr) {
    GlobalFree(data);
    CloseClipboard();
    return false;
  }
  CloseClipboard();
  return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 2) {
    std::wcerr << L"Usage: sync_package_integrity <work-dir>\n";
    return 1;
  }

  const std::filesystem::path work_dir = argv[1];
  std::error_code error;
  std::filesystem::create_directories(work_dir, error);
  if (error) {
    std::wcerr << L"Failed to create work dir: " << error.message().c_str() << L"\n";
    return 1;
  }

  fp::sync::SyncConfig config;
  if (!SetClipboardText(L"FluentPinyin sync integrity smoke")) {
    std::wcerr << L"Failed to seed clipboard text.\n";
    return 2;
  }

  config.sync_clipboard = true;
  config.sync_user_data = false;
  config.encryption_secret = L"sync-integrity-secret";

  const auto package_path = work_dir / L"valid.fpsync";
  const auto create = fp::sync::CreateLocalBackup(package_path, config);
  if (!create.success) {
    std::wcerr << L"CreateLocalBackup failed: " << create.message << L"\n";
    return 3;
  }

  auto package = ReadAllBytes(package_path);
  if (package.size() < 32) {
    std::wcerr << L"Package is unexpectedly small.\n";
    return 4;
  }
  package.back() ^= 0x5A;

  const auto tampered_path = work_dir / L"tampered.fpsync";
  if (!WriteAllBytes(tampered_path, package)) {
    std::wcerr << L"Failed to write tampered package.\n";
    return 5;
  }

  fp::sync::PackageMetadata metadata;
  const auto restore = fp::sync::RestoreLocalBackup(tampered_path, config, false, &metadata);
  if (restore.success) {
    std::wcerr << L"Tampered package restored successfully; integrity check failed.\n";
    return 6;
  }

  std::wcout << L"Sync package integrity smoke passed.\n";
  return 0;
}
