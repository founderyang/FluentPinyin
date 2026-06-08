#include "config_winui/window_helpers.h"

#include "config_winui/app_paths.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/theme_helpers.h"
#include "../tsf/resource.h"

#include <windows.h>

#include <winrt/Microsoft.UI.Windowing.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace fp::config_winui {

HWND GetWindowHandle(winrt::Microsoft::UI::Xaml::Window const& window) {
  HWND hwnd = nullptr;
  if (auto native = window.try_as<IWindowNative>()) {
    native->get_WindowHandle(&hwnd);
  }
  return hwnd;
}

bool ActivateWindowByTitle(std::wstring_view title, int attempts) {
  const std::wstring window_title(title);
  for (int attempt = 0; attempt < attempts; ++attempt) {
    const HWND hwnd = FindWindowW(nullptr, window_title.c_str());
    if (hwnd != nullptr) {
      ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOWNORMAL);
      SetForegroundWindow(hwnd);
      return true;
    }
    if (attempt + 1 < attempts) {
      Sleep(50);
    }
  }
  return false;
}

winrt::Windows::Graphics::SizeInt32 DefaultWindowSize(
    winrt::Microsoft::UI::Xaml::Window const& window) {
  const HWND hwnd = GetWindowHandle(window);
  int width = 860;
  int height = 480;
  if (hwnd) {
    MONITORINFOEXW monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (GetMonitorInfoW(monitor, &monitor_info)) {
      const int work_width = monitor_info.rcWork.right - monitor_info.rcWork.left;
      const int work_height = monitor_info.rcWork.bottom - monitor_info.rcWork.top;
      int physical_work_width = work_width;
      int physical_work_height = work_height;

      DEVMODEW mode{};
      mode.dmSize = sizeof(mode);
      const int monitor_width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
      const int monitor_height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
      if (monitor_width > 0 && monitor_height > 0 &&
          EnumDisplaySettingsW(monitor_info.szDevice, ENUM_CURRENT_SETTINGS, &mode)) {
        physical_work_width = std::max(
            1,
            static_cast<int>(std::lround(static_cast<double>(work_width) *
                                         static_cast<double>(mode.dmPelsWidth) /
                                         static_cast<double>(monitor_width))));
        physical_work_height = std::max(
            1,
            static_cast<int>(std::lround(static_cast<double>(work_height) *
                                         static_cast<double>(mode.dmPelsHeight) /
                                         static_cast<double>(monitor_height))));
      }

      width = std::max(1, physical_work_width / 2);
      height = std::max(1, physical_work_height / 2);
    }
  }
  return winrt::Windows::Graphics::SizeInt32{width, height};
}

winrt::Windows::Graphics::SizeInt32 ScaleSizeForDpi(
    winrt::Windows::Graphics::SizeInt32 const& size,
    UINT dpi) {
  if (dpi == 0) {
    dpi = GetDpiForSystem();
  }
  if (dpi == 0) {
    dpi = 96;
  }
  return winrt::Windows::Graphics::SizeInt32{
      MulDiv(size.Width, static_cast<int>(dpi), 96),
      MulDiv(size.Height, static_cast<int>(dpi), 96)};
}

void CenterWindowOnMonitor(winrt::Microsoft::UI::Xaml::Window const& window) {
  const HWND hwnd = GetWindowHandle(window);
  if (!hwnd) {
    return;
  }
  RECT window_rect{};
  MONITORINFO monitor_info{};
  monitor_info.cbSize = sizeof(monitor_info);
  const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  if (!GetWindowRect(hwnd, &window_rect) || !GetMonitorInfoW(monitor, &monitor_info)) {
    return;
  }

  const int window_width = window_rect.right - window_rect.left;
  const int window_height = window_rect.bottom - window_rect.top;
  const int work_width = monitor_info.rcWork.right - monitor_info.rcWork.left;
  const int work_height = monitor_info.rcWork.bottom - monitor_info.rcWork.top;
  const int x = monitor_info.rcWork.left + std::max(0, (work_width - window_width) / 2);
  const int y = monitor_info.rcWork.top + std::max(0, (work_height - window_height) / 2);
  SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void ApplyTitleBarColors(winrt::Microsoft::UI::Xaml::Window const& window) {
  const auto palette = CurrentSettingsPalette();
  auto title_bar = window.AppWindow().TitleBar();
  title_bar.IconShowOptions(Microsoft::UI::Windowing::IconShowOptions::HideIconAndSystemMenu);
  const auto background = ColorReference(0, 0, 0, 0);
  title_bar.BackgroundColor(background);
  title_bar.ForegroundColor(ColorReference(palette.text));
  title_bar.InactiveBackgroundColor(background);
  title_bar.InactiveForegroundColor(ColorReference(palette.muted_text));
  title_bar.ButtonBackgroundColor(background);
  title_bar.ButtonForegroundColor(ColorReference(palette.text));
  title_bar.ButtonHoverBackgroundColor(ColorReference(palette.button_hover));
  title_bar.ButtonHoverForegroundColor(ColorReference(palette.text));
  title_bar.ButtonPressedBackgroundColor(ColorReference(palette.button_pressed));
  title_bar.ButtonPressedForegroundColor(ColorReference(palette.text));
  title_bar.ButtonInactiveBackgroundColor(background);
  title_bar.ButtonInactiveForegroundColor(ColorReference(palette.muted_text));
}

void ApplyWindowIcons(winrt::Microsoft::UI::Xaml::Window const& window) {
  const auto icon_path = WindowIconPath();
  if (icon_path.empty()) {
    return;
  }
  auto app_window = window.AppWindow();
  app_window.SetIcon(icon_path);
  app_window.SetTaskbarIcon(icon_path);
  app_window.TitleBar().IconShowOptions(Microsoft::UI::Windowing::IconShowOptions::HideIconAndSystemMenu);

  const HWND hwnd = GetWindowHandle(window);
  if (hwnd == nullptr) {
    return;
  }
  auto load_icon = [](int cx, int cy) -> HICON {
    return reinterpret_cast<HICON>(
        LoadImageW(GetModuleHandleW(nullptr),
                   MAKEINTRESOURCEW(IDI_APP_ICON),
                   IMAGE_ICON,
                   cx,
                   cy,
                   LR_DEFAULTCOLOR | LR_SHARED));
  };
  if (HICON large_icon = load_icon(GetSystemMetrics(SM_CXICON),
                                   GetSystemMetrics(SM_CYICON))) {
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon));
  }
  if (HICON small_icon = load_icon(GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON))) {
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
  }
}

void ApplyDwmWindowFrame(winrt::Microsoft::UI::Xaml::Window const& window) {
  const HWND hwnd = GetWindowHandle(window);
  if (hwnd == nullptr) {
    return;
  }

  auto dwm = LoadLibraryW(L"dwmapi.dll");
  if (dwm == nullptr) {
    return;
  }
  using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
  auto set_attribute = reinterpret_cast<DwmSetWindowAttributeFn>(
      GetProcAddress(dwm, "DwmSetWindowAttribute"));
  if (set_attribute != nullptr) {
    constexpr DWORD kDwmUseImmersiveDarkMode = 20;
    constexpr DWORD kDwmWindowCornerPreference = 33;
    constexpr DWORD kDwmBorderColor = 34;
    constexpr DWORD kDwmCaptionColor = 35;
    constexpr DWORD kDwmTextColor = 36;
    constexpr DWORD kDwmCornerRound = 2;

    const auto palette = CurrentSettingsPalette();
    const BOOL dark_mode = palette.light ? FALSE : TRUE;
    const DWORD corner = kDwmCornerRound;
    const COLORREF border_color = SettingsEdgeColorRef();
    const COLORREF caption_color =
        RGB(palette.background.red, palette.background.green, palette.background.blue);
    const COLORREF text_color = RGB(palette.text.red, palette.text.green, palette.text.blue);
    set_attribute(hwnd, kDwmUseImmersiveDarkMode, &dark_mode, sizeof(dark_mode));
    set_attribute(hwnd, kDwmWindowCornerPreference, &corner, sizeof(corner));
    set_attribute(hwnd, kDwmBorderColor, &border_color, sizeof(border_color));
    set_attribute(hwnd, kDwmCaptionColor, &caption_color, sizeof(caption_color));
    set_attribute(hwnd, kDwmTextColor, &text_color, sizeof(text_color));
  }
  FreeLibrary(dwm);
}

}  // namespace fp::config_winui
