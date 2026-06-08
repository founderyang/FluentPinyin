#include "config_winui/about_page.h"

#include "common/constants.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/shell_actions.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <string>

namespace fp::config_winui {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

UIElement BuildAboutPage() {
  auto page = PageShell(L"关于", L"版本和更新。");
  page.Children().Append(SectionHeader(L"更新", true));
  auto check_button = ActionPathButton(L"更新", kFluentIconArrowClockwise20RegularPath, 0.84);
  check_button.Click([](auto const&, auto const&) {
    RunTool(L"fluent-pinyin-updater.exe", L"update");
  });
  page.Children().Append(SettingRowWithIcon(L"更新",
                                            L"从 GitHub 下载并安装最新版本。",
                                            check_button,
                                            FluentPathIcon(kFluentIconArrowClockwise20RegularPath, 0.94),
                                            L"已接通"));
  page.Children().Append(SectionHeader(L"版本信息"));
  Grid version_spacer;
  version_spacer.Width(1);
  version_spacer.Height(1);
  page.Children().Append(SettingRowWithIcon(std::wstring(fp::kProductName),
                                            L"版本 " + std::wstring(fp::kProductVersion),
                                            version_spacer,
                                            TextIcon(L"畅"),
                                            L"",
                                            1.0));
  auto repo_button = ActionPathButton(L"打开 GitHub", kGitHubMark24Path, 0.76, 24.0);
  repo_button.Click([](auto const&, auto const&) {
    OpenUrl(fp::kGitHubRepoUrl);
  });
  page.Children().Append(SettingRowWithIcon(L"GitHub 开源地址",
                                            std::wstring(fp::kGitHubRepoUrl),
                                            repo_button,
                                            FluentPathIcon(kGitHubMark24Path, 0.78, 24.0),
                                            L"开源"));
  return Scroll(page);
}

}  // namespace fp::config_winui
