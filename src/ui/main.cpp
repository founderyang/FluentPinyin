#include "common/constants.h"
#include "common/logging.h"

#include <windows.h>

#include <cwchar>
#include <string>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR command_line, int show_command) {
  (void)instance;
  (void)previous;
  (void)show_command;

  fp::LogInfo(L"ui", L"fp-ui placeholder started.");

  if (command_line == nullptr || wcsstr(command_line, L"--service") == nullptr) {
    const std::wstring title(fp::kProductName);
    const std::wstring body =
        title + L" candidate UI placeholder.\n\nRun with --service to keep the UI process alive.";
    MessageBoxW(nullptr,
                body.c_str(),
                title.c_str(),
                MB_OK | MB_ICONINFORMATION);
    return 0;
  }

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  fp::LogInfo(L"ui", L"fp-ui placeholder stopped.");
  return 0;
}
