#include "config_winui/shell_actions.h"

#include "common/path_utils.h"
#include "config_winui/app_paths.h"

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace fp::config_winui {

void OpenPath(const std::filesystem::path& path) {
  fp::EnsureDirectory(path);
  ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenUrl(std::wstring_view url) {
  const std::wstring target(url);
  ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void RunTool(std::wstring_view exe_name, std::wstring_view parameters) {
  const auto exe = SiblingExe(exe_name);
  const std::wstring params(parameters);
  ShellExecuteW(nullptr,
                L"open",
                exe.c_str(),
                params.empty() ? nullptr : params.c_str(),
                nullptr,
                SW_SHOWNORMAL);
}

}  // namespace fp::config_winui
