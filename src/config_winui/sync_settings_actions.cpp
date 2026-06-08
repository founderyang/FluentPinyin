#include "config_winui/sync_settings_actions.h"

#include "common/constants.h"
#include "config_winui/app_paths.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/theme_helpers.h"

#include <windows.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace fp::config_winui {
namespace {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

void ShowSyncMessage(std::wstring_view title, std::wstring_view message, UINT flags = MB_OK) {
  MessageBoxW(SettingsWindowHandle(),
              std::wstring(message).c_str(),
              std::wstring(title).c_str(),
              flags);
}

void SaveProtectedSyncSetting(std::wstring_view key, std::wstring_view value) {
  if (auto protected_value = fp::sync::ProtectSecretText(value)) {
    WriteStringSetting(std::wstring(key) + L"_protected", *protected_value);
    WriteStringSetting(key, L"");
  } else {
    WriteStringSetting(key, value);
  }
}

TextBox SyncDialogTextBox(std::wstring_view text,
                          std::wstring_view placeholder) {
  TextBox box;
  ApplySettingsUIFont(box);
  box.MinWidth(260);
  box.HorizontalAlignment(HorizontalAlignment::Stretch);
  box.Text(text);
  box.PlaceholderText(placeholder);
  return box;
}

PasswordBox SyncDialogPasswordBox(std::wstring_view text,
                                  std::wstring_view placeholder) {
  PasswordBox box;
  ApplySettingsUIFont(box);
  box.MinWidth(260);
  box.HorizontalAlignment(HorizontalAlignment::Stretch);
  box.Password(text);
  box.PlaceholderText(placeholder);
  return box;
}

UIElement SyncDialogField(std::wstring_view label_text, UIElement const& control) {
  Grid row;
  row.ColumnSpacing(12);
  row.MinHeight(36);
  ColumnDefinition label_column;
  label_column.Width(GridLengthHelper::FromPixels(134));
  ColumnDefinition input_column;
  input_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  row.ColumnDefinitions().Append(label_column);
  row.ColumnDefinitions().Append(input_column);

  auto label = Text(label_text, 13, FW_SEMIBOLD);
  label.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(label, 0);
  row.Children().Append(label);

  if (auto framework = control.try_as<FrameworkElement>()) {
    Grid::SetColumn(framework, 1);
    row.Children().Append(framework);
  } else {
    row.Children().Append(control);
  }
  return row;
}

}  // namespace

fp::sync::SyncConfig CurrentSyncConfigFromSettings() {
  ResetSettingsCache();
  return fp::sync::LoadConfig(SettingsPath());
}

void ApplyAutoSyncSchedule(bool enabled, int interval_minutes) {
  WriteBoolSetting(fp::kSyncAutoEnabledSetting, enabled);
  WriteIntSetting(fp::kSyncAutoIntervalMinutesSetting, interval_minutes);
  const auto result =
      enabled ? fp::sync::InstallScheduledSync(SiblingExe(L"fluent-pinyin-settings.exe"),
                                               interval_minutes)
              : fp::sync::RemoveScheduledSync();
  if (!result.success) {
    ShowSyncMessage(L"定时同步", result.message, MB_OK | MB_ICONERROR);
  }
}

void RunSyncActionAsync(std::wstring title,
                        std::function<fp::sync::SyncResult()> action,
                        bool refresh_input_config) {
  auto hwnd = SettingsWindowHandle();
  std::thread([title = std::move(title),
               action = std::move(action),
               refresh_input_config,
               hwnd]() mutable {
    const auto result = action();
    if (refresh_input_config && result.success) {
      RequestApplyInputConfig();
    }
    MessageBoxW(hwnd,
                result.message.c_str(),
                title.c_str(),
                result.success ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONERROR);
  }).detach();
}

void ShowSyncProviderDialog(XamlRoot const& xaml_root) {
  ContentDialog dialog;
  ApplySettingsDialogBase(dialog, xaml_root);
  dialog.PrimaryButtonText(L"保存");
  dialog.CloseButtonText(L"取消");
  dialog.DefaultButton(ContentDialogButton::Primary);
  ApplySettingsResources(dialog.Resources());
  dialog.Resources().Insert(box_value(L"ContentDialogPadding"),
                            box_value(Thickness{24, 16, 24, 8}));
  dialog.Resources().Insert(box_value(L"ContentDialogCommandSpaceMargin"),
                            box_value(Thickness{0, 6, 0, 0}));
  dialog.Resources().Insert(box_value(L"ContentDialogTitleMargin"),
                            box_value(Thickness{0, 0, 0, 10}));

  const auto config = CurrentSyncConfigFromSettings();

  StackPanel content;
  content.Width(kSettingsDialogContentWidth);
  content.MaxWidth(kSettingsDialogContentWidth);
  content.Spacing(8);
  content.HorizontalAlignment(HorizontalAlignment::Stretch);
  content.Children().Append(Text(L"同步配置", 20, FW_SEMIBOLD));
  auto description = Text(L"同步包会先加密，再上传到云端。", 12);
  description.Foreground(SettingsSecondaryTextBrush());
  description.TextWrapping(TextWrapping::Wrap);
  content.Children().Append(description);

  ComboBox provider_combo;
  ApplySettingsUIFont(provider_combo);
  provider_combo.MinWidth(220);
  provider_combo.HorizontalAlignment(HorizontalAlignment::Left);
  provider_combo.Items().Append(box_value(L"对象存储"));
  provider_combo.Items().Append(box_value(L"WebDAV"));
  provider_combo.SelectedIndex(config.provider == fp::sync::SyncProvider::WebDav ? 1 : 0);
  content.Children().Append(SyncDialogField(L"同步提供商", provider_combo));

  auto encryption_secret =
      SyncDialogPasswordBox(config.encryption_secret, L"至少 8 个字符，所有设备保持一致");
  content.Children().Append(SyncDialogField(L"加密口令", encryption_secret));

  Grid provider_host;
  provider_host.Height(kSyncProviderDialogFormViewportHeight);
  provider_host.MinHeight(kSyncProviderDialogFormViewportHeight);
  provider_host.MaxHeight(kSyncProviderDialogFormViewportHeight);
  provider_host.HorizontalAlignment(HorizontalAlignment::Stretch);

  auto object_panel = StackPanel();
  object_panel.Spacing(8);
  object_panel.Margin(Thickness{0, 0, 14, 0});
  auto object_endpoint =
      SyncDialogTextBox(config.object_endpoint, L"https://s3.example.com");
  auto object_bucket = SyncDialogTextBox(config.object_bucket, L"bucket");
  auto object_region = SyncDialogTextBox(config.object_region.empty() ? L"auto"
                                                                      : config.object_region,
                                         L"auto / us-east-1");
  auto object_access_key = SyncDialogTextBox(config.object_access_key, L"Access Key");
  auto object_secret_key = SyncDialogPasswordBox(config.object_secret_key, L"Secret Key");
  auto object_key = SyncDialogTextBox(config.object_key.empty() ? fp::kDefaultSyncObjectKey
                                                                : config.object_key,
                                      L"fluent-pinyin/sync.fpsync");
  object_panel.Children().Append(Text(L"对象存储", 14, FW_SEMIBOLD));
  object_panel.Children().Append(SyncDialogField(L"Endpoint", object_endpoint));
  object_panel.Children().Append(SyncDialogField(L"Bucket", object_bucket));
  object_panel.Children().Append(SyncDialogField(L"Region", object_region));
  object_panel.Children().Append(SyncDialogField(L"Access Key", object_access_key));
  object_panel.Children().Append(SyncDialogField(L"Secret Key", object_secret_key));
  object_panel.Children().Append(SyncDialogField(L"对象路径", object_key));

  auto webdav_panel = StackPanel();
  webdav_panel.Spacing(8);
  webdav_panel.Margin(Thickness{0, 0, 14, 0});
  auto webdav_url = SyncDialogTextBox(config.webdav_url, L"https://dav.example.com/fluent-pinyin");
  auto webdav_username = SyncDialogTextBox(config.webdav_username, L"用户名，可留空");
  auto webdav_password = SyncDialogPasswordBox(config.webdav_password, L"密码，可留空");
  auto webdav_key = SyncDialogTextBox(config.object_key.empty() ? fp::kDefaultSyncObjectKey
                                                                : config.object_key,
                                      L"fluent-pinyin/sync.fpsync");
  webdav_panel.Children().Append(Text(L"WebDAV", 14, FW_SEMIBOLD));
  webdav_panel.Children().Append(SyncDialogField(L"根地址", webdav_url));
  webdav_panel.Children().Append(SyncDialogField(L"用户名", webdav_username));
  webdav_panel.Children().Append(SyncDialogField(L"密码", webdav_password));
  webdav_panel.Children().Append(SyncDialogField(L"文件路径", webdav_key));

  ScrollViewer object_scroller;
  object_scroller.Content(object_panel);
  object_scroller.Height(kSyncProviderDialogFormViewportHeight - 20.0);
  object_scroller.MaxHeight(kSyncProviderDialogFormViewportHeight - 20.0);
  object_scroller.HorizontalAlignment(HorizontalAlignment::Stretch);
  object_scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
  object_scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
  object_scroller.VerticalScrollMode(ScrollMode::Enabled);
  object_scroller.HorizontalScrollMode(ScrollMode::Disabled);
  object_scroller.ZoomMode(ZoomMode::Disabled);
  object_scroller.BringIntoViewOnFocusChange(true);

  ScrollViewer webdav_scroller;
  webdav_scroller.Content(webdav_panel);
  webdav_scroller.Height(kSyncProviderDialogFormViewportHeight - 20.0);
  webdav_scroller.MaxHeight(kSyncProviderDialogFormViewportHeight - 20.0);
  webdav_scroller.HorizontalAlignment(HorizontalAlignment::Stretch);
  webdav_scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
  webdav_scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
  webdav_scroller.VerticalScrollMode(ScrollMode::Enabled);
  webdav_scroller.HorizontalScrollMode(ScrollMode::Disabled);
  webdav_scroller.ZoomMode(ZoomMode::Disabled);
  webdav_scroller.BringIntoViewOnFocusChange(true);

  auto object_frame = SettingsFrame(object_scroller,
                                    CurrentSettingsPalette().card,
                                    Radius(8),
                                    Thickness{12, 10, 12, 10});
  object_frame.Height(kSyncProviderDialogFormViewportHeight);
  object_frame.MinHeight(kSyncProviderDialogFormViewportHeight);

  auto webdav_frame = SettingsFrame(webdav_scroller,
                                    CurrentSettingsPalette().card,
                                    Radius(8),
                                    Thickness{12, 10, 12, 10});
  webdav_frame.Height(kSyncProviderDialogFormViewportHeight);
  webdav_frame.MinHeight(kSyncProviderDialogFormViewportHeight);

  provider_host.Children().Append(object_frame);
  provider_host.Children().Append(webdav_frame);
  content.Children().Append(provider_host);

  auto update_provider_panels = std::make_shared<std::function<void()>>();
  *update_provider_panels = [provider_combo, object_frame, webdav_frame]() {
    const bool webdav = provider_combo.SelectedIndex() == 1;
    object_frame.Visibility(webdav ? Visibility::Collapsed : Visibility::Visible);
    webdav_frame.Visibility(webdav ? Visibility::Visible : Visibility::Collapsed);
  };
  provider_combo.SelectionChanged([update_provider_panels](auto const&, auto const&) {
    (*update_provider_panels)();
  });
  (*update_provider_panels)();

  dialog.Content(content);
  auto operation = dialog.ShowAsync();
  operation.Completed([=](auto const& async, auto const& status) {
    if (status != Windows::Foundation::AsyncStatus::Completed ||
        async.GetResults() != ContentDialogResult::Primary) {
      return;
    }

    const bool webdav = provider_combo.SelectedIndex() == 1;
    WriteStringSetting(fp::kSyncProviderSetting, webdav ? L"webdav" : L"object");
    WriteStringSetting(fp::kSyncObjectEndpointSetting, object_endpoint.Text().c_str());
    WriteStringSetting(fp::kSyncObjectBucketSetting, object_bucket.Text().c_str());
    WriteStringSetting(fp::kSyncObjectRegionSetting, object_region.Text().c_str());
    WriteStringSetting(fp::kSyncObjectAccessKeySetting, object_access_key.Text().c_str());
    SaveProtectedSyncSetting(fp::kSyncObjectSecretKeySetting,
                             object_secret_key.Password().c_str());
    WriteStringSetting(fp::kSyncWebDavUrlSetting, webdav_url.Text().c_str());
    WriteStringSetting(fp::kSyncWebDavUsernameSetting, webdav_username.Text().c_str());
    SaveProtectedSyncSetting(fp::kSyncWebDavPasswordSetting,
                             webdav_password.Password().c_str());
    WriteStringSetting(fp::kSyncObjectKeySetting,
                       webdav ? std::wstring(webdav_key.Text()) : std::wstring(object_key.Text()));
    SaveProtectedSyncSetting(fp::kSyncEncryptionSecretSetting,
                             encryption_secret.Password().c_str());
    ResetSettingsCache();
  });
}

}  // namespace fp::config_winui
