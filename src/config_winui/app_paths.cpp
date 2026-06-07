#include "config_winui/app_paths.h"

#include "common/path_utils.h"

#include <windows.h>

#include <array>
#include <atomic>
#include <system_error>

namespace fp::config_winui {

std::filesystem::path RimeUserDataPath() {
  return fp::GetFpRoamingDataPath() / L"Rime";
}

std::filesystem::path ModuleDirectory() {
  std::wstring buffer(32768, L'\0');
  const DWORD length =
      GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) {
    return std::filesystem::current_path();
  }
  buffer.resize(length);
  return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path SiblingExe(std::wstring_view name) {
  return ModuleDirectory() / std::wstring(name);
}

std::wstring WindowIconPath() {
  const auto module_dir = ModuleDirectory();
  const auto app_icon = module_dir / L"fluent-pinyin.ico";
  if (std::filesystem::exists(app_icon)) {
    return app_icon.wstring();
  }

  DWORD light_theme = 1;
  DWORD size = sizeof(light_theme);
  RegGetValueW(HKEY_CURRENT_USER,
               L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
               L"SystemUsesLightTheme",
               RRF_RT_REG_DWORD,
               nullptr,
               &light_theme,
               &size);
  const auto preferred_icon =
      module_dir / (light_theme != 0 ? L"fluent-pinyin-light.ico"
                                     : L"fluent-pinyin-dark.ico");
  if (std::filesystem::exists(preferred_icon)) {
    return preferred_icon.wstring();
  }
  const auto dark_icon = module_dir / L"fluent-pinyin-dark.ico";
  if (std::filesystem::exists(dark_icon)) {
    return dark_icon.wstring();
  }
  const auto light_icon = module_dir / L"fluent-pinyin-light.ico";
  if (std::filesystem::exists(light_icon)) {
    return light_icon.wstring();
  }
  return {};
}

void EnsureUiFontsLoaded() {
  static std::atomic_bool loaded = false;
  bool expected = false;
  if (!loaded.compare_exchange_strong(expected, true)) {
    return;
  }

  const auto font_dir = ModuleDirectory() / L"fonts";
  constexpr std::array<std::wstring_view, 11> font_files{
      L"MiSans-Regular.ttf",
      L"MiSans-Medium.ttf",
      L"MiSans-Semibold.ttf",
      L"MiSansTC-Regular.ttf",
      L"MiSansTC-Medium.ttf",
      L"MiSansTC-Semibold.ttf",
      L"MiSansL3-Regular.ttf",
      L"SourceHanSansSC-Regular.otf",
      L"SourceHanSansTC-Regular.otf",
      L"PlangothicP1-Regular.ttf",
      L"PlangothicP2-Regular.ttf",
  };
  for (const auto file : font_files) {
    const auto path = font_dir / std::wstring(file);
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
      AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
    }
  }
}

}  // namespace fp::config_winui
