#include "config_winui/settings_app_lifecycle.h"

#include "common/constants.h"
#include "config_winui/app_paths.h"
#include "config_winui/settings_navigation.h"
#include "config_winui/settings_refresh.h"
#include "sync/sync_service.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <system_error>

namespace fp::config_winui {
namespace {

constexpr std::wstring_view kWindowsAppRuntimeInstallerName =
    L"windowsappruntimeinstall-x64.exe";

}  // namespace

std::wstring InitialPageTagFromProcess() {
  const wchar_t* command_line = GetCommandLineW();
  return InitialSettingsPageTag(command_line != nullptr ? command_line : L"");
}

void ShowWindowsAppRuntimeMissingMessage(long result) {
  const auto installer = ModuleDirectory() / std::wstring(kWindowsAppRuntimeInstallerName);
  std::wstring message =
      L"无法启动设置面板，因为系统缺少 Windows App Runtime 1.8。\n\n"
      L"请重新运行 FluentPinyin 安装包修复运行时依赖。";
  std::error_code error;
  if (std::filesystem::exists(installer, error)) {
    message += L"\n\n也可以运行安装目录中的 windowsappruntimeinstall-x64.exe 后再打开设置。";
  }
  wchar_t code[32]{};
  swprintf_s(code, L"\n\n错误代码：0x%08lX", static_cast<unsigned long>(result));
  message += code;
  MessageBoxW(nullptr, message.c_str(), L"流畅拼音 设置", MB_ICONERROR);
}

bool HasCommandLineSwitch(std::wstring_view switch_name) {
  const std::wstring command = GetCommandLineW() != nullptr ? GetCommandLineW() : L"";
  return command.find(std::wstring(switch_name)) != std::wstring::npos;
}

int RunAutoSyncCommand() {
  const auto result = fp::sync::RunAutoSync();
  if (result.success) {
    RequestApplyInputConfig();
    return 0;
  }
  return 1;
}

bool ActivateExistingSettingsWindow(int attempts) {
  return fp::config_winui::ActivateWindowByTitle(kSettingsAppTitle, attempts);
}

}  // namespace fp::config_winui
