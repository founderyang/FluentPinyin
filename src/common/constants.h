#pragma once

#include <string_view>

namespace fp {

inline constexpr std::wstring_view kProductName = L"\u6D41\u7545\u62FC\u97F3";
inline constexpr std::wstring_view kProductVersion = L"00.00.03";
inline constexpr std::wstring_view kGitHubRepoOwner = L"founderyang";
inline constexpr std::wstring_view kGitHubRepoName = L"FluentPinyin";
inline constexpr std::wstring_view kGitHubRepoUrl =
    L"https://github.com/founderyang/FluentPinyin";
inline constexpr std::wstring_view kGitHubLatestReleaseApiUrl =
    L"https://api.github.com/repos/founderyang/FluentPinyin/releases/latest";
inline constexpr std::wstring_view kReleaseMsiAssetName = L"FluentPinyin.msi";
inline constexpr std::wstring_view kLogsDirectoryName = L"Logs";
inline constexpr std::wstring_view kDefaultInputScheme = L"pinyin";
inline constexpr std::wstring_view kDefaultDoublePinyinScheme = L"zrm";
inline constexpr std::wstring_view kApplyInputConfigMessageName =
    L"FluentPinyin.RequestApplyInputConfig";
inline constexpr std::wstring_view kRestartInputCoreMessageName =
    L"FluentPinyin.RequestRestartInputCore";
inline constexpr std::wstring_view kShutdownInputCoreMessageName =
    L"FluentPinyin.RequestShutdownInputCore";
inline constexpr std::wstring_view kRefreshInputStateMessageName =
    L"FluentPinyin.RequestRefreshInputState.V2";
inline constexpr std::wstring_view kLegacyRefreshInputStateMessageName =
    L"FluentPinyin.RequestRefreshInputState";
inline constexpr std::wstring_view kToolbarRefreshMessageName =
    L"FluentPinyin.Toolbar.Refresh.V4";
inline constexpr std::wstring_view kLegacyToolbarRefreshMessageName =
    L"FluentPinyin.Toolbar.Refresh.V2";
inline constexpr std::wstring_view kToolbarHostShutdownMessageName =
    L"FluentPinyin.Toolbar.Host.Shutdown.V1";

}  // namespace fp
