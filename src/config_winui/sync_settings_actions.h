#pragma once

#include "sync/sync_service.h"

#include <winrt/Microsoft.UI.Xaml.h>

#include <functional>
#include <string>
#include <string_view>

namespace fp::config_winui {

fp::sync::SyncConfig CurrentSyncConfigFromSettings();
void ApplyAutoSyncSchedule(bool enabled, int interval_minutes);
void RunSyncActionAsync(std::wstring title,
                        std::function<fp::sync::SyncResult()> action,
                        bool refresh_input_config);
void ShowSyncProviderDialog(winrt::Microsoft::UI::Xaml::XamlRoot const& xaml_root);

}  // namespace fp::config_winui
