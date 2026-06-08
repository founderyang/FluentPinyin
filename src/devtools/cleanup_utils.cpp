#include "devtools/cleanup_utils.h"

#include "common/encoding.h"
#include "common/logging.h"

#include <windows.h>

#include <algorithm>
#include <string_view>
#include <system_error>
#include <vector>

namespace fp::devtools {
namespace {

constexpr std::wstring_view kInstallDirName = L"FluentPinyin";

}  // namespace

void LogDeferredDeleteSkipped(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }
  fp::LogWarning(L"installer",
                 L"cleanup left path in place to avoid Windows restart prompts: " +
                     path.wstring());
}

bool RemoveFileNow(const std::filesystem::path& path) {
  if (path.empty()) {
    return true;
  }

  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return true;
  }

  std::filesystem::permissions(path,
                               std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::add,
                               error);
  error.clear();

  for (int attempt = 0; attempt < 8; ++attempt) {
    std::filesystem::remove(path, error);
    if (!std::filesystem::exists(path, error)) {
      return true;
    }
    DeleteFileW(path.c_str());
    if (!std::filesystem::exists(path, error)) {
      return true;
    }
    Sleep(250);
    error.clear();
  }

  LogDeferredDeleteSkipped(path);
  return false;
}

void RemovePathTree(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }

  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return;
  }

  for (auto iterator =
           std::filesystem::recursive_directory_iterator(path,
                                                        std::filesystem::directory_options::
                                                            skip_permission_denied,
                                                        error);
       !error && iterator != std::filesystem::recursive_directory_iterator();
       iterator.increment(error)) {
    std::filesystem::permissions(iterator->path(),
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::add,
                                 error);
    error.clear();
  }

  for (int attempt = 0; attempt < 6; ++attempt) {
    std::filesystem::remove_all(path, error);
    if (!std::filesystem::exists(path, error)) {
      return;
    }
    Sleep(250);
    error.clear();
  }

  std::vector<std::filesystem::path> remaining;
  error.clear();
  for (auto iterator =
           std::filesystem::recursive_directory_iterator(path,
                                                        std::filesystem::directory_options::
                                                            skip_permission_denied,
                                                        error);
       !error && iterator != std::filesystem::recursive_directory_iterator();
       iterator.increment(error)) {
    remaining.push_back(iterator->path());
  }
  std::sort(remaining.begin(),
            remaining.end(),
            [](const auto& left, const auto& right) {
              return left.native().size() > right.native().size();
            });
  for (const auto& child : remaining) {
    if (std::filesystem::is_regular_file(child, error)) {
      RemoveFileNow(child);
    } else {
      LogDeferredDeleteSkipped(child);
    }
    error.clear();
  }
  LogDeferredDeleteSkipped(path);
}

bool IsSafeInstallDirectory(const std::filesystem::path& path) {
  if (path.empty() || !path.has_root_path()) {
    return false;
  }
  return fp::EqualsInsensitive(path.filename().wstring(), kInstallDirName);
}

}  // namespace fp::devtools
