#include "common/constants.h"
#include "common/logging.h"

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace {

std::filesystem::path ModuleDirectory() {
  std::wstring buffer(32768, L'\0');
  const DWORD length =
      GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) {
    return std::filesystem::current_path();
  }
  buffer.resize(length);
  return std::filesystem::path(buffer).parent_path();
}

bool HasArgument(std::wstring_view command_line, std::wstring_view argument) {
  return command_line.find(argument) != std::wstring_view::npos;
}

UINT ToolbarHostShutdownMessage() {
  static const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kToolbarHostShutdownMessageName).c_str());
  return message;
}

void RequestToolbarHostShutdown() {
  const UINT message = ToolbarHostShutdownMessage();
  if (message != 0) {
    PostMessageW(HWND_BROADCAST, message, 0, 0);
  }
}

void OpenSettings() {
  const auto settings = ModuleDirectory() / L"fluent-pinyin-settings.exe";
  ShellExecuteW(nullptr, L"open", settings.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

int RunToolbarHost() {
  RequestToolbarHostShutdown();
  fp::LogWarning(L"ui", L"Toolbar host mode is disabled to avoid recursive TSF activation.");
  return 0;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR command_line, int) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  const std::wstring_view args = command_line != nullptr ? std::wstring_view(command_line) : L"";
  if (HasArgument(args, L"--toolbar") || HasArgument(args, L"--service")) {
    return RunToolbarHost();
  }

  OpenSettings();
  return 0;
}
