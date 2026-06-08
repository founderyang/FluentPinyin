#include "config_winui/sync_page.h"

#include "common/constants.h"
#include "config_winui/file_dialogs.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/settings_value_controls.h"
#include "config_winui/sync_settings_actions.h"
#include "sync/sync_service.h"

#include <windows.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <string>
#include <string_view>

namespace fp::config_winui {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

UIElement BuildSyncPage(HWND owner, XamlRoot const& xaml_root) {
  auto page = PageShell(L"同步", L"剪贴板、配置、词库和备份。");
  page.Children().Append(SectionHeader(L"同步内容", true));
  page.Children().Append(SettingRowWithIcon(L"剪贴板同步",
                                            L"在设备之间同步剪贴板。",
                                            SettingSwitch(fp::kSyncClipboardSetting, false),
                                            FluentPathIcon(kFluentIconClipboard20RegularPath, 0.94),
                                            L"已保存"));
  page.Children().Append(SettingRowWithIcon(L"配置和词库同步",
                                            L"同步设置、词库和输入数据。",
                                            SettingSwitch(fp::kSyncUserDataSetting, false),
                                            FluentPathIcon(kFluentIconBookDatabase20RegularPath, 0.90),
                                            L"已保存"));
  page.Children().Append(SectionHeader(L"云端服务"));
  auto edit_provider = StableIconToolButton(
      kFluentIconEditSettings20RegularPath,
      L"同步配置",
      0.86,
      [xaml_root]() {
        ShowSyncProviderDialog(xaml_root);
      });
  page.Children().Append(SettingRowWithIcon(
      L"同步提供商",
      L"编辑云端连接和加密配置。",
      edit_provider,
      FluentPathIcon(kFluentIconCloud20RegularPath, 0.94),
      L"需配置",
      48.0));

  StackPanel sync_actions;
  sync_actions.Orientation(Orientation::Horizontal);
  sync_actions.Spacing(10);
  auto upload = ActionPathButton(L"上传", kFluentIconArrowUpload20RegularPath, 0.82);
  upload.Click([](auto const&, auto const&) {
    const auto config = CurrentSyncConfigFromSettings();
    RunSyncActionAsync(L"同步上传",
                       [config]() { return fp::sync::UploadNow(config); },
                       false);
  });
  auto download = ActionPathButton(L"下载", kFluentIconArrowDownload20RegularPath, 0.84);
  download.Click([](auto const&, auto const&) {
    const auto config = CurrentSyncConfigFromSettings();
    RunSyncActionAsync(L"同步下载",
                       [config]() {
                         fp::sync::PackageMetadata metadata;
                         return fp::sync::DownloadNow(config, true, &metadata);
                       },
                       true);
  });
  sync_actions.Children().Append(upload);
  sync_actions.Children().Append(download);
  page.Children().Append(SettingWideRowWithIcon(L"立即同步",
                                                L"上传或下载同步包。",
                                                sync_actions,
                                                FluentPathIcon(kFluentIconCloudSync20RegularPath, 0.94),
                                                L"已接入"));

  page.Children().Append(SectionHeader(L"定时自动同步"));
  const int current_interval =
      ReadIntSetting(fp::kSyncAutoIntervalMinutesSetting,
                     fp::kDefaultSyncAutoIntervalMinutes,
                     fp::kMinSyncAutoIntervalMinutes,
                     fp::kMaxSyncAutoIntervalMinutes);
  auto auto_sync_switch = SettingSwitch(
      fp::kSyncAutoEnabledSetting,
      false,
      [current_interval](bool enabled) {
        ApplyAutoSyncSchedule(enabled,
                              ReadIntSetting(fp::kSyncAutoIntervalMinutesSetting,
                                             current_interval,
                                             fp::kMinSyncAutoIntervalMinutes,
                                             fp::kMaxSyncAutoIntervalMinutes));
      });
  page.Children().Append(SettingRowWithIcon(L"自动同步",
                                            L"按固定间隔自动同步。",
                                            auto_sync_switch,
                                            FluentPathIcon(kFluentIconCalendarClock20RegularPath, 0.92),
                                            L"计划任务"));
  page.Children().Append(SettingRowWithIcon(
      L"同步间隔",
      L"设置自动同步频率。",
      StringChoiceCombo(fp::kSyncAutoIntervalMinutesSetting,
                        SyncAutoIntervalChoices(),
                        std::to_wstring(fp::kDefaultSyncAutoIntervalMinutes),
                        L"",
                        false,
                        [](std::wstring_view value) {
                          if (!ReadBoolSetting(fp::kSyncAutoEnabledSetting, false)) {
                            return;
                          }
                          ApplyAutoSyncSchedule(true, SyncAutoIntervalMinutesFromValue(value));
                        }),
      FluentPathIcon(kFluentIconCalendarClock20RegularPath, 0.92),
      L"已保存"));

  page.Children().Append(SectionHeader(L"备份和恢复"));
  StackPanel actions;
  actions.Orientation(Orientation::Horizontal);
  actions.Spacing(10);
  auto backup = ActionPathButton(L"备份配置", kFluentIconArchive20RegularPath, 0.84);
  backup.Click([owner](auto const&, auto const&) {
    const auto selected = PickSyncBackupSaveFile(owner);
    if (!selected) {
      return;
    }
    auto config = CurrentSyncConfigFromSettings();
    config.sync_clipboard = false;
    config.sync_user_data = true;
    RunSyncActionAsync(L"备份配置",
                       [config, path = *selected]() {
                         return fp::sync::CreateLocalBackup(path, config);
                       },
                       false);
  });
  auto restore = ActionPathButton(L"恢复配置", kFluentIconHistory20RegularPath, 0.84);
  restore.Click([owner](auto const&, auto const&) {
    const auto selected = PickSyncBackupFile(owner);
    if (!selected) {
      return;
    }
    const int confirm = MessageBoxW(owner,
                                    L"恢复会覆盖本机设置和用户词库；恢复前会自动备份。是否继续？",
                                    L"恢复配置",
                                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (confirm != IDYES) {
      return;
    }
    auto config = CurrentSyncConfigFromSettings();
    config.sync_clipboard = false;
    config.sync_user_data = true;
    RunSyncActionAsync(L"恢复配置",
                       [config, path = *selected]() {
                         fp::sync::PackageMetadata metadata;
                         return fp::sync::RestoreLocalBackup(path, config, true, &metadata);
                       },
                       true);
  });
  actions.Children().Append(backup);
  actions.Children().Append(restore);
  page.Children().Append(SettingWideRowWithIcon(L"备份和恢复",
                                                L"备份或恢复本机数据。",
                                                actions,
                                                FluentPathIcon(kFluentIconArchive20RegularPath, 0.94),
                                                L"已接入"));
  return Scroll(page);
}

}  // namespace fp::config_winui
