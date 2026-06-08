#pragma once

#include <string>
#include <string_view>

namespace fp::config_winui {

inline constexpr std::wstring_view kSettingsAppTitle = L"流畅拼音输入法设置";
inline constexpr std::wstring_view kSettingsSingleInstanceMutexName =
    L"Local\\FluentPinyinSettingsSingleInstance";

std::wstring InitialPageTagFromProcess();
void ShowWindowsAppRuntimeMissingMessage(long result);
bool HasCommandLineSwitch(std::wstring_view switch_name);
int RunAutoSyncCommand();
bool ActivateExistingSettingsWindow(int attempts = 1);

}  // namespace fp::config_winui
