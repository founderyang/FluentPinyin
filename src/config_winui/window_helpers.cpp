#include "config_winui/window_helpers.h"

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

}  // namespace fp::config_winui
