#include "config_winui/toolbar_visibility_state.h"

#include "common/constants.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"

#include <vector>

namespace fp::config_winui {
namespace {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

std::vector<ToggleSwitch> g_toolbar_visible_switches;
bool g_syncing_toolbar_visible_switches = false;

}  // namespace

ToggleSwitch ToolbarVisibleSwitch(std::function<void(bool)> on_change) {
  ToggleSwitch toggle;
  ApplySettingsUIFont(toggle);
  toggle.MinWidth(0);
  toggle.HorizontalAlignment(HorizontalAlignment::Right);
  toggle.IsOn(ReadBoolSettingMigrated(fp::kToolbarVisibleSetting,
                                      false,
                                      fp::kLegacyToolbarVisibleSetting));
  toggle.Toggled([toggle, on_change](auto const&, auto const&) {
    if (g_syncing_toolbar_visible_switches) {
      return;
    }
    WriteBoolSetting(fp::kToolbarVisibleSetting, toggle.IsOn());
    WriteBoolSetting(fp::kLegacyToolbarVisibleSetting, false);
    RequestInputStateRefresh();
    if (on_change) {
      on_change(toggle.IsOn());
    }
  });
  return toggle;
}

void RegisterToolbarVisibleSwitch(ToggleSwitch const& toggle) {
  g_toolbar_visible_switches.push_back(toggle);
}

void ClearToolbarVisibleSwitches() {
  g_toolbar_visible_switches.clear();
}

void SyncToolbarVisibleSwitchesFromSettings() {
  ResetSettingsCache();
  const bool visible =
      ReadBoolSettingMigrated(fp::kToolbarVisibleSetting,
                              false,
                              fp::kLegacyToolbarVisibleSetting);
  g_syncing_toolbar_visible_switches = true;
  for (auto it = g_toolbar_visible_switches.begin(); it != g_toolbar_visible_switches.end();) {
    if (*it == nullptr) {
      it = g_toolbar_visible_switches.erase(it);
      continue;
    }
    it->IsOn(visible);
    ++it;
  }
  g_syncing_toolbar_visible_switches = false;
}

}  // namespace fp::config_winui
