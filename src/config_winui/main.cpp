#include "config_winui/settings_app.h"

#include "config_winui/app_paths.h"
#include "config_winui/settings_app_lifecycle.h"
#include "config_winui/settings_ui_helpers.h"

#include <windows.h>

#include <string>

using fp::config_winui::ActivateExistingSettingsWindow;
using fp::config_winui::EnsureUiFontsLoaded;
using fp::config_winui::HasCommandLineSwitch;
using fp::config_winui::kSettingsSingleInstanceMutexName;
using fp::config_winui::RunAutoSyncCommand;
using fp::config_winui::RunWinUiApp;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  EnsureUiFontsLoaded();
  if (HasCommandLineSwitch(L"--auto-sync")) {
    return RunAutoSyncCommand();
  }

  const std::wstring mutex_name(kSettingsSingleInstanceMutexName);
  HANDLE single_instance_mutex = CreateMutexW(nullptr, TRUE, mutex_name.c_str());
  const DWORD mutex_error = single_instance_mutex != nullptr ? GetLastError() : ERROR_SUCCESS;
  if (single_instance_mutex != nullptr && mutex_error == ERROR_ALREADY_EXISTS) {
    ActivateExistingSettingsWindow(40);
    CloseHandle(single_instance_mutex);
    return 0;
  }

  const int result = RunWinUiApp();
  if (single_instance_mutex != nullptr) {
    ReleaseMutex(single_instance_mutex);
    CloseHandle(single_instance_mutex);
  }
  return result;
}
