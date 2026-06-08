#include "common/broadcast_messages.h"
#include "common/constants.h"
#include "common/logging.h"
#include "common/path_utils.h"

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <string_view>

namespace {

bool HasArgument(std::wstring_view command_line, std::wstring_view argument) {
  return command_line.find(argument) != std::wstring_view::npos;
}

void RequestToolbarHostShutdown() {
  fp::PostRegisteredBroadcastMessage(fp::kToolbarHostShutdownMessageName);
}

void OpenSettings() {
  const auto settings = fp::GetSiblingExecutablePath(L"fluent-pinyin-settings.exe");
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
