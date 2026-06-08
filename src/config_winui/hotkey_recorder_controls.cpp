#include "config_winui/hotkey_recorder_controls.h"

#include "config_winui/hotkey_helpers.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/theme_helpers.h"

#include <windows.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace fp::config_winui {
namespace {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
namespace XamlInput = Microsoft::UI::Xaml::Input;
using Windows::Foundation::IInspectable;

std::wstring ShortcutConflictTooltip(std::wstring_view current_key,
                                     std::wstring_view display) {
  return fp::config_winui::ShortcutConflictTooltip(
      current_key, display, [](std::wstring_view key, std::wstring_view fallback) {
        return ReadStringSetting(key, fallback);
      });
}

}  // namespace

Button HotkeyRecorderButton(std::wstring_view key,
                            std::wstring_view fallback) {
  const auto palette = CurrentSettingsPalette();
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(160);
  button.HorizontalAlignment(HorizontalAlignment::Right);
  button.Padding(Thickness{12, 6, 12, 7});
  button.Background(Brush(palette.button));
  button.BorderBrush(SettingsBorderBrush());
  button.BorderThickness(SettingsHairlineThickness());
  button.CornerRadius(Radius(8));
  const auto background = Brush(palette.button).as<IInspectable>();
  const auto hover = Brush(palette.button_hover).as<IInspectable>();
  const auto pressed = Brush(palette.button_pressed).as<IInspectable>();
  const auto border = SettingsBorderBrush().as<IInspectable>();
  const auto border_hover = SettingsBorderHoverBrush().as<IInspectable>();
  const auto foreground = Brush(palette.text).as<IInspectable>();
  button.Resources().Insert(box_value(L"ButtonBackground"), background);
  button.Resources().Insert(box_value(L"ButtonBackgroundPointerOver"), hover);
  button.Resources().Insert(box_value(L"ButtonBackgroundPressed"), pressed);
  button.Resources().Insert(box_value(L"ButtonBorderBrush"), border);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPointerOver"), border_hover);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPressed"), border_hover);
  button.Resources().Insert(box_value(L"ButtonForeground"), foreground);
  button.Resources().Insert(box_value(L"ButtonForegroundPointerOver"), foreground);
  button.Resources().Insert(box_value(L"ButtonForegroundPressed"), foreground);

  auto label = Text(L"", 13, FW_SEMIBOLD);
  label.TextWrapping(TextWrapping::NoWrap);
  label.HorizontalAlignment(HorizontalAlignment::Center);
  label.VerticalAlignment(VerticalAlignment::Center);
  button.Content(label);

  const auto apply_text = std::make_shared<std::function<void(std::wstring_view)>>();
  *apply_text = [button, label, key = std::wstring(key)](std::wstring_view value) {
    const std::wstring normalized = NormalizeShortcutDisplay(value);
    label.Text(normalized.empty() ? L"未设置" : normalized);
    label.Foreground(normalized.empty() ? SettingsSecondaryTextBrush() : SettingsTextBrush());
    SetSettingsToolTip(button, ShortcutConflictTooltip(key, normalized));
  };

  (*apply_text)(ReadStringSetting(key, fallback));
  button.Click([button](auto const&, auto const&) {
    button.Focus(FocusState::Programmatic);
  });
  button.GotFocus([label](auto const&, auto const&) {
    label.Text(L"按下组合键");
    label.Foreground(SettingsSecondaryTextBrush());
  });
  button.LostFocus([apply_text, key = std::wstring(key), fallback = std::wstring(fallback)](
                       auto const&, auto const&) {
    (*apply_text)(ReadStringSetting(key, fallback));
  });
  const auto pending_modifier_key = std::make_shared<WPARAM>(0);
  const auto pending_modifier_display = std::make_shared<std::wstring>();
  button.PreviewKeyDown([button,
                         apply_text,
                         pending_modifier_key,
                         pending_modifier_display,
                         key = std::wstring(key)](IInspectable const&,
                                                  XamlInput::KeyRoutedEventArgs const& args) {
    args.Handled(true);
    const WPARAM virtual_key = static_cast<WPARAM>(args.OriginalKey());
    if (virtual_key == VK_ESCAPE) {
      *pending_modifier_key = 0;
      pending_modifier_display->clear();
      (*apply_text)(ReadStringSetting(key, L""));
      return;
    }
    if (IsShortcutModifierKey(virtual_key)) {
      const std::wstring modifier = RecordedShortcutFromKey(virtual_key);
      if (modifier.empty()) {
        *pending_modifier_key = 0;
        pending_modifier_display->clear();
        return;
      }
      *pending_modifier_key = virtual_key;
      *pending_modifier_display = modifier;
      if (auto label = button.Content().try_as<TextBlock>()) {
        label.Text(modifier + L"+");
      }
      return;
    }

    *pending_modifier_key = 0;
    pending_modifier_display->clear();
    const std::wstring shortcut = RecordedShortcutFromKey(virtual_key);
    if (shortcut.empty()) {
      return;
    }
    if (WriteStringSetting(key, shortcut)) {
      RequestInputStateRefreshDeferred(80);
    }
    (*apply_text)(shortcut);
  });
  button.PreviewKeyUp([apply_text,
                       pending_modifier_key,
                       pending_modifier_display,
                       key = std::wstring(key)](IInspectable const&,
                                                XamlInput::KeyRoutedEventArgs const& args) {
    args.Handled(true);
    const WPARAM virtual_key = static_cast<WPARAM>(args.OriginalKey());
    if (*pending_modifier_key == 0 ||
        !ShortcutModifierKeyEquals(*pending_modifier_key, virtual_key) ||
        pending_modifier_display->empty()) {
      return;
    }

    const std::wstring shortcut = *pending_modifier_display;
    *pending_modifier_key = 0;
    pending_modifier_display->clear();
    if (WriteStringSetting(key, shortcut)) {
      RequestInputStateRefreshDeferred(80);
    }
    (*apply_text)(shortcut);
  });
  return button;
}

void SetHotkeyRecorderDisplay(Button const& button,
                              std::wstring_view key,
                              std::wstring_view display) {
  const std::wstring normalized = NormalizeShortcutDisplay(display);
  if (auto label = button.Content().try_as<TextBlock>()) {
    label.Text(normalized.empty() ? L"未设置" : normalized);
    label.Foreground(normalized.empty() ? SettingsSecondaryTextBrush() : SettingsTextBrush());
  }
  SetSettingsToolTip(button, ShortcutConflictTooltip(key, normalized));
}

}  // namespace fp::config_winui
