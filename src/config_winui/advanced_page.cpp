#include "config_winui/advanced_page.h"

#include "common/constants.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/settings_value_controls.h"
#include "config_winui/wanxiang_modes.h"

#include <winrt/Microsoft.UI.Xaml.h>

#include <cstddef>

namespace fp::config_winui {

using namespace winrt;
using namespace Microsoft::UI::Xaml;

UIElement BuildAdvancedPage() {
  auto page = PageShell(L"高级", L"扩展输入模式。");
  page.Children().Append(SectionHeader(L"进阶输入", true));
  page.Children().Append(SettingRowWithIcon(L"人名输入",
                                            L"启用人名候选偏好。",
                                            RimeConfigSwitch(fp::kNameInputSetting, true),
                                            TextIcon(L"名"),
                                            L"已保存"));
  page.Children().Append(SectionHeader(L"基础模式"));
  const auto& modes = WanxiangModeDefinitions();
  for (size_t index = 0; index < modes.size(); ++index) {
    if (index == 10) {
      page.Children().Append(SectionHeader(L"扩展模式"));
    }
    const auto& mode = modes[index];
    page.Children().Append(SettingRowWithIcon(mode.title,
                                              mode.subtitle,
                                              WanxiangModeSwitch(mode),
                                              WanxiangModeIcon(mode.icon),
                                              L"已联动"));
  }
  return Scroll(page);
}

}  // namespace fp::config_winui
