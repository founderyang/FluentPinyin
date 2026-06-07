#include "common/constants.h"
#include "common/encoding.h"
#include "common/path_utils.h"
#include "common/theme.h"
#include "config_winui/app_paths.h"
#include "config_winui/fuzzy_pinyin_custom_rules.h"
#include "config_winui/hotkey_helpers.h"
#include "config_winui/lexicon_store.h"
#include "config_winui/settings_binding.h"
#include "config_winui/status_tip_blacklist.h"
#include "config_winui/theme_helpers.h"
#include "config_winui/window_helpers.h"
#include "sync/sync_service.h"
#include "../tsf/resource.h"

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <MddBootstrap.h>
#include <microsoft.ui.xaml.window.h>

#undef GetCurrentTime

#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cwctype>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <future>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace Markup = Microsoft::UI::Xaml::Markup;
namespace Media = Microsoft::UI::Xaml::Media;
namespace Primitives = Microsoft::UI::Xaml::Controls::Primitives;
namespace Shapes = Microsoft::UI::Xaml::Shapes;
namespace XamlInput = Microsoft::UI::Xaml::Input;
namespace XamlTypeInfo = Microsoft::UI::Xaml::XamlTypeInfo;
using Windows::Foundation::IInspectable;
using Windows::Foundation::IReference;
using Windows::Graphics::SizeInt32;
using Windows::UI::Color;
using Windows::UI::Text::FontWeights;
using Windows::UI::Xaml::Interop::TypeName;
using fp::config_winui::ReadBoolSetting;
using fp::config_winui::ReadBoolSettingMigrated;
using fp::config_winui::ReadIntSetting;
using fp::config_winui::ReadStringSetting;
using fp::config_winui::RimeUserDataPath;
using fp::config_winui::ResetSettingsCache;
using fp::config_winui::SettingsPath;
using fp::config_winui::SiblingExe;
using fp::config_winui::ClearSettingsPaletteOverride;
using fp::config_winui::CurrentSettingsPalette;
using fp::config_winui::CurrentThemeModeSetting;
using fp::config_winui::CurrentThemePresetSetting;
using fp::config_winui::DefaultPresetForThemeMode;
using fp::config_winui::EffectiveThemePreset;
using fp::config_winui::FuzzyPinyinRuleSelected;
using fp::config_winui::JoinFuzzyPinyinCustomRules;
using fp::config_winui::NormalizeFuzzyPinyinCustomRuleToken;
using fp::config_winui::ParseFuzzyPinyinCustomRuleSetting;
using fp::config_winui::SplitFuzzyPinyinCustomRule;
using fp::config_winui::NormalizeShortcutDisplay;
using fp::config_winui::ShortcutDisplayForKey;
using fp::config_winui::IsShortcutModifierKey;
using fp::config_winui::ShortcutModifierKeyEquals;
using fp::config_winui::RecordedShortcutFromKey;
using fp::config_winui::NormalizeStatusTipBlacklistToken;
using fp::config_winui::ParseStatusTipBlacklistSetting;
using fp::config_winui::StatusTipBlacklistContains;
using fp::config_winui::JoinStatusTipBlacklistItems;
using fp::config_winui::CurrentCustomPhraseCount;
using fp::config_winui::CurrentManagedDictionaryCount;
using fp::config_winui::CurrentUserLexiconCount;
using fp::config_winui::IsSafeRimeDictName;
using fp::config_winui::ManagedDictionaryEntry;
using fp::config_winui::NormalizePhraseWeight;
using fp::config_winui::PhraseEntry;
using fp::config_winui::ReadCustomPhraseEntries;
using fp::config_winui::ReadFileUtf8;
using fp::config_winui::ReadManagedDictionaryManifest;
using fp::config_winui::ReadUserLexiconEntries;
using fp::config_winui::RemoveManagedDictionaryFile;
using fp::config_winui::TrimLexiconText;
using fp::config_winui::WriteCustomPhraseEntries;
using fp::config_winui::WriteFileUtf8;
using fp::config_winui::WriteUserLexiconEntries;
using fp::config_winui::SetSettingsPaletteOverride;
using fp::config_winui::SettingsThemePalette;
using fp::config_winui::ThemeModeDisplayText;
using fp::config_winui::ThemeModeIndex;
using fp::config_winui::ThemeModeValueForIndex;
using fp::config_winui::ThemePreview;
using fp::config_winui::ThemePreviewPalette;
using fp::config_winui::CenterWindowOnMonitor;
using fp::config_winui::DefaultWindowSize;
using fp::config_winui::GetWindowHandle;
using fp::config_winui::ScaleSizeForDpi;
using fp::config_winui::WriteBoolSetting;
using fp::config_winui::WriteIntSetting;
using fp::config_winui::WriteStringSetting;
using fp::config_winui::EnsureUiFontsLoaded;
using fp::config_winui::ModuleDirectory;
using fp::config_winui::WindowIconPath;

constexpr int kDefaultCandidateCount = 7;
constexpr int kMinCandidateCount = 3;
constexpr int kMaxCandidateCount = 9;
constexpr int kDefaultCandidateFontSizeLevel = 0;
constexpr int kMinCandidateFontSizeLevel = 0;
constexpr int kMaxCandidateFontSizeLevel = 3;
constexpr std::array<std::wstring_view, 4> kCandidateFontSizeLabels{L"小", L"中", L"大", L"特大"};
constexpr std::wstring_view kSettingsUiFontFamily = L"MiSans";
constexpr std::wstring_view kDefaultCandidateFontFamily = L"misans";
constexpr std::wstring_view kDefaultStatusTipBlacklist =
    L"explorer.exe,fluent-pinyin-settings.exe,ShellExperienceHost.exe,StartMenuExperienceHost.exe";
constexpr std::wstring_view kDefaultFuzzyPinyinRules =
    L"nl,ry,hf,rl,kg,en_eng,in_ing,c_ch,z_zh,s_sh";
constexpr std::wstring_view kCommonFuzzyPinyinRules =
    L"en_eng,in_ing,c_ch,z_zh,s_sh";
constexpr std::wstring_view kSettingsAppTitle = L"流畅拼音输入法设置";
constexpr std::wstring_view kSettingsSingleInstanceMutexName =
    L"Local\\FluentPinyinSettingsSingleInstance";
constexpr double kSettingIconBackdropSize = 36.0;
constexpr double kSettingIconBackdropRadius = 8.0;
constexpr double kSettingIconHostSize = 22.0;
constexpr double kSettingIconVisualSize = 18.0;
constexpr double kInlineButtonIconHostSize = 16.0;
constexpr double kActionButtonIconHostSize = 18.0;
constexpr double kNavigationOpenPaneLength = 248.0;
constexpr double kTitleBarDragHeight = 40.0;
constexpr double kTitleBarCaptionButtonReservedWidth = 150.0;
constexpr double kContentFrameLeftInset = 12.0;
constexpr double kContentFrameTopInset = 48.0;
constexpr double kContentFrameRightInset = 24.0;
constexpr double kContentFrameBottomInset = 22.0;
constexpr double kSettingsDialogContentWidth = 500.0;
constexpr double kSettingsDialogOuterWidth = 548.0;
constexpr double kSettingsDialogListHeight = 160.0;
constexpr double kSyncProviderDialogFormViewportHeight = 220.0;
constexpr std::wstring_view kToolbarVisibleSetting = L"toolbar_visible_v2";
constexpr std::wstring_view kLegacyToolbarVisibleSetting = L"toolbar_visible";
constexpr std::wstring_view kWindowsAppRuntimeInstallerName =
    L"windowsappruntimeinstall-x64.exe";
constexpr int kMinSettingsWindowWidthDips = 860;
constexpr int kMinSettingsWindowHeightDips = 480;

WNDPROC g_settings_window_proc = nullptr;
HWND g_settings_window_hwnd = nullptr;
SizeInt32 g_min_settings_window_size_dips{kMinSettingsWindowWidthDips,
                                          kMinSettingsWindowHeightDips};
std::vector<ToggleSwitch> g_toolbar_visible_switches;
bool g_syncing_toolbar_visible_switches = false;

std::wstring InitialPageValue() {
  const wchar_t* command_line = GetCommandLineW();
  if (command_line == nullptr) {
    return L"";
  }

  const std::wstring command(command_line);
  const std::wstring marker = L"--page=";
  const size_t marker_pos = command.find(marker);
  if (marker_pos == std::wstring::npos) {
    return L"";
  }

  size_t value_start = marker_pos + marker.size();
  if (value_start < command.size() && command[value_start] == L'"') {
    ++value_start;
  }
  size_t value_end = command.find_first_of(L" \"", value_start);
  if (value_end == std::wstring::npos) {
    value_end = command.size();
  }

  return command.substr(value_start, value_end - value_start);
}

std::wstring InitialPageTag() {
  const std::wstring tag = InitialPageValue();
  if (tag.empty()) {
    return L"general";
  }

  if (tag == L"phrases" || tag == L"custom-phrases" || tag == L"custom_phrases" ||
      tag == L"dictionaries" || tag == L"lexicon-management" ||
      tag == L"lexicon_management" || tag == L"domain" || tag == L"imports") {
    return L"lexicon";
  }
  if (tag == L"keys") {
    return L"hotkeys";
  }
  if (tag == L"input") {
    return L"general";
  }
  if (tag == L"general" || tag == L"advanced" || tag == L"appearance" ||
      tag == L"lexicon" || tag == L"hotkeys" || tag == L"sync" || tag == L"about") {
    return tag;
  }
  return L"general";
}

Color ColorFromArgb(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) {
  return Color{alpha, red, green, blue};
}

Color ColorFromRgb(uint8_t red, uint8_t green, uint8_t blue) {
  return ColorFromArgb(255, red, green, blue);
}

SolidColorBrush Brush(uint8_t red, uint8_t green, uint8_t blue) {
  return SolidColorBrush(ColorFromRgb(red, green, blue));
}

SolidColorBrush Brush(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) {
  return SolidColorBrush(ColorFromArgb(alpha, red, green, blue));
}

SolidColorBrush TransparentBrush() {
  return SolidColorBrush(Windows::UI::Colors::Transparent());
}

FontFamily SettingsUiFontFamily() {
  return FontFamily(kSettingsUiFontFamily);
}

void ApplySettingsUIFont(Control const& control) {
  control.FontFamily(SettingsUiFontFamily());
}

void ApplySettingsUIFont(TextBlock const& text) {
  text.FontFamily(SettingsUiFontFamily());
}

void ApplySettingsUIFont(CheckBox const& check) {
  check.FontFamily(SettingsUiFontFamily());
}

IReference<Color> ColorReference(uint8_t red, uint8_t green, uint8_t blue) {
  return box_value(ColorFromRgb(red, green, blue)).as<IReference<Color>>();
}

IReference<Color> ColorReference(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) {
  return box_value(ColorFromArgb(alpha, red, green, blue)).as<IReference<Color>>();
}

CornerRadius Radius(double value) {
  return CornerRadius{value, value, value, value};
}

Thickness UniformThickness(double value) {
  return Thickness{value, value, value, value};
}

double SettingsHairlineDip() {
  UINT dpi = g_settings_window_hwnd != nullptr ? GetDpiForWindow(g_settings_window_hwnd) : 0;
  if (dpi == 0) {
    dpi = GetDpiForSystem();
  }
  dpi = std::max<UINT>(dpi, 96);
  return 96.0 / static_cast<double>(dpi);
}

Thickness SettingsHairlineThickness() {
  return UniformThickness(SettingsHairlineDip());
}

using ThemeColor = fp::ThemeColor;

SolidColorBrush Brush(ThemeColor color) {
  return Brush(color.red, color.green, color.blue);
}

IReference<Color> ColorReference(ThemeColor color) {
  return ColorReference(color.red, color.green, color.blue);
}

SolidColorBrush SettingsSurfaceBrush() {
  return Brush(CurrentSettingsPalette().background);
}

SolidColorBrush SettingsCardBrush() {
  return Brush(CurrentSettingsPalette().card);
}

ThemeColor SettingsBorderColor() {
  return CurrentSettingsPalette().border;
}

ThemeColor SettingsBorderHoverColor() {
  return CurrentSettingsPalette().edge;
}

SolidColorBrush SettingsBorderBrush() {
  return Brush(SettingsBorderColor());
}

SolidColorBrush SettingsBorderHoverBrush() {
  return Brush(SettingsBorderHoverColor());
}

SolidColorBrush SettingsIconBrush() {
  return Brush(CurrentSettingsPalette().icon);
}

SolidColorBrush SettingsTextBrush() {
  return Brush(CurrentSettingsPalette().text);
}

SolidColorBrush SettingsSecondaryTextBrush() {
  return Brush(CurrentSettingsPalette().secondary_text);
}

ThemeColor SettingsEdgeColor() {
  return CurrentSettingsPalette().border;
}

SolidColorBrush SettingsEdgeBrush() {
  return Brush(SettingsEdgeColor());
}

COLORREF SettingsEdgeColorRef() {
  const ThemeColor color = SettingsEdgeColor();
  return RGB(color.red, color.green, color.blue);
}

Border SettingsFrame(UIElement const& child,
                     ThemeColor surface,
                     CornerRadius radius,
                     Thickness content_padding = UniformThickness(0),
                     Thickness margin = UniformThickness(0)) {
  Border frame;
  frame.Background(Brush(surface));
  frame.BorderBrush(SettingsBorderBrush());
  frame.BorderThickness(SettingsHairlineThickness());
  frame.CornerRadius(radius);
  frame.Padding(content_padding);
  frame.Margin(margin);
  frame.HorizontalAlignment(HorizontalAlignment::Stretch);
  frame.VerticalAlignment(VerticalAlignment::Stretch);
  frame.UseLayoutRounding(true);
  frame.Child(child);
  return frame;
}

ElementTheme CurrentSettingsElementTheme() {
  return CurrentSettingsPalette().light ? ElementTheme::Light : ElementTheme::Dark;
}

ApplicationTheme CurrentSettingsApplicationTheme() {
  return CurrentSettingsPalette().light ? ApplicationTheme::Light : ApplicationTheme::Dark;
}

std::wstring ReadCandidateLayoutSetting(std::wstring_view default_value = L"horizontal") {
  const std::wstring value = ReadStringSetting(L"candidate_layout");
  if (value == L"horizontal" || value == L"vertical") {
    return value;
  }
  return ReadBoolSetting(L"candidate_horizontal", default_value != L"vertical") ? L"horizontal"
                                                                                : L"vertical";
}

std::vector<std::wstring> CurrentStatusTipBlacklistItems() {
  return ParseStatusTipBlacklistSetting(
      ReadStringSetting(L"status_tip_blacklist", kDefaultStatusTipBlacklist));
}

int CurrentStatusTipBlacklistCount() {
  return static_cast<int>(CurrentStatusTipBlacklistItems().size());
}

bool ContainsIgnoreCase(const std::wstring& value, const std::wstring& needle) {
  std::wstring lower_value = value;
  std::wstring lower_needle = needle;
  std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  std::transform(lower_needle.begin(), lower_needle.end(), lower_needle.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  return lower_value.find(lower_needle) != std::wstring::npos;
}

std::wstring FileStemDisplayName(const std::filesystem::path& path) {
  std::wstring name = path.filename().wstring();
  constexpr std::wstring_view suffix = L".dict.yaml";
  if (name.size() > suffix.size() &&
      name.substr(name.size() - suffix.size()) == suffix) {
    name.resize(name.size() - suffix.size());
    return name;
  }
  return path.stem().wstring();
}

std::optional<std::filesystem::path> PickRimeDictionaryFile(HWND owner) {
  std::wstring file_name(32768, L'\0');
  OPENFILENAMEW open_file{};
  open_file.lStructSize = sizeof(open_file);
  open_file.hwndOwner = owner;
  open_file.lpstrFilter =
      L"词库 (*.dict.yaml)\0*.dict.yaml\0YAML 文件 (*.yaml)\0*.yaml\0所有文件 (*.*)\0*.*\0";
  open_file.lpstrFile = file_name.data();
  open_file.nMaxFile = static_cast<DWORD>(file_name.size());
  open_file.lpstrTitle = L"选择要导入的词库";
  open_file.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&open_file)) {
    return std::nullopt;
  }
  file_name.resize(std::wcslen(file_name.c_str()));
  return std::filesystem::path(file_name);
}

std::optional<std::filesystem::path> PickSyncBackupFile(HWND owner) {
  std::wstring file_name(32768, L'\0');
  OPENFILENAMEW open_file{};
  open_file.lStructSize = sizeof(open_file);
  open_file.hwndOwner = owner;
  open_file.lpstrFilter = L"流畅拼音同步包 (*.fpsync)\0*.fpsync\0所有文件 (*.*)\0*.*\0";
  open_file.lpstrFile = file_name.data();
  open_file.nMaxFile = static_cast<DWORD>(file_name.size());
  open_file.lpstrTitle = L"选择要恢复的备份包";
  open_file.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&open_file)) {
    return std::nullopt;
  }
  file_name.resize(std::wcslen(file_name.c_str()));
  return std::filesystem::path(file_name);
}

std::optional<std::filesystem::path> PickSyncBackupSaveFile(HWND owner) {
  fp::EnsureDirectory(fp::sync::DefaultBackupDirectory());
  std::wstring file_name =
      (fp::sync::DefaultBackupDirectory() / fp::sync::NewTimestampedBackupName()).wstring();
  file_name.resize(32768, L'\0');
  OPENFILENAMEW save_file{};
  save_file.lStructSize = sizeof(save_file);
  save_file.hwndOwner = owner;
  save_file.lpstrFilter = L"流畅拼音同步包 (*.fpsync)\0*.fpsync\0所有文件 (*.*)\0*.*\0";
  save_file.lpstrFile = file_name.data();
  save_file.nMaxFile = static_cast<DWORD>(file_name.size());
  save_file.lpstrTitle = L"保存加密备份包";
  save_file.lpstrDefExt = L"fpsync";
  save_file.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
  if (!GetSaveFileNameW(&save_file)) {
    return std::nullopt;
  }
  file_name.resize(std::wcslen(file_name.c_str()));
  return std::filesystem::path(file_name);
}

bool WriteManagedDictionaryIntegrationFiles(
    const std::vector<ManagedDictionaryEntry>& entries) {
  return fp::config_winui::WriteManagedDictionaryIntegrationFiles(
      entries,
      ReadBoolSetting(L"user_lexicon_enabled", true),
      ReadBoolSetting(L"imported_lexicons_enabled", true));
}

bool ImportManagedDictionary(const std::filesystem::path& source_path,
                             std::wstring* error_message) {
  const auto yaml = ReadFileUtf8(source_path);
  if (!yaml) {
    if (error_message) {
      *error_message = L"无法读取所选文件。";
    }
    return false;
  }

  const auto dict_name = fp::config_winui::ParseRimeDictName(*yaml);
  if (!dict_name || !IsSafeRimeDictName(*dict_name)) {
    if (error_message) {
      *error_message = L"请选择带有合法 name 字段的 .dict.yaml 词库。";
    }
    return false;
  }

  const auto user_data_dir = RimeUserDataPath();
  fp::EnsureDirectory(user_data_dir);
  const auto target_path = user_data_dir / (fp::Utf8ToWide(*dict_name) + L".dict.yaml");
  if (!WriteFileUtf8(target_path, *yaml)) {
    if (error_message) {
      *error_message = L"写入用户词库目录失败。";
    }
    return false;
  }

  auto entries = ReadManagedDictionaryManifest();
  auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
    return entry.name == *dict_name;
  });
  if (found == entries.end()) {
    entries.push_back({*dict_name, true});
  } else {
    found->enabled = true;
  }

  if (!WriteManagedDictionaryIntegrationFiles(entries)) {
    if (error_message) {
      *error_message = L"更新词库清单失败。";
    }
    return false;
  }
  return true;
}

std::wstring LexiconCountText(int count) {
  return count > 99 ? L"99+" : std::to_wstring(std::max(0, count));
}

struct FuzzyPinyinRuleDefinition {
  std::wstring_view id;
  std::wstring_view name;
  std::wstring_view example;
};

struct WanxiangModeDefinition {
  std::wstring_view title;
  std::wstring_view subtitle;
  std::wstring_view setting_key;
  bool default_value;
  std::wstring_view icon;
  std::wstring_view legacy_key;
};

const std::array<FuzzyPinyinRuleDefinition, 10>& FuzzyPinyinRuleDefinitions() {
  static constexpr std::array<FuzzyPinyinRuleDefinition, 10> rules{{
      {L"nl", L"n / l", L"nan ? lan"},
      {L"ry", L"r / y", L"ran ? yan"},
      {L"hf", L"h / f", L"hu ? fu"},
      {L"rl", L"r / l", L"ru ? lu"},
      {L"kg", L"k / g", L"ka ? ga"},
      {L"en_eng", L"en / eng", L"shen ? sheng"},
      {L"in_ing", L"in / ing", L"xin ? xing"},
      {L"c_ch", L"c / ch", L"cao ? chao"},
      {L"z_zh", L"z / zh", L"zai ? zhai"},
      {L"s_sh", L"s / sh", L"san ? shan"},
  }};
  return rules;
}

const std::array<WanxiangModeDefinition, 14>& WanxiangModeDefinitions() {
  static constexpr std::array<WanxiangModeDefinition, 14> modes{{
      {L"反查拆字",
       L"使用 ` 引导部件拼音、笔画和辅助码反查。",
       L"wanxiang_reverse_lookup_enabled",
       true,
       L"`",
       L"u_mode"},
      {L"Unicode 码位",
       L"使用 U 加十六进制码位输入字符。",
       L"wanxiang_unicode_enabled",
       true,
       L"U",
       L"u_mode"},
      {L"数字金额",
       L"使用 R 引导中文数字和财务大写。",
       L"wanxiang_number_enabled",
       true,
       L"R",
       L"v_mode"},
      {L"日期时间",
       L"使用 N 或斜杠命令输入日期时间。",
       L"wanxiang_date_enabled",
       true,
       L"N",
       L"v_mode"},
      {L"超级计算器",
       L"使用 V 输入表达式、换算和计算。",
       L"wanxiang_calculator_enabled",
       true,
       L"V",
       L"v_mode"},
      {L"符号表",
       L"使用 / 加数字或字母输出符号表。",
       L"wanxiang_symbol_enabled",
       true,
       L"/",
       L""},
      {L"快捷符号",
       L"使用单字母命令快速上屏符号。",
       L"wanxiang_quick_symbol_enabled",
       false,
       L"a/",
       L""},
      {L"成对符号包裹",
       L"使用命令包裹候选文字和标记。",
       L"wanxiang_paired_symbol_enabled",
       true,
       L"\\",
       L""},
      {L"英文词库",
       L"启用英文候选、大小写格式化和中英混排辅助。",
       L"wanxiang_english_enabled",
       true,
       L"En",
       L""},
      {L"混合编码",
       L"启用中英数字混合候选。",
       L"wanxiang_mixed_code_enabled",
       true,
       L"Mix",
       L""},
      {L"输入统计",
       L"启用输入统计查询和记录。",
       L"wanxiang_input_statistics_enabled",
       false,
       L"统",
       L""},
      {L"自动造词",
       L"启用自动造词和英文造词过滤器。",
       L"wanxiang_auto_phrase_enabled",
       true,
       L"词",
       L""},
      {L"手动造词",
       L"启用手动创建用户词入口。",
       L"wanxiang_user_phrase_enabled",
       false,
       L"``",
       L""},
      {L"方案命令",
       L"启用输入方案切换命令。",
       L"wanxiang_schema_shortcuts_enabled",
       false,
       L"命",
       L""},
  }};
  return modes;
}

bool IsKnownFuzzyPinyinRule(std::wstring_view id) {
  const auto& rules = FuzzyPinyinRuleDefinitions();
  return std::any_of(rules.begin(), rules.end(), [id](const auto& rule) {
    return rule.id == id;
  });
}

std::vector<std::wstring> ParseFuzzyPinyinRuleSetting(std::wstring_view value,
                                                      bool default_to_all) {
  std::vector<std::wstring> result;
  std::wstring token;
  auto append_token = [&]() {
    if (token.empty()) {
      return;
    }
    if (token == L"all") {
      for (const auto& rule : FuzzyPinyinRuleDefinitions()) {
        const std::wstring id(rule.id);
        if (std::find(result.begin(), result.end(), id) == result.end()) {
          result.push_back(id);
        }
      }
    } else if (IsKnownFuzzyPinyinRule(token) &&
               std::find(result.begin(), result.end(), token) == result.end()) {
      result.push_back(token);
    }
    token.clear();
  };

  for (const wchar_t ch : value) {
    if (ch == L',' || ch == L';' || ch == L'|' || ch == L' ' || ch == L'\t') {
      append_token();
    } else {
      token.push_back(ch);
    }
  }
  append_token();

  if (result.empty() && default_to_all) {
    for (const auto& rule : FuzzyPinyinRuleDefinitions()) {
      result.emplace_back(rule.id);
    }
  }
  return result;
}

std::wstring JoinFuzzyPinyinRules(const std::vector<std::wstring>& rules) {
  std::wstring value;
  for (const auto& rule : rules) {
    if (!IsKnownFuzzyPinyinRule(rule)) {
      continue;
    }
    if (!value.empty()) {
      value += L',';
    }
    value += rule;
  }
  return value;
}

std::vector<std::wstring> CurrentFuzzyPinyinRules(bool default_to_all = false) {
  constexpr std::wstring_view marker = L"__fluent_default_fuzzy_rules__";
  const std::wstring raw = ReadStringSetting(L"fuzzy_pinyin_rules", marker);
  return ParseFuzzyPinyinRuleSetting(raw, default_to_all || raw == marker);
}

std::wstring CurrentFuzzyPinyinCustomRulesText() {
  return JoinFuzzyPinyinCustomRules(
      ParseFuzzyPinyinCustomRuleSetting(
          ReadStringSetting(L"fuzzy_pinyin_custom_rules", L"")),
      L'\n');
}

int CurrentFuzzyPinyinCustomRuleCount() {
  return static_cast<int>(ParseFuzzyPinyinCustomRuleSetting(
      ReadStringSetting(L"fuzzy_pinyin_custom_rules", L"")).size());
}

std::wstring FuzzyPinyinRuleIconText() {
  return ReadBoolSetting(L"fuzzy_pinyin", false) ? L"模" : L"规";
}

IReference<bool> NullableBool(bool value) {
  return box_value(value).as<IReference<bool>>();
}

bool IsChecked(CheckBox const& check) {
  const auto checked = check.IsChecked();
  return checked && checked.Value();
}

struct CustomFuzzyPinyinRuleRow {
  UIElement row{nullptr};
  TextBox left{nullptr};
  TextBox right{nullptr};
  bool removed = false;
};

struct LexiconPhraseRow {
  UIElement row{nullptr};
  TextBox phrase{nullptr};
  TextBox code{nullptr};
  TextBox weight{nullptr};
  bool removed = false;
};

struct ManagedDictionaryRow {
  UIElement row{nullptr};
  ToggleSwitch toggle{nullptr};
  std::string name;
  bool removed = false;
};

struct StatusTipBlacklistRow {
  UIElement row{nullptr};
  TextBox process{nullptr};
  bool removed = false;
};

void SetFuzzyPinyinRuleChecks(
    const std::vector<std::pair<std::wstring, CheckBox>>& checks,
    const std::vector<std::wstring>& enabled_rules) {
  for (const auto& [id, check] : checks) {
    check.IsChecked(NullableBool(FuzzyPinyinRuleSelected(enabled_rules, id)));
  }
}

bool WriteCandidateLayoutSetting(std::wstring_view value) {
  const bool horizontal = value != L"vertical";
  bool changed = WriteStringSetting(L"candidate_layout", horizontal ? L"horizontal" : L"vertical");
  changed = WriteBoolSetting(L"candidate_horizontal", horizontal) || changed;
  return changed;
}

void OpenPath(const std::filesystem::path& path) {
  fp::EnsureDirectory(path);
  ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenUrl(std::wstring_view url) {
  const std::wstring target(url);
  ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void RunTool(std::wstring_view exe_name, std::wstring_view parameters = L"") {
  const auto exe = SiblingExe(exe_name);
  const std::wstring params(parameters);
  ShellExecuteW(nullptr,
                L"open",
                exe.c_str(),
                params.empty() ? nullptr : params.c_str(),
                nullptr,
                SW_SHOWNORMAL);
}

void ShowWindowsAppRuntimeMissingMessage(HRESULT result) {
  const auto installer = ModuleDirectory() / std::wstring(kWindowsAppRuntimeInstallerName);
  std::wstring message =
      L"无法启动设置面板，因为系统缺少 Windows App Runtime 1.8。\n\n"
      L"请重新运行 FluentPinyin 安装包修复运行时依赖。";
  std::error_code error;
  if (std::filesystem::exists(installer, error)) {
    message += L"\n\n也可以运行安装目录中的 windowsappruntimeinstall-x64.exe 后再打开设置。";
  }
  wchar_t code[32]{};
  swprintf_s(code, L"\n\n错误代码：0x%08lX", static_cast<unsigned long>(result));
  message += code;
  MessageBoxW(nullptr, message.c_str(), L"流畅拼音 设置", MB_ICONERROR);
}

void RequestToolbarHostRefresh() {
  const UINT message = RegisterWindowMessageW(std::wstring(fp::kToolbarRefreshMessageName).c_str());
  if (message != 0) {
    PostMessageW(HWND_BROADCAST, message, 0, 0);
  }
}

void RequestToolbarHostShutdown() {
  const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kToolbarHostShutdownMessageName).c_str());
  if (message != 0) {
    PostMessageW(HWND_BROADCAST, message, 0, 0);
  }
}

void RequestApplyInputConfig() {
  const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kApplyInputConfigMessageName).c_str());
  if (message == 0) {
    return;
  }
  PostMessageW(HWND_BROADCAST, message, 0, 0);
}

void RequestInputStateRefresh() {
  const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kRefreshInputStateMessageName).c_str());
  if (message == 0) {
    return;
  }
  PostMessageW(HWND_BROADCAST, message, 0, 0);
}

void RequestCandidateWindowVisualRefresh() {
  const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kRefreshCandidateWindowVisualsMessageName).c_str());
  if (message != 0) {
    PostMessageW(HWND_BROADCAST, message, 0, 0);
  }
  RequestInputStateRefresh();
}

void RequestApplyInputConfigDeferred(DWORD delay_ms = 360) {
  static std::atomic<unsigned long> generation{0};
  const unsigned long request_generation = ++generation;
  std::thread([request_generation, delay_ms]() {
    Sleep(delay_ms);
    if (generation.load() != request_generation) {
      return;
    }
    RequestApplyInputConfig();
  }).detach();
}

void RequestInputStateRefreshDeferred(DWORD delay_ms = 260) {
  static std::atomic<unsigned long> generation{0};
  const unsigned long request_generation = ++generation;
  std::thread([request_generation, delay_ms]() {
    Sleep(delay_ms);
    if (generation.load() != request_generation) {
      return;
    }
    RequestInputStateRefresh();
  }).detach();
}

void RequestCandidateWindowVisualRefreshDeferred(DWORD delay_ms = 120) {
  static std::atomic<unsigned long> generation{0};
  const unsigned long request_generation = ++generation;
  std::thread([request_generation, delay_ms]() {
    Sleep(delay_ms);
    if (generation.load() != request_generation) {
      return;
    }
    RequestCandidateWindowVisualRefresh();
  }).detach();
}

bool HasCommandLineSwitch(std::wstring_view switch_name) {
  const std::wstring command = GetCommandLineW() != nullptr ? GetCommandLineW() : L"";
  return command.find(std::wstring(switch_name)) != std::wstring::npos;
}

int RunAutoSyncCommand() {
  const auto result = fp::sync::RunAutoSync();
  if (result.success) {
    RequestApplyInputConfig();
    return 0;
  }
  return 1;
}

bool ActivateExistingSettingsWindow(int attempts = 1) {
  return fp::config_winui::ActivateWindowByTitle(kSettingsAppTitle, attempts);
}

SizeInt32 SettingsMinimumWindowSize(HWND hwnd) {
  const UINT dpi = hwnd != nullptr ? GetDpiForWindow(hwnd) : GetDpiForSystem();
  return ScaleSizeForDpi(g_min_settings_window_size_dips, dpi);
}

LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  const UINT toolbar_refresh_message =
      RegisterWindowMessageW(std::wstring(fp::kToolbarRefreshMessageName).c_str());
  if (toolbar_refresh_message != 0 && message == toolbar_refresh_message) {
    ResetSettingsCache();
    const bool visible =
        ReadBoolSettingMigrated(kToolbarVisibleSetting, false, kLegacyToolbarVisibleSetting);
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
  if (message == WM_GETMINMAXINFO) {
    auto info = reinterpret_cast<MINMAXINFO*>(lparam);
    if (info != nullptr) {
      const SizeInt32 min_size = SettingsMinimumWindowSize(hwnd);
      info->ptMinTrackSize.x = min_size.Width;
      info->ptMinTrackSize.y = min_size.Height;
    }
  }
  if (message == WM_DPICHANGED) {
    const auto* suggested = reinterpret_cast<const RECT*>(lparam);
    if (suggested != nullptr) {
      SetWindowPos(hwnd,
                   nullptr,
                   suggested->left,
                   suggested->top,
                   suggested->right - suggested->left,
                   suggested->bottom - suggested->top,
                   SWP_NOACTIVATE | SWP_NOZORDER);
    }
    return 0;
  }
  if (g_settings_window_proc != nullptr) {
    return CallWindowProcW(g_settings_window_proc, hwnd, message, wparam, lparam);
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

void ApplyMinimumWindowSize(Window const& window, SizeInt32 const& min_size) {
  const HWND hwnd = GetWindowHandle(window);
  g_min_settings_window_size_dips = min_size;
  if (hwnd == nullptr || g_settings_window_proc != nullptr) {
    return;
  }
  g_settings_window_proc =
      reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SettingsWindowProc)));
}

void ApplyTitleBarColors(Window const& window) {
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

void ApplyWindowIcons(Window const& window) {
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

void ApplyDwmWindowFrame(Window const& window) {
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

void ApplySettingsResources(ResourceDictionary const& resources) {
  const auto palette = CurrentSettingsPalette();
  const Thickness hairline = SettingsHairlineThickness();
  const auto settings_font = SettingsUiFontFamily().as<IInspectable>();
  const auto border = SettingsBorderBrush().as<IInspectable>();
  const auto border_hover = SettingsBorderHoverBrush().as<IInspectable>();
  const auto surface = Brush(palette.background).as<IInspectable>();
  const auto card = Brush(palette.card).as<IInspectable>();
  const auto text = Brush(palette.text).as<IInspectable>();
  const auto secondary_text = Brush(palette.secondary_text).as<IInspectable>();
  const auto button = Brush(palette.button).as<IInspectable>();
  const auto button_hover = Brush(palette.button_hover).as<IInspectable>();
  const auto button_pressed = Brush(palette.button_pressed).as<IInspectable>();
  const auto accent = Brush(palette.accent).as<IInspectable>();
  const auto transparent = TransparentBrush().as<IInspectable>();
  const auto knob_on =
      Brush(palette.light ? ThemeColor{255, 255, 255} : ThemeColor{0, 0, 0}).as<IInspectable>();
  const auto knob_off = Brush(palette.secondary_text).as<IInspectable>();
  resources.Insert(box_value(L"ContentControlThemeFontFamily"), settings_font);
  resources.Insert(box_value(L"TextBlockFontFamily"), settings_font);
  resources.Insert(box_value(L"ButtonFontFamily"), settings_font);
  resources.Insert(box_value(L"CheckBoxFontFamily"), settings_font);
  resources.Insert(box_value(L"ComboBoxFontFamily"), settings_font);
  resources.Insert(box_value(L"ComboBoxItemFontFamily"), settings_font);
  resources.Insert(box_value(L"ContentDialogFontFamily"), settings_font);
  resources.Insert(box_value(L"ContentDialogButtonFontFamily"), settings_font);
  resources.Insert(box_value(L"MTCMediaFontFamily"), settings_font);
  resources.Insert(box_value(L"NavigationViewFontFamily"), settings_font);
  resources.Insert(box_value(L"NavigationViewItemFontFamily"), settings_font);
  resources.Insert(box_value(L"PasswordBoxFontFamily"), settings_font);
  resources.Insert(box_value(L"PhoneFontFamily"), settings_font);
  resources.Insert(box_value(L"PhoneFontFamilyNormal"), settings_font);
  resources.Insert(box_value(L"PhoneFontFamilySemiLight"), settings_font);
  resources.Insert(box_value(L"PivotHeaderItemFontFamily"), settings_font);
  resources.Insert(box_value(L"PivotTitleFontFamily"), settings_font);
  resources.Insert(box_value(L"TextBoxFontFamily"), settings_font);
  resources.Insert(box_value(L"TextControlFontFamily"), settings_font);
  resources.Insert(box_value(L"ToggleSwitchFontFamily"), settings_font);
  resources.Insert(box_value(L"ToolTipFontFamily"), settings_font);
  resources.Insert(box_value(L"ToolTipContentThemeFontFamily"), settings_font);
  resources.Insert(box_value(L"KeyTipFontFamily"), settings_font);
  resources.Insert(box_value(L"ApplicationPageBackgroundThemeBrush"), surface);
  resources.Insert(box_value(L"SolidBackgroundFillColorBaseBrush"), surface);
  resources.Insert(box_value(L"SolidBackgroundFillColorSecondaryBrush"), surface);
  resources.Insert(box_value(L"LayerFillColorDefaultBrush"), surface);
  resources.Insert(box_value(L"LayerFillColorAltBrush"), surface);
  resources.Insert(box_value(L"ControlStrokeColorDefaultBrush"), border);
  resources.Insert(box_value(L"SurfaceStrokeColorDefaultBrush"), border);
  resources.Insert(box_value(L"ButtonBorderThemeThickness"), box_value(hairline));
  resources.Insert(box_value(L"ComboBoxBorderThemeThickness"), box_value(hairline));
  resources.Insert(box_value(L"ComboBoxDropdownBorderThickness"), box_value(hairline));
  resources.Insert(box_value(L"ComboBoxPopupBorderThemeThickness"), box_value(hairline));
  resources.Insert(box_value(L"ContentDialogMaxWidth"),
                   box_value(kSettingsDialogOuterWidth));
  resources.Insert(box_value(L"ContentDialogMinWidth"),
                   box_value(kSettingsDialogOuterWidth));
  resources.Insert(box_value(L"ContentDialogBackground"), card);
  resources.Insert(box_value(L"ContentDialogForeground"), text);
  resources.Insert(box_value(L"ContentDialogBorderBrush"), border);
  resources.Insert(box_value(L"ContentDialogBorderWidth"), box_value(hairline));
  resources.Insert(box_value(L"ContentDialogButtonBackground"), button);
  resources.Insert(box_value(L"ContentDialogButtonBackgroundPointerOver"), button_hover);
  resources.Insert(box_value(L"ContentDialogButtonBackgroundPressed"), button_pressed);
  resources.Insert(box_value(L"ContentDialogButtonForeground"), text);
  resources.Insert(box_value(L"ContentDialogButtonBorderBrush"), border);
  resources.Insert(box_value(L"ContentDialogButtonBorderBrushPointerOver"), border_hover);
  resources.Insert(box_value(L"ContentDialogButtonBorderBrushPressed"), border_hover);
  resources.Insert(box_value(L"ContentDialogPrimaryButtonBackground"), Brush(palette.accent).as<IInspectable>());
  resources.Insert(box_value(L"ContentDialogPrimaryButtonBackgroundPointerOver"), accent);
  resources.Insert(box_value(L"ContentDialogPrimaryButtonBackgroundPressed"), accent);
  resources.Insert(box_value(L"ContentDialogPrimaryButtonForeground"),
                   Brush(palette.light ? ThemeColor{255, 255, 255} : ThemeColor{0, 0, 0}).as<IInspectable>());
  resources.Insert(box_value(L"ComboBoxBackground"), button);
  resources.Insert(box_value(L"ComboBoxBackgroundPointerOver"), button_hover);
  resources.Insert(box_value(L"ComboBoxBackgroundPressed"), button_pressed);
  resources.Insert(box_value(L"ComboBoxBackgroundDropDownOpen"), button_pressed);
  resources.Insert(box_value(L"ComboBoxForeground"), text);
  resources.Insert(box_value(L"ComboBoxForegroundPointerOver"), text);
  resources.Insert(box_value(L"ComboBoxForegroundPressed"), text);
  resources.Insert(box_value(L"ComboBoxForegroundDropDownOpen"), text);
  resources.Insert(box_value(L"ComboBoxBorderBrush"), border);
  resources.Insert(box_value(L"ComboBoxBorderBrushPointerOver"), border_hover);
  resources.Insert(box_value(L"ComboBoxBorderBrushPressed"), border_hover);
  resources.Insert(box_value(L"ComboBoxBorderBrushDropDownOpen"), border);
  resources.Insert(box_value(L"ComboBoxDropDownBackground"), card);
  resources.Insert(box_value(L"ComboBoxDropDownBorderBrush"), border);
  resources.Insert(box_value(L"ComboBoxItemBackground"), card);
  resources.Insert(box_value(L"ComboBoxItemBackgroundPointerOver"), button_hover);
  resources.Insert(box_value(L"ComboBoxItemBackgroundPressed"), button_pressed);
  resources.Insert(box_value(L"ComboBoxItemBackgroundSelected"), button_pressed);
  resources.Insert(box_value(L"ComboBoxItemBackgroundSelectedPointerOver"), button_pressed);
  resources.Insert(box_value(L"ComboBoxItemForeground"), text);
  resources.Insert(box_value(L"ComboBoxItemForegroundPointerOver"), text);
  resources.Insert(box_value(L"ComboBoxItemForegroundPressed"), text);
  resources.Insert(box_value(L"ComboBoxItemForegroundSelected"), text);
  resources.Insert(box_value(L"ComboBoxItemPillFillBrush"), transparent);
  resources.Insert(box_value(L"NavigationViewForeground"), text);
  resources.Insert(box_value(L"NavigationViewItemForeground"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundPointerOver"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundPressed"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundDisabled"), secondary_text);
  resources.Insert(box_value(L"NavigationViewItemForegroundChecked"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundCheckedPointerOver"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundCheckedPressed"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundSelected"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundSelectedPointerOver"), text);
  resources.Insert(box_value(L"NavigationViewItemForegroundSelectedPressed"), text);
  resources.Insert(box_value(L"NavigationViewItemIconForeground"), text);
  resources.Insert(box_value(L"NavigationViewItemIconForegroundPointerOver"), text);
  resources.Insert(box_value(L"NavigationViewItemIconForegroundPressed"), text);
  resources.Insert(box_value(L"NavigationViewItemIconForegroundSelected"), text);
  resources.Insert(box_value(L"NavigationViewItemIconForegroundSelectedPointerOver"), text);
  resources.Insert(box_value(L"NavigationViewItemBackground"), transparent);
  resources.Insert(box_value(L"NavigationViewItemBackgroundPointerOver"), button_hover);
  resources.Insert(box_value(L"NavigationViewItemBackgroundPressed"), button_pressed);
  resources.Insert(box_value(L"NavigationViewItemBackgroundChecked"), button);
  resources.Insert(box_value(L"NavigationViewItemBackgroundCheckedPointerOver"), button_hover);
  resources.Insert(box_value(L"NavigationViewItemBackgroundCheckedPressed"), button_pressed);
  resources.Insert(box_value(L"NavigationViewItemBackgroundSelected"), button);
  resources.Insert(box_value(L"NavigationViewItemBackgroundSelectedPointerOver"), button_hover);
  resources.Insert(box_value(L"NavigationViewItemBackgroundSelectedPressed"), button_pressed);
  resources.Insert(box_value(L"NavigationViewItemBorderBrush"), transparent);
  resources.Insert(box_value(L"NavigationViewItemBorderBrushPointerOver"), transparent);
  resources.Insert(box_value(L"NavigationViewItemBorderBrushPressed"), transparent);
  resources.Insert(box_value(L"NavigationViewItemBorderBrushChecked"), transparent);
  resources.Insert(box_value(L"NavigationViewItemBorderBrushSelected"), transparent);
  resources.Insert(box_value(L"NavigationViewSelectionIndicatorForeground"), accent);
  resources.Insert(box_value(L"ToggleSwitchContentForeground"), text);
  resources.Insert(box_value(L"ToggleSwitchContentForegroundDisabled"), secondary_text);
  resources.Insert(box_value(L"ToggleSwitchHeaderForeground"), text);
  resources.Insert(box_value(L"ToggleSwitchHeaderForegroundDisabled"), secondary_text);
  resources.Insert(box_value(L"ToggleSwitchContainerBackground"), transparent);
  resources.Insert(box_value(L"ToggleSwitchContainerBackgroundPointerOver"),
                   ColorReference(0, 0, 0, 0));
  resources.Insert(box_value(L"ToggleSwitchContainerBackgroundPressed"),
                   ColorReference(0, 0, 0, 0));
  resources.Insert(box_value(L"ToggleSwitchContainerBackgroundDisabled"),
                   ColorReference(0, 0, 0, 0));
  resources.Insert(box_value(L"ToggleSwitchFillOff"), button);
  resources.Insert(box_value(L"ToggleSwitchFillOffPointerOver"),
                   ColorReference(palette.button_hover));
  resources.Insert(box_value(L"ToggleSwitchFillOffPressed"),
                   ColorReference(palette.button_pressed));
  resources.Insert(box_value(L"ToggleSwitchFillOffDisabled"),
                   ColorReference(palette.button));
  resources.Insert(box_value(L"ToggleSwitchStrokeOff"), border);
  resources.Insert(box_value(L"ToggleSwitchStrokeOffPointerOver"),
                   ColorReference(SettingsBorderHoverColor()));
  resources.Insert(box_value(L"ToggleSwitchStrokeOffPressed"),
                   ColorReference(SettingsBorderHoverColor()));
  resources.Insert(box_value(L"ToggleSwitchStrokeOffDisabled"),
                   ColorReference(SettingsBorderColor()));
  resources.Insert(box_value(L"ToggleSwitchOuterBorderStrokeThickness"),
                   box_value(SettingsHairlineDip()));
  resources.Insert(box_value(L"ToggleSwitchFillOn"), accent);
  resources.Insert(box_value(L"ToggleSwitchFillOnPointerOver"), accent);
  resources.Insert(box_value(L"ToggleSwitchFillOnPressed"), accent);
  resources.Insert(box_value(L"ToggleSwitchFillOnDisabled"), button_pressed);
  resources.Insert(box_value(L"ToggleSwitchStrokeOn"), accent);
  resources.Insert(box_value(L"ToggleSwitchStrokeOnPointerOver"), accent);
  resources.Insert(box_value(L"ToggleSwitchStrokeOnPressed"), accent);
  resources.Insert(box_value(L"ToggleSwitchStrokeOnDisabled"), button_pressed);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOff"), knob_off);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOffPointerOver"), knob_off);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOffPressed"), knob_off);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOffDisabled"), secondary_text);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOn"), knob_on);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOnPointerOver"), knob_on);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOnPressed"), knob_on);
  resources.Insert(box_value(L"ToggleSwitchKnobFillOnDisabled"), secondary_text);
  resources.Insert(box_value(L"TextControlForeground"), text);
  resources.Insert(box_value(L"TextControlForegroundPointerOver"), text);
  resources.Insert(box_value(L"TextControlForegroundFocused"), text);
  resources.Insert(box_value(L"TextControlPlaceholderForeground"), secondary_text);
  resources.Insert(box_value(L"TextControlPlaceholderForegroundPointerOver"), secondary_text);
  resources.Insert(box_value(L"TextControlPlaceholderForegroundFocused"), secondary_text);
  resources.Insert(box_value(L"TextControlHeaderForeground"), text);
  resources.Insert(box_value(L"TextControlBackground"), button);
  resources.Insert(box_value(L"TextControlBackgroundPointerOver"), button_hover);
  resources.Insert(box_value(L"TextControlBackgroundFocused"), button_pressed);
  resources.Insert(box_value(L"TextControlBorderBrush"), border);
  resources.Insert(box_value(L"TextControlBorderBrushPointerOver"), border_hover);
  resources.Insert(box_value(L"TextControlBorderBrushFocused"), border_hover);
  resources.Insert(box_value(L"TextControlBorderBrushDisabled"), border);
  resources.Insert(box_value(L"TextControlBorderThemeThickness"), box_value(hairline));
  resources.Insert(box_value(L"TextControlBorderThemeThicknessFocused"), box_value(hairline));
  resources.Insert(box_value(L"TextControlButtonForeground"), secondary_text);
  resources.Insert(box_value(L"TextControlSelectionHighlightColor"), accent);
  resources.Insert(box_value(L"NavigationViewDefaultPaneBackground"), surface);
  resources.Insert(box_value(L"NavigationViewContentBackground"), surface);
}

TextBlock Text(std::wstring_view value, double size, int weight = FW_NORMAL) {
  TextBlock text;
  text.Text(value);
  ApplySettingsUIFont(text);
  text.FontSize(size);
  text.Foreground(SettingsTextBrush());
  text.TextWrapping(TextWrapping::Wrap);
  if (weight >= FW_SEMIBOLD) {
    text.FontWeight(FontWeights::SemiBold());
  }
  return text;
}

ToolTip SettingsToolTip(std::wstring_view value) {
  ToolTip tooltip;
  ApplySettingsUIFont(tooltip);
  tooltip.RequestedTheme(CurrentSettingsElementTheme());
  tooltip.Content(Text(value, 12));
  ApplySettingsResources(tooltip.Resources());
  return tooltip;
}

void SetSettingsToolTip(DependencyObject const& target, std::wstring_view value) {
  ToolTipService::SetToolTip(target, SettingsToolTip(value));
}

void ApplySettingsDialogBase(ContentDialog const& dialog, XamlRoot const& xaml_root) {
  dialog.XamlRoot(xaml_root);
  dialog.RequestedTheme(CurrentSettingsElementTheme());
  dialog.Title(nullptr);
  dialog.FontFamily(SettingsUiFontFamily());
  ApplySettingsResources(dialog.Resources());
}

void PrepareIconElement(FrameworkElement const& element,
                        double host_size = kSettingIconHostSize) {
  element.Width(host_size);
  element.Height(host_size);
  element.MinWidth(host_size);
  element.MinHeight(host_size);
  element.HorizontalAlignment(HorizontalAlignment::Center);
  element.VerticalAlignment(VerticalAlignment::Center);
  element.UseLayoutRounding(true);
}

FontIcon Icon(std::wstring_view glyph, double size = kSettingIconVisualSize) {
  FontIcon icon;
  icon.Glyph(glyph);
  icon.FontFamily(FontFamily(L"Segoe Fluent Icons"));
  icon.FontSize(size);
  icon.Foreground(SettingsIconBrush());
  icon.FontWeight(FontWeights::Normal());
  PrepareIconElement(icon, kSettingIconHostSize);
  return icon;
}

TextBlock TextIcon(std::wstring_view value, double size = 15.0) {
  auto icon = Text(value, size, FW_SEMIBOLD);
  icon.Foreground(SettingsIconBrush());
  PrepareIconElement(icon, kSettingIconHostSize);
  icon.TextAlignment(TextAlignment::Center);
  icon.TextWrapping(TextWrapping::NoWrap);
  icon.LineHeight(kSettingIconHostSize);
  icon.LineStackingStrategy(LineStackingStrategy::BlockLineHeight);
  return icon;
}

Grid IconHost(UIElement const& icon_content) {
  Grid host;
  PrepareIconElement(host, kSettingIconHostSize);
  host.Children().Append(icon_content);
  return host;
}

Shapes::Path PathShape(std::wstring_view data,
                       double size,
                       double view_box_size,
                       double scale = 1.0,
                       double dx = 0.0,
                       double dy = 0.0,
                       double scale_y = 0.0) {
  auto xaml = L"<Path xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
              L"Data=\"" +
              std::wstring(data) +
              L"\" Fill=\"#FFF2F2F2\" Stretch=\"None\" Width=\"" +
              std::to_wstring(view_box_size) + L"\" Height=\"" +
              std::to_wstring(view_box_size) + L"\"/>";
  auto path = Markup::XamlReader::Load(xaml).as<Shapes::Path>();
  path.Fill(SettingsIconBrush());
  path.HorizontalAlignment(HorizontalAlignment::Center);
  path.VerticalAlignment(VerticalAlignment::Center);
  path.UseLayoutRounding(true);
  path.RenderTransformOrigin(Windows::Foundation::Point{0.5f, 0.5f});
  Media::CompositeTransform transform;
  const double normalized_scale = scale * size / view_box_size;
  transform.ScaleX(normalized_scale);
  transform.ScaleY((scale_y > 0.0 ? scale_y : scale) * size / view_box_size);
  transform.TranslateX(size * dx);
  transform.TranslateY(size * dy);
  path.RenderTransform(transform);
  return path;
}

Shapes::Path RawPathShape(std::wstring_view data) {
  auto xaml = L"<Path xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
              L"Data=\"" +
              std::wstring(data) + L"\" Fill=\"#FFF2F2F2\" Stretch=\"None\"/>";
  auto path = Markup::XamlReader::Load(xaml).as<Shapes::Path>();
  path.Fill(SettingsIconBrush());
  path.UseLayoutRounding(true);
  return path;
}

Canvas ScaledIconCanvas(double size, double view_box_size, double scale = 1.0) {
  Canvas canvas;
  canvas.Width(view_box_size);
  canvas.Height(view_box_size);
  canvas.HorizontalAlignment(HorizontalAlignment::Center);
  canvas.VerticalAlignment(VerticalAlignment::Center);
  canvas.UseLayoutRounding(true);
  canvas.RenderTransformOrigin(Windows::Foundation::Point{0.5f, 0.5f});
  Media::CompositeTransform transform;
  const double normalized_scale = scale * size / view_box_size;
  transform.ScaleX(normalized_scale);
  transform.ScaleY(normalized_scale);
  canvas.RenderTransform(transform);
  return canvas;
}

void SetIconChild(Grid const& root, UIElement const& child) {
  root.Children().Clear();
  root.Children().Append(child);
}

Shapes::Ellipse EllipseShape(double left,
                             double top,
                             double width,
                             double height,
                             SolidColorBrush const& fill) {
  Shapes::Ellipse ellipse;
  ellipse.Width(width);
  ellipse.Height(height);
  ellipse.Fill(fill);
  ellipse.UseLayoutRounding(true);
  Canvas::SetLeft(ellipse, left);
  Canvas::SetTop(ellipse, top);
  return ellipse;
}

constexpr std::wstring_view kFluentCircle20FilledPath =
    L"M10 2C14.4183 2 18 5.58172 18 10C18 14.4183 14.4183 18 10 18C5.58172 18 2 14.4183 2 10C2 5.58172 5.58172 2 10 2Z";
constexpr std::wstring_view kFluentCircle20RegularPath =
    L"M10 3C6.13401 3 3 6.13401 3 10C3 13.866 6.13401 17 10 17C13.866 17 17 13.866 17 10C17 6.13401 13.866 3 10 3ZM2 10C2 5.58172 5.58172 2 10 2C14.4183 2 18 5.58172 18 10C18 14.4183 14.4183 18 10 18C5.58172 18 2 14.4183 2 10Z";
constexpr std::wstring_view kFluentWeatherMoon24Path =
    L"M20.0258 17.0014C17.2639 21.7851 11.1471 23.4241 6.3634 20.6622C5.06068 19.9101 3.964 18.8926 3.12872 17.6797C2.84945 17.2741 3.0301 16.7141 3.49369 16.5482C7.26112 15.1997 9.27892 13.6372 10.4498 11.4021C11.6825 9.04908 12.001 6.47162 11.1387 2.93862C11.0195 2.45008 11.4053 1.98492 11.9075 2.01186C13.4645 2.09539 14.9856 2.54263 16.3649 3.33903C21.1486 6.10088 22.7876 12.2177 20.0258 17.0014ZM11.7785 12.0981C10.5272 14.4867 8.46706 16.1972 4.96104 17.597C5.5693 18.2929 6.29275 18.8894 7.1134 19.3632C11.1796 21.7108 16.3791 20.3176 18.7267 16.2514C21.0744 12.1852 19.6812 6.98571 15.6149 4.63807C14.7379 4.1317 13.7951 3.79168 12.8228 3.62253C13.4699 7.00652 13.0525 9.66622 11.7785 12.0981Z";
constexpr std::wstring_view kFluentDarkTheme20RegularPath =
    L"M10 3C13.866 3 17 6.13401 17 10C17 13.866 13.866 17 10 17V3ZM10 2C5.58172 2 2 5.58172 2 10C2 14.4183 5.58172 18 10 18C14.4183 18 18 14.4183 18 10C18 5.58172 14.4183 2 10 2Z";
constexpr std::wstring_view kFluentWeatherMoon20FilledPath =
    L"M16.3592 13.9967C14.1552 17.8141 9.27399 19.122 5.45663 16.918C4.41706 16.3178 3.54192 15.5059 2.87537 14.538C2.65251 14.2143 2.79667 13.7674 3.16661 13.635C6.17301 12.559 7.78322 11.312 8.71759 9.52844C9.70125 7.65076 9.95545 5.59395 9.26732 2.77462C9.17217 2.38477 9.4801 2.01357 9.88082 2.03507C11.1233 2.10173 12.3371 2.45863 13.4378 3.09415C17.2552 5.2981 18.5631 10.1793 16.3592 13.9967Z";
constexpr std::wstring_view kFluentWeatherSunny20RegularPath =
    L"M10 2C10.2761 2 10.5 2.22386 10.5 2.5V3.5C10.5 3.77614 10.2761 4 10 4C9.72386 4 9.5 3.77614 9.5 3.5V2.5C9.5 2.22386 9.72386 2 10 2ZM10 14C12.2091 14 14 12.2091 14 10C14 7.79086 12.2091 6 10 6C7.79086 6 6 7.79086 6 10C6 12.2091 7.79086 14 10 14ZM10 13C8.34315 13 7 11.6569 7 10C7 8.34315 8.34315 7 10 7C11.6569 7 13 8.34315 13 10C13 11.6569 11.6569 13 10 13ZM17.5 10.5C17.7761 10.5 18 10.2761 18 10C18 9.72386 17.7761 9.5 17.5 9.5H16.5C16.2239 9.5 16 9.72386 16 10C16 10.2761 16.2239 10.5 16.5 10.5H17.5ZM10 16C10.2761 16 10.5 16.2239 10.5 16.5V17.5C10.5 17.7761 10.2761 18 10 18C9.72386 18 9.5 17.7761 9.5 17.5V16.5C9.5 16.2239 9.72386 16 10 16ZM3.5 10.5C3.77614 10.5 4 10.2761 4 10C4 9.72386 3.77614 9.5 3.5 9.5H2.46289C2.18675 9.5 1.96289 9.72386 1.96289 10C1.96289 10.2761 2.18675 10.5 2.46289 10.5H3.5ZM4.14645 4.14645C4.34171 3.95118 4.65829 3.95118 4.85355 4.14645L5.85355 5.14645C6.04882 5.34171 6.04882 5.65829 5.85355 5.85355C5.65829 6.04882 5.34171 6.04882 5.14645 5.85355L4.14645 4.85355C3.95118 4.65829 3.95118 4.34171 4.14645 4.14645ZM4.85355 15.8536C4.65829 16.0488 4.34171 16.0488 4.14645 15.8536C3.95118 15.6583 3.95118 15.3417 4.14645 15.1464L5.14645 14.1464C5.34171 13.9512 5.65829 13.9512 5.85355 14.1464C6.04882 14.3417 6.04882 14.6583 5.85355 14.8536L4.85355 15.8536ZM15.8536 4.14645C15.6583 3.95118 15.3417 3.95118 15.1464 4.14645L14.1464 5.14645C13.9512 5.34171 13.9512 5.65829 14.1464 5.85355C14.3417 6.04882 14.6583 6.04882 14.8536 5.85355L15.8536 4.85355C16.0488 4.65829 16.0488 4.34171 15.8536 4.14645ZM15.1464 15.8536C15.3417 16.0488 15.6583 16.0488 15.8536 15.8536C16.0488 15.6583 16.0488 15.3417 15.8536 15.1464L14.8536 14.1464C14.6583 13.9512 14.3417 13.9512 14.1464 14.1464C13.9512 14.3417 13.9512 14.6583 14.1464 14.8536L15.1464 15.8536Z";
constexpr std::wstring_view kFluentEmoji24Path =
    L"M12 1.99805C17.5237 1.99805 22.0015 6.47589 22.0015 11.9996C22.0015 17.5233 17.5237 22.0011 12 22.0011C6.47626 22.0011 1.99841 17.5233 1.99841 11.9996C1.99841 6.47589 6.47626 1.99805 12 1.99805ZM12 3.49805C7.30469 3.49805 3.49841 7.30432 3.49841 11.9996C3.49841 16.6949 7.30469 20.5011 12 20.5011C16.6952 20.5011 20.5015 16.6949 20.5015 11.9996C20.5015 7.30432 16.6952 3.49805 12 3.49805ZM8.4617 14.7829C9.31084 15.8606 10.6019 16.5012 11.9999 16.5012C13.3962 16.5012 14.6856 15.8624 15.5349 14.7871C15.7916 14.462 16.2633 14.4066 16.5883 14.6634C16.9134 14.9201 16.9688 15.3917 16.712 15.7168C15.5813 17.1485 13.8601 18.0012 11.9999 18.0012C10.1373 18.0012 8.41408 17.1462 7.28348 15.7112C7.02713 15.3859 7.08307 14.9143 7.40843 14.658C7.73379 14.4016 8.20535 14.4576 8.4617 14.7829ZM9.00041 8.75024C9.69037 8.75024 10.2497 9.30956 10.2497 9.99953C10.2497 10.6895 9.69037 11.2488 9.00041 11.2488C8.31045 11.2488 7.75112 10.6895 7.75112 9.99953C7.75112 9.30956 8.31045 8.75024 9.00041 8.75024ZM15.0004 8.75024C15.6904 8.75024 16.2497 9.30956 16.2497 9.99953C16.2497 10.6895 15.6904 11.2488 15.0004 11.2488C14.3104 11.2488 13.7511 10.6895 13.7511 9.99953C13.7511 9.30956 14.3104 8.75024 15.0004 8.75024Z";
constexpr std::wstring_view kFluentChevronLeft20Path =
    L"M12.3534 15.8537C12.1585 16.0493 11.8419 16.0499 11.6463 15.855L6.16178 10.39C5.94607 10.1751 5.94607 9.82574 6.16178 9.6108L11.6463 4.14582C11.8419 3.9509 12.1585 3.95147 12.3534 4.14708C12.5483 4.34269 12.5477 4.65927 12.3521 4.85418L7.18753 10.0004L12.3521 15.1466C12.5477 15.3415 12.5483 15.6581 12.3534 15.8537Z";
constexpr std::wstring_view kFluentChevronRight20Path =
    L"M7.64582 4.14708C7.84073 3.95147 8.15731 3.9509 8.35292 4.14582L13.8374 9.6108C14.0531 9.82574 14.0531 10.1751 13.8374 10.39L8.35292 15.855C8.15731 16.0499 7.84073 16.0493 7.64582 15.8537C7.4509 15.6581 7.45147 15.3415 7.64708 15.1466L12.8117 10.0004L7.64708 4.85418C7.45147 4.65927 7.4509 4.34269 7.64582 4.14708Z";
constexpr std::wstring_view kFluentChevronDown20Path =
    L"M15.8537 7.64582C16.0493 7.84073 16.0499 8.15731 15.855 8.35292L10.39 13.8374C10.1751 14.0531 9.82574 14.0531 9.6108 13.8374L4.14582 8.35292C3.9509 8.15731 3.95147 7.84073 4.14708 7.64582C4.34269 7.4509 4.65927 7.45147 4.85418 7.64708L10.0004 12.8117L15.1466 7.64708C15.3415 7.45147 15.6581 7.4509 15.8537 7.64582Z";
constexpr std::wstring_view kFluentChevronUp20Path =
    L"M4.14708 12.3534C3.95147 12.1585 3.9509 11.8419 4.14582 11.6463L9.6108 6.16178C9.82574 5.94607 10.1751 5.94607 10.39 6.16178L15.855 11.6463C16.0499 11.8419 16.0493 12.1585 15.8537 12.3534C15.6581 12.5483 15.3415 12.5477 15.1466 12.3521L10.0004 7.18753L4.85418 12.3521C4.65927 12.5477 4.34269 12.5483 4.14708 12.3534Z";
constexpr std::wstring_view kFluentTriangleLeft12FilledPath =
    L"M1.45866 5.21367C0.847113 5.56267 0.847113 6.43734 1.45866 6.78633L8.62781 10.8776C9.23809 11.2259 10 10.7893 10 10.0913V10.0635L10 10.0586V1.94235L10 1.93748V1.9087C10 1.2107 9.23809 0.774094 8.62781 1.12237L1.45866 5.21367Z";
constexpr std::wstring_view kFluentTriangleRight12FilledPath =
    L"M10.5414 6.78633C11.1529 6.43734 11.1529 5.56267 10.5414 5.21367L3.3722 1.12237C2.76192 0.774094 2.00001 1.2107 2.00001 1.9087L2.00001 1.9365L2 1.94138L2 10.0576L2.00001 10.0625L2.00001 10.0913C2.00001 10.7893 2.76192 11.2259 3.3722 10.8776L10.5414 6.78633Z";
constexpr std::wstring_view kFluentReOrderDotsHorizontal20RegularPath =
    L"M15 7C15 7.55229 15.4477 8 16 8C16.5523 8 17 7.55229 17 7C17 6.44772 16.5523 6 16 6C15.4477 6 15 6.44772 15 7ZM9 7C9 7.55228 9.44772 8 10 8C10.5523 8 11 7.55228 11 7C11 6.44772 10.5523 6 10 6C9.44772 6 9 6.44772 9 7ZM4 8C3.44772 8 3 7.55228 3 7C3 6.44772 3.44772 6 4 6C4.55229 6 5 6.44772 5 7C5 7.55228 4.55229 8 4 8ZM15 13C15 13.5523 15.4477 14 16 14C16.5523 14 17 13.5523 17 13C17 12.4477 16.5523 12 16 12C15.4477 12 15 12.4477 15 13ZM10 14C9.44772 14 9 13.5523 9 13C9 12.4477 9.44772 12 10 12C10.5523 12 11 12.4477 11 13C11 13.5523 10.5523 14 10 14ZM3 13C3 13.5523 3.44772 14 4 14C4.55229 14 5 13.5523 5 13C5 12.4477 4.55229 12 4 12C3.44772 12 3 12.4477 3 13Z";
constexpr std::wstring_view kFluentReOrderDotsVertical20RegularPath =
    L"M7 5C7.55228 5 8 4.55228 8 4C8 3.44772 7.55228 3 7 3C6.44772 3 6 3.44772 6 4C6 4.55228 6.44772 5 7 5ZM7 11C7.55228 11 8 10.5523 8 10C8 9.44771 7.55228 9 7 9C6.44772 9 6 9.44771 6 10C6 10.5523 6.44772 11 7 11ZM8 16C8 16.5523 7.55228 17 7 17C6.44772 17 6 16.5523 6 16C6 15.4477 6.44772 15 7 15C7.55228 15 8 15.4477 8 16ZM13 5C13.5523 5 14 4.55228 14 4C14 3.44772 13.5523 3 13 3C12.4477 3 12 3.44772 12 4C12 4.55228 12.4477 5 13 5ZM14 10C14 10.5523 13.5523 11 13 11C12.4477 11 12 10.5523 12 10C12 9.44771 12.4477 9 13 9C13.5523 9 14 9.44771 14 10ZM13 17C13.5523 17 14 16.5523 14 16C14 15.4477 13.5523 15 13 15C12.4477 15 12 15.4477 12 16C12 16.5523 12.4477 17 13 17Z";
constexpr std::wstring_view kFluentIconRename20RegularPath =
    L"M8.5 2C8.22386 2 8 2.22386 8 2.5C8 2.77614 8.22386 3 8.5 3H9.5V17H8.5C8.22386 17 8 17.2239 8 17.5C8 17.7761 8.22386 18 8.5 18H11.5C11.7761 18 12 17.7761 12 17.5C12 17.2239 11.7761 17 11.5 17H10.5V3H11.5C11.7761 3 12 2.77614 12 2.5C12 2.22386 11.7761 2 11.5 2H8.5ZM4.5 4H8.5V5H4.5C3.67157 5 3 5.67157 3 6.5V13.5C3 14.3284 3.67157 15 4.5 15H8.5V16H4.5C3.11929 16 2 14.8807 2 13.5V6.5C2 5.11929 3.11929 4 4.5 4ZM15.5 15H11.5V16H15.5C16.8807 16 18 14.8807 18 13.5V6.5C18 5.11929 16.8807 4 15.5 4H11.5V5H15.5C16.3284 5 17 5.67157 17 6.5V13.5C17 14.3284 16.3284 15 15.5 15Z";
constexpr std::wstring_view kFluentIconRectangleLandscape20RegularPath =
    L"M2 7C2 5.34315 3.34315 4 5 4H15C16.6569 4 18 5.34315 18 7V13C18 14.6569 16.6569 16 15 16H5C3.34315 16 2 14.6569 2 13V7ZM5 5C3.89543 5 3 5.89543 3 7V13C3 14.1046 3.89543 15 5 15H15C16.1046 15 17 14.1046 17 13V7C17 5.89543 16.1046 5 15 5H5Z";
constexpr std::wstring_view kFluentIconEdit20RegularPath =
    L"M17.1813 2.92689C16.029 1.71505 14.1046 1.69077 12.9222 2.87317L3.54735 12.2475C3.21952 12.5754 2.99198 12.9899 2.89142 13.4424L2.0138 17.3923C1.97672 17.5592 2.02748 17.7335 2.14838 17.8544C2.26928 17.9753 2.44356 18.026 2.61044 17.9889L6.53683 17.1157C7.00426 17.0118 7.43237 16.7767 7.77096 16.4381L17.129 7.08003C18.27 5.939 18.2932 4.09631 17.1813 2.92689ZM13.6293 3.58029C14.4142 2.79538 15.6917 2.8115 16.4566 3.61596C17.1947 4.39225 17.1793 5.61548 16.4218 6.37293L15.7506 7.04418L12.958 4.25155L13.6293 3.58029ZM12.2508 4.95864L15.0435 7.7513L7.06385 15.731C6.8597 15.9352 6.60158 16.0769 6.31975 16.1396L3.16044 16.8421L3.86762 13.6593C3.92692 13.3924 4.0611 13.148 4.25444 12.9547L12.2508 4.95864Z";
constexpr std::wstring_view kFluentIconEditSettings20RegularPath =
    L"M17.1794 2.92689C16.0271 1.71505 14.1027 1.69077 12.9203 2.87317L3.54545 12.2475C3.21763 12.5754 2.99008 12.9899 2.88953 13.4424L2.01191 17.3923C1.97483 17.5592 2.02559 17.7335 2.14649 17.8544C2.26739 17.9753 2.44166 18.026 2.60855 17.9889L6.53494 17.1157C7.00237 17.0118 7.43048 16.7767 7.76907 16.4381L8.20707 16.0001C8.09703 15.6111 8.02873 15.2046 8.00729 14.7857L7.06196 15.731C6.85781 15.9352 6.59969 16.0769 6.31786 16.1396L3.15855 16.8421L3.86572 13.6593C3.92502 13.3924 4.05921 13.148 4.25254 12.9547L12.2489 4.95864L15.0416 7.7513L13.7856 9.00729C14.2045 9.02872 14.6111 9.09702 15.0001 9.20705L17.1271 7.08003C18.2681 5.939 18.2913 4.09631 17.1794 2.92689ZM13.6274 3.58029C14.4123 2.79538 15.6898 2.8115 16.4547 3.61596C17.1928 4.39225 17.1774 5.61548 16.42 6.37293L15.7487 7.04418L12.9561 4.25155L13.6274 3.58029ZM11.0667 11.4429C11.37 12.5241 10.724 13.643 9.63604 13.9209L9.175 14.0387C9.16002 14.1906 9.15234 14.3448 9.15234 14.5008C9.15234 14.6885 9.16344 14.8735 9.185 15.0551L9.53456 15.1377C10.654 15.4024 11.32 16.5545 10.9906 17.6567L10.8643 18.0795C11.1215 18.2827 11.4012 18.4569 11.699 18.5974L12.0239 18.2533C12.8138 17.417 14.1445 17.4177 14.9335 18.2548L15.2708 18.6128C15.5632 18.4778 15.8386 18.3105 16.0927 18.1151L15.9365 17.5585C15.6332 16.4773 16.2792 15.3584 17.3672 15.0805L17.8277 14.9629C17.8427 14.811 17.8504 14.6568 17.8504 14.5008C17.8504 14.313 17.8393 14.128 17.8177 13.9462L17.4687 13.8637C16.3492 13.599 15.6832 12.4469 16.0126 11.3447L16.1388 10.9225C15.8815 10.7192 15.6018 10.5449 15.304 10.4044L14.9793 10.7482C14.1895 11.5845 12.8587 11.5837 12.0698 10.7466L11.7324 10.3887C11.44 10.5236 11.1646 10.6909 10.9105 10.8862L11.0667 11.4429ZM13.5014 15.5008C12.9491 15.5008 12.5014 15.0531 12.5014 14.5008C12.5014 13.9485 12.9491 13.5008 13.5014 13.5008C14.0536 13.5008 14.5014 13.9485 14.5014 14.5008C14.5014 15.0531 14.0536 15.5008 13.5014 15.5008Z";
constexpr std::wstring_view kFluentIconEditOff20RegularPath =
    L"M2.85355 2.14645C2.65829 1.95118 2.34171 1.95118 2.14645 2.14645C1.95118 2.34171 1.95118 2.65829 2.14645 2.85355L7.54304 8.25015L3.54545 12.2475C3.21763 12.5754 2.99008 12.9899 2.88953 13.4424L2.01191 17.3923C1.97483 17.5592 2.02559 17.7335 2.14649 17.8544C2.26739 17.9753 2.44166 18.026 2.60855 17.9889L6.53494 17.1157C7.00237 17.0118 7.43048 16.7767 7.76907 16.4381L11.75 12.4571L17.1464 17.8536C17.3417 18.0488 17.6583 18.0488 17.8536 17.8536C18.0488 17.6583 18.0488 17.3417 17.8536 17.1464L2.85355 2.14645ZM11.0429 11.75L7.06196 15.731C6.85781 15.9352 6.59969 16.0769 6.31786 16.1396L3.15855 16.8421L3.86572 13.6593C3.92502 13.3924 4.05921 13.148 4.25254 12.9547L8.25015 8.95725L11.0429 11.75ZM15.0416 7.7513L12.457 10.3359L13.1641 11.043L17.1271 7.08003C18.2681 5.939 18.2913 4.09631 17.1794 2.92689C16.0271 1.71505 14.1027 1.69077 12.9203 2.87317L8.95717 6.83608L9.66428 7.54319L12.2489 4.95864L15.0416 7.7513ZM13.6274 3.58029C14.4123 2.79538 15.6898 2.8115 16.4547 3.61596C17.1928 4.39225 17.1774 5.61548 16.42 6.37293L15.7487 7.04418L12.9561 4.25155L13.6274 3.58029Z";
constexpr std::wstring_view kFluentIconDelete20RegularPath =
    L"M8.5 4H11.5C11.5 3.17157 10.8284 2.5 10 2.5C9.17157 2.5 8.5 3.17157 8.5 4ZM7.5 4C7.5 2.61929 8.61929 1.5 10 1.5C11.3807 1.5 12.5 2.61929 12.5 4H17.5C17.7761 4 18 4.22386 18 4.5C18 4.77614 17.7761 5 17.5 5H16.4456L15.2521 15.3439C15.0774 16.8576 13.7957 18 12.2719 18H7.72813C6.20431 18 4.92256 16.8576 4.7479 15.3439L3.55437 5H2.5C2.22386 5 2 4.77614 2 4.5C2 4.22386 2.22386 4 2.5 4H7.5ZM5.74131 15.2292C5.85775 16.2384 6.71225 17 7.72813 17H12.2719C13.2878 17 14.1422 16.2384 14.2587 15.2292L15.439 5H4.56101L5.74131 15.2292ZM8.5 7.5C8.77614 7.5 9 7.72386 9 8V14C9 14.2761 8.77614 14.5 8.5 14.5C8.22386 14.5 8 14.2761 8 14V8C8 7.72386 8.22386 7.5 8.5 7.5ZM12 8C12 7.72386 11.7761 7.5 11.5 7.5C11.2239 7.5 11 7.72386 11 8V14C11 14.2761 11.2239 14.5 11.5 14.5C11.7761 14.5 12 14.2761 12 14V8Z";
constexpr std::wstring_view kFluentIconAddCircle20RegularPath =
    L"M6 10C6 9.72386 6.22386 9.5 6.5 9.5H9.5V6.5C9.5 6.22386 9.72386 6 10 6C10.2761 6 10.5 6.22386 10.5 6.5V9.5H13.5C13.7761 9.5 14 9.72386 14 10C14 10.2761 13.7761 10.5 13.5 10.5H10.5V13.5C10.5 13.7761 10.2761 14 10 14C9.72386 14 9.5 13.7761 9.5 13.5V10.5H6.5C6.22386 10.5 6 10.2761 6 10ZM10 18C14.4183 18 18 14.4183 18 10C18 5.58172 14.4183 2 10 2C5.58172 2 2 5.58172 2 10C2 14.4183 5.58172 18 10 18ZM10 17C6.13401 17 3 13.866 3 10C3 6.13401 6.13401 3 10 3C13.866 3 17 6.13401 17 10C17 13.866 13.866 17 10 17Z";
constexpr std::wstring_view kFluentIconDismissCircle20RegularPath =
    L"M10 2C14.4183 2 18 5.58172 18 10C18 14.4183 14.4183 18 10 18C5.58172 18 2 14.4183 2 10C2 5.58172 5.58172 2 10 2ZM10 3C6.13401 3 3 6.13401 3 10C3 13.866 6.13401 17 10 17C13.866 17 17 13.866 17 10C17 6.13401 13.866 3 10 3ZM7.80943 7.11372L7.87868 7.17157L10 9.29289L12.1213 7.17157C12.2949 6.99801 12.5643 6.97872 12.7592 7.11372L12.8284 7.17157C13.002 7.34514 13.0213 7.61456 12.8863 7.80943L12.8284 7.87868L10.7071 10L12.8284 12.1213C13.002 12.2949 13.0213 12.5643 12.8863 12.7592L12.8284 12.8284C12.6549 13.002 12.3854 13.0213 12.1906 12.8863L12.1213 12.8284L10 10.7071L7.87868 12.8284C7.70511 13.002 7.43569 13.0213 7.24082 12.8863L7.17157 12.8284C6.99801 12.6549 6.97872 12.3854 7.11372 12.1906L7.17157 12.1213L9.29289 10L7.17157 7.87868C6.99801 7.70511 6.97872 7.43569 7.11372 7.24082L7.17157 7.17157C7.34514 6.99801 7.61456 6.97872 7.80943 7.11372Z";
constexpr std::wstring_view kFluentIconCheckmarkCircle20RegularPath =
    L"M10 2C14.4183 2 18 5.58172 18 10C18 14.4183 14.4183 18 10 18C5.58172 18 2 14.4183 2 10C2 5.58172 5.58172 2 10 2ZM10 3C6.13401 3 3 6.13401 3 10C3 13.866 6.13401 17 10 17C13.866 17 17 13.866 17 10C17 6.13401 13.866 3 10 3ZM13.3584 7.64645C13.532 7.82001 13.5513 8.08944 13.4163 8.28431L13.3584 8.35355L9.35355 12.3584C9.17999 12.532 8.91056 12.5513 8.71569 12.4163L8.64645 12.3584L6.64645 10.3584C6.45118 10.1632 6.45118 9.84658 6.64645 9.65131C6.82001 9.47775 7.08944 9.45846 7.28431 9.59346L7.35355 9.65131L9 11.298L12.6513 7.64645C12.8466 7.45118 13.1632 7.45118 13.3584 7.64645Z";
constexpr std::wstring_view kFluentIconSparkleCircle20RegularPath =
    L"M10 3C6.13401 3 3 6.13401 3 10C3 13.866 6.13401 17 10 17C13.866 17 17 13.866 17 10C17 6.13401 13.866 3 10 3ZM2 10C2 5.58172 5.58172 2 10 2C14.4183 2 18 5.58172 18 10C18 14.4183 14.4183 18 10 18C5.58172 18 2 14.4183 2 10ZM7.175 5.58341C7.44985 4.80579 8.54943 4.80544 8.82478 5.58288L9.08202 6.30917C9.18275 6.59357 9.4065 6.81729 9.69091 6.91798L10.4165 7.17487C11.1939 7.45008 11.1938 8.54945 10.4164 8.82457L9.69105 9.08128C9.40656 9.18196 9.18276 9.40573 9.08203 9.6902L8.82481 10.4166C8.5495 11.1942 7.44979 11.1938 7.17498 10.4161L6.91872 9.69091C6.81806 9.40604 6.59401 9.18196 6.30917 9.08126L5.58335 8.82467C4.8056 8.54971 4.80553 7.44982 5.58326 7.17477L6.30931 6.918C6.59408 6.81729 6.81807 6.59326 6.91873 6.30847L7.175 5.58341ZM12.6719 9.99954C12.4725 9.33434 11.5306 9.33406 11.3309 9.99915L11.1524 10.5934C11.0719 10.8616 10.862 11.0714 10.5938 11.1518L10.0008 11.3298C9.33542 11.5294 9.33576 12.4717 10.0013 12.6709L10.5932 12.848C10.8618 12.9283 11.0719 13.1383 11.1525 13.4068L11.3309 14.0016C11.5305 14.6668 12.4726 14.6666 12.6719 14.0013L12.8497 13.4081C12.9302 13.1397 13.1401 12.9296 13.4085 12.8491L14.0023 12.671C14.6675 12.4714 14.6675 11.5295 14.0023 11.33L13.4085 11.1519C13.1401 11.0714 12.9301 10.8614 12.8497 10.593L12.6719 9.99954Z";
constexpr std::wstring_view kFluentIconTextGrammarCheckmark20RegularPath =
    L"M17.5 5C17.7761 5 18 5.22386 18 5.5C18 5.77614 17.7761 6 17.5 6H2.5C2.22386 6 2 5.77614 2 5.5C2 5.22386 2.22386 5 2.5 5H17.5ZM17.5 8C17.7761 8 18 8.22386 18 8.5C18 8.77614 17.7761 9 17.5 9H2.5C2.22386 9 2 8.77614 2 8.5C2 8.22386 2.22386 8 2.5 8H17.5ZM9 14.5C9 14.3315 9.00758 14.1647 9.02242 14H2.5C2.22386 14 2 14.2239 2 14.5C2 14.7761 2.22386 15 2.5 15H9.02242C9.00758 14.8353 9 14.6685 9 14.5ZM9.59971 12C9.78261 11.6422 10.0035 11.3071 10.2572 11H2.5C2.22386 11 2 11.2239 2 11.5C2 11.7761 2.22386 12 2.5 12H9.59971ZM14.5 10C16.9853 10 19 12.0147 19 14.5C19 16.9853 16.9853 19 14.5 19C12.0147 19 10 16.9853 10 14.5C10 12.0147 12.0147 10 14.5 10ZM13.3788 15.7687L12.3804 14.5423L12.3174 14.4777C12.1373 14.3236 11.8673 14.3153 11.677 14.4703C11.4628 14.6447 11.4306 14.9596 11.6049 15.1737L12.9443 16.8187L13.0109 16.8864C13.2016 17.0466 13.4889 17.0438 13.6765 16.8654L17.189 13.2248L17.2486 13.157C17.3885 12.9656 17.376 12.6958 17.2069 12.5179C17.0166 12.3177 16.7001 12.3097 16.5 12.5L13.3788 15.7687Z";
constexpr std::wstring_view kFluentIconTextFont20RegularPath =
    L"M6.00059 2C6.2068 2.00001 6.39185 2.12661 6.46658 2.3188L8.89716 8.56965C8.90012 8.57674 8.90291 8.58392 8.90554 8.59118L9.07527 9.0277L8.54852 10.4324L8.09325 9.26151H3.90726L2.96613 11.6812C2.86603 11.9386 2.57625 12.0661 2.31889 11.966C2.06153 11.8659 1.93404 11.5761 2.03414 11.3188L3.09501 8.59113C3.09764 8.58388 3.10043 8.57671 3.10338 8.56961L5.53458 2.31876C5.60932 2.12657 5.79439 1.99999 6.00059 2ZM4.29619 8.26151H7.70441L6.00051 3.87951L4.29619 8.26151ZM12.467 5.32118C12.3927 5.12714 12.2061 4.99928 11.9983 5C11.7906 5.00073 11.6049 5.12988 11.5319 5.32444L7.15357 17L6.50013 17C6.22399 17 6.00013 17.2239 6.00012 17.5C6.00012 17.7761 6.22397 18 6.50011 18L7.49054 18C7.49686 18.0002 7.50316 18.0002 7.50946 18L8.50011 18.0001C8.77625 18.0001 9.00012 17.7762 9.00012 17.5001C9.00013 17.2239 8.77628 17.0001 8.50013 17.0001L8.22156 17.0001L9.34658 14H14.72L15.8687 16.9993L15.5001 16.9993C15.224 16.9993 15.0001 17.2231 15.0001 17.4993C15.0001 17.7754 15.224 17.9993 15.5001 17.9993L17.5001 17.9993C17.7763 17.9993 18.0001 17.7755 18.0001 17.4993C18.0001 17.2232 17.7763 16.9993 17.5001 16.9993L16.9395 16.9993L12.467 5.32118ZM14.337 13H9.72158L12.005 6.91088L14.337 13Z";
constexpr std::wstring_view kFluentIconToggleLeft20RegularPath =
    L"M6 12C4.89543 12 4 11.1046 4 10C4 8.89543 4.89543 8 6 8C7.10457 8 8 8.89543 8 10C8 11.1046 7.10457 12 6 12ZM18 10C18 7.79086 16.2091 6 14 6H6C3.79086 6 2 7.79086 2 10C2 12.2091 3.79086 14 6 14H14C16.2091 14 18 12.2091 18 10ZM14 7C15.6569 7 17 8.34315 17 10C17 11.6569 15.6569 13 14 13H6C4.34315 13 3 11.6569 3 10C3 8.34315 4.34315 7 6 7H14Z";
constexpr std::wstring_view kFluentIconToggleRight20RegularPath =
    L"M14 12C15.1046 12 16 11.1046 16 10C16 8.89543 15.1046 8 14 8C12.8954 8 12 8.89543 12 10C12 11.1046 12.8954 12 14 12ZM2 10C2 7.79086 3.79086 6 6 6H14C16.2091 6 18 7.79086 18 10C18 12.2091 16.2091 14 14 14H6C3.79086 14 2 12.2091 2 10ZM6 7C4.34315 7 3 8.34315 3 10C3 11.6569 4.34315 13 6 13H14C15.6569 13 17 11.6569 17 10C17 8.34315 15.6569 7 14 7H6Z";
constexpr std::wstring_view kFluentIconCommentOff20RegularPath =
    L"M2.85355 2.14645C2.65829 1.95118 2.34171 1.95118 2.14645 2.14645C1.95118 2.34171 1.95118 2.65829 2.14645 2.85355L2.90934 3.61645C2.35272 4.08703 2 4.78577 2 5.56581V12.2764C2 13.6935 3.16406 14.8422 4.6 14.8422H5.19937L5.2 17.0132C5.2 17.2262 5.26989 17.4335 5.39921 17.6041C5.73 18.0406 6.35668 18.1298 6.79895 17.8033L10.81 14.8422H14.1351L17.1464 17.8536C17.3417 18.0488 17.6583 18.0488 17.8536 17.8536C18.0488 17.6583 18.0488 17.3417 17.8536 17.1464L15.5454 14.8383C15.5454 14.8383 15.5454 14.8383 15.5454 14.8383L3.8235 3.11637C3.8235 3.11637 3.82351 3.11636 3.8235 3.11637L2.85355 2.14645ZM13.1351 13.8422H10.4809L6.20502 16.9988L6.20346 16.9997L6.2 17L6.19908 13.8422H4.6C3.70383 13.8422 3 13.1288 3 12.2764V5.56581C3 5.06712 3.24093 4.61598 3.62034 4.32745L13.1351 13.8422ZM17 12.2764C17 12.9658 16.5396 13.5643 15.8894 13.768L16.6488 14.5274C17.4541 14.0914 18 13.2468 18 12.2764V5.56581C18 4.14874 16.8359 2.99999 15.4 2.99999H5.12134L6.12134 3.99999H15.4C16.2962 3.99999 17 4.71346 17 5.56581V12.2764Z";
constexpr std::wstring_view kFluentIconComment20RegularPath =
    L"M10.4809 13.8423H15.4C16.2962 13.8423 17 13.1288 17 12.2764V5.56582C17 4.71348 16.2962 4 15.4 4H4.6C3.70383 4 3 4.71348 3 5.56582V12.2764C3 13.1288 3.70383 13.8423 4.6 13.8423H6.19908L6.2 17L6.20346 16.9997L6.20502 16.9988L10.4809 13.8423ZM6.79895 17.8034C6.35668 18.1298 5.73 18.0406 5.39921 17.6042C5.26989 17.4335 5.2 17.2262 5.2 17.0133L5.19937 14.8423H4.6C3.16406 14.8423 2 13.6935 2 12.2764V5.56582C2 4.14876 3.16406 3 4.6 3H15.4C16.8359 3 18 4.14876 18 5.56582V12.2764C18 13.6935 16.8359 14.8423 15.4 14.8423H10.81L6.79895 17.8034Z";
constexpr std::wstring_view kFluentIconCommentNote20RegularPath =
    L"M11.5 1C10.6716 1 10 1.67157 10 2.5V7.5C10 8.32843 10.6716 9 11.5 9H17.5C18.3284 9 19 8.32843 19 7.5V2.5C19 1.67157 18.3284 1 17.5 1H11.5ZM12.5 6H16.5C16.7761 6 17 6.22386 17 6.5C17 6.77614 16.7761 7 16.5 7H12.5C12.2239 7 12 6.77614 12 6.5C12 6.22386 12.2239 6 12.5 6ZM12 3.5C12 3.22386 12.2239 3 12.5 3H16.5C16.7761 3 17 3.22386 17 3.5C17 3.77614 16.7761 4 16.5 4H12.5C12.2239 4 12 3.77614 12 3.5ZM4.6 3H9V4H4.6C3.70383 4 3 4.71348 3 5.56582V12.2764C3 13.1288 3.70383 13.8423 4.6 13.8423H6.19908L6.2 17L6.20346 16.9997L6.20502 16.9988L10.4809 13.8423H15.4C16.2962 13.8423 17 13.1288 17 12.2764V10H17.5C17.6712 10 17.8384 9.98278 18 9.94999V12.2764C18 13.6935 16.8359 14.8423 15.4 14.8423H10.81L6.79895 17.8034C6.35668 18.1298 5.73 18.0406 5.39921 17.6042C5.26989 17.4335 5.2 17.2262 5.2 17.0133L5.19937 14.8423H4.6C3.16406 14.8423 2 13.6935 2 12.2764V5.56582C2 4.14876 3.16406 3 4.6 3Z";
constexpr std::wstring_view kFluentIconCommentEdit20RegularPath =
    L"M17 8.13362V5.56582C17 4.71348 16.2962 4 15.4 4H4.6C3.70383 4 3 4.71348 3 5.56582V12.2764C3 13.1288 3.70383 13.8423 4.6 13.8423H6.19908L6.2 17L6.20346 16.9997L6.20502 16.9988L8.3716 15.3994L8.05813 16.6533C8.03677 16.7387 8.0215 16.8237 8.01199 16.9078L6.79895 17.8034C6.35668 18.1298 5.73 18.0406 5.39921 17.6042C5.26989 17.4335 5.2 17.2262 5.2 17.0133L5.19937 14.8423H4.6C3.16406 14.8423 2 13.6935 2 12.2764V5.56582C2 4.14876 3.16406 3 4.6 3H15.4C16.8359 3 18 4.14876 18 5.56582V8.69046C17.6997 8.43265 17.3588 8.24703 17 8.13362ZM14.8072 9.54599L9.9778 14.3754C9.69622 14.657 9.49647 15.0098 9.39989 15.3961L9.02541 16.894C8.86256 17.5454 9.45261 18.1355 10.104 17.9726L11.6019 17.5981C11.9882 17.5016 12.341 17.3018 12.6226 17.0202L17.452 12.1908C18.1824 11.4605 18.1824 10.2763 17.452 9.54599C16.7217 8.81564 15.5376 8.81564 14.8072 9.54599Z";
constexpr std::wstring_view kFluentIconKeyboard20RegularPath =
    L"M5 12.5C5 12.2239 5.22386 12 5.5 12H14.5C14.7761 12 15 12.2239 15 12.5C15 12.7761 14.7761 13 14.5 13H5.5C5.22386 13 5 12.7761 5 12.5ZM11.5024 8.00468C11.9179 8.00468 12.2547 7.66783 12.2547 7.25231C12.2547 6.83679 11.9179 6.49994 11.5024 6.49994C11.0868 6.49994 10.75 6.83679 10.75 7.25231C10.75 7.66783 11.0868 8.00468 11.5024 8.00468ZM15.2547 7.25231C15.2547 7.66783 14.9179 8.00468 14.5024 8.00468C14.0868 8.00468 13.75 7.66783 13.75 7.25231C13.75 6.83679 14.0868 6.49994 14.5024 6.49994C14.9179 6.49994 15.2547 6.83679 15.2547 7.25231ZM5.50237 8.00468C5.91789 8.00468 6.25474 7.66783 6.25474 7.25231C6.25474 6.83679 5.91789 6.49994 5.50237 6.49994C5.08685 6.49994 4.75 6.83679 4.75 7.25231C4.75 7.66783 5.08685 8.00468 5.50237 8.00468ZM7.74998 9.75231C7.74998 10.1678 7.41313 10.5047 6.99761 10.5047C6.58209 10.5047 6.24524 10.1678 6.24524 9.75231C6.24524 9.33679 6.58209 8.99994 6.99761 8.99994C7.41313 8.99994 7.74998 9.33679 7.74998 9.75231ZM10.0024 10.5047C10.4179 10.5047 10.7547 10.1678 10.7547 9.75231C10.7547 9.33679 10.4179 8.99994 10.0024 8.99994C9.58685 8.99994 9.25 9.33679 9.25 9.75231C9.25 10.1678 9.58685 10.5047 10.0024 10.5047ZM13.7595 9.75231C13.7595 10.1678 13.4227 10.5047 13.0071 10.5047C12.5916 10.5047 12.2548 10.1678 12.2548 9.75231C12.2548 9.33679 12.5916 8.99994 13.0071 8.99994C13.4227 8.99994 13.7595 9.33679 13.7595 9.75231ZM8.50237 8.00468C8.91789 8.00468 9.25474 7.66783 9.25474 7.25231C9.25474 6.83679 8.91789 6.49994 8.50237 6.49994C8.08685 6.49994 7.75 6.83679 7.75 7.25231C7.75 7.66783 8.08685 8.00468 8.50237 8.00468ZM2 5.5C2 4.67157 2.67157 4 3.5 4H16.5C17.3284 4 18 4.67157 18 5.5V13.5C18 14.3284 17.3284 15 16.5 15H3.5C2.67157 15 2 14.3284 2 13.5V5.5ZM3.5 5C3.22386 5 3 5.22386 3 5.5V13.5C3 13.7761 3.22386 14 3.5 14H16.5C16.7761 14 17 13.7761 17 13.5V5.5C17 5.22386 16.7761 5 16.5 5H3.5Z";
constexpr std::wstring_view kFluentIconFlash20RegularPath =
    L"M6.19079 2.77054C6.3211 2.31445 6.73797 2 7.21231 2H12.4614C13.1865 2 13.6986 2.71043 13.4693 3.39836L13.4668 3.40584L13.4667 3.40582L12.2053 7H14.7691C15.7155 7 16.1764 8.1436 15.5356 8.81137L15.532 8.81508L15.532 8.81506L6.85551 17.6726C6.10113 18.4551 4.79636 17.7329 5.06026 16.6773L6.22998 11.9984H4.96271C4.25687 11.9984 3.74727 11.3228 3.94118 10.6442L6.19079 2.77054ZM7.21231 3C7.18445 3 7.15996 3.01847 7.15231 3.04526L4.90271 10.9189C4.89132 10.9587 4.92125 10.9984 4.96271 10.9984H6.87037C7.02434 10.9984 7.16972 11.0694 7.26447 11.1907C7.35923 11.3121 7.39279 11.4703 7.35544 11.6197L6.03041 16.9198C6.02649 16.9355 6.02679 16.9448 6.02721 16.949C6.02765 16.9534 6.02868 16.9568 6.03027 16.9601C6.03383 16.9676 6.04314 16.9798 6.06076 16.9896C6.07838 16.9993 6.09368 17.0007 6.10191 16.9997C6.10557 16.9993 6.109 16.9984 6.11295 16.9964C6.11674 16.9945 6.12478 16.9898 6.13597 16.9782L6.13952 16.9745L6.13954 16.9745L14.8151 8.11787C14.8273 8.10484 14.8305 8.09481 14.8318 8.0864C14.8336 8.07504 14.8325 8.05898 14.8253 8.04178C14.8181 8.0246 14.8079 8.01365 14.8002 8.00817C14.7949 8.00438 14.7869 8 14.7691 8H11.5C11.3378 8 11.1858 7.9214 11.092 7.78915C10.9983 7.65689 10.9745 7.48739 11.0282 7.33443L12.5212 3.08022C12.5331 3.04042 12.5033 3 12.4614 3H7.21231Z";
constexpr std::wstring_view kFluentIconFlashOff20RegularPath =
    L"M5.27265 5.97975L2.14645 2.85355C1.95118 2.65829 1.95118 2.34171 2.14645 2.14645C2.34171 1.95118 2.65829 1.95118 2.85355 2.14645L17.8536 17.1464C18.0488 17.3417 18.0488 17.6583 17.8536 17.8536C17.6583 18.0488 17.3417 18.0488 17.1464 17.8536L11.8577 12.5648L6.85429 17.6726C6.09991 18.4551 4.79514 17.7329 5.05904 16.6773L6.22876 11.9984H4.96148C4.25565 11.9984 3.74605 11.3228 3.93996 10.6442L5.27265 5.97975ZM11.1506 11.8577L6.08155 6.78865L4.90148 10.9189C4.8901 10.9587 4.92003 10.9984 4.96148 10.9984H6.86915C7.02312 10.9984 7.1685 11.0694 7.26325 11.1907C7.35801 11.3121 7.39156 11.4703 7.35422 11.6197L6.02919 16.9198C6.02527 16.9355 6.02557 16.9448 6.02599 16.949C6.02643 16.9534 6.02746 16.9568 6.02904 16.9601C6.03261 16.9676 6.04192 16.9798 6.05954 16.9896C6.07716 16.9993 6.09246 17.0007 6.10069 16.9997C6.10435 16.9993 6.10778 16.9984 6.11173 16.9964C6.11552 16.9945 6.12356 16.9898 6.13475 16.9782L6.13832 16.9745L11.1506 11.8577ZM14.8139 8.11787L12.5501 10.429L13.2572 11.1361L15.5308 8.81506L15.5344 8.81137C16.1752 8.1436 15.7143 7 14.7678 7H12.2041L13.4655 3.40582L13.468 3.39836C13.6974 2.71043 13.1853 2 12.4602 2H7.21109C6.73675 2 6.31988 2.31445 6.18956 2.77054L5.90113 3.78004L6.71004 4.58894L7.15109 3.04526C7.15874 3.01847 7.18323 3 7.21109 3H12.4602C12.5021 3 12.5319 3.04042 12.52 3.08022L11.027 7.33443C10.9733 7.48739 10.9971 7.65689 11.0908 7.78915C11.1846 7.9214 11.3366 8 11.4987 8H14.7678C14.7857 8 14.7936 8.00438 14.799 8.00817C14.8067 8.01365 14.8168 8.0246 14.824 8.04178C14.8313 8.05898 14.8324 8.07504 14.8306 8.0864C14.8293 8.09481 14.8261 8.10484 14.8139 8.11787Z";
constexpr std::wstring_view kFluentIconSearchSparkle20RegularPath =
    L"M8.03711 3.16699C8.01223 3.27312 8 3.38325 8 3.49609C8 3.73436 8.05675 3.95999 8.16504 4.16309C5.77256 4.7596 4 6.92282 4 9.5C4 12.5376 6.46243 15 9.5 15C10.8388 15 12.0658 14.5216 13.0195 13.7266C13.276 13.5128 13.5128 13.276 13.7266 13.0195C14.0768 12.5994 14.3646 12.1257 14.5781 11.6133C14.6326 11.6691 14.6909 11.7222 14.7559 11.7695C14.9747 11.9189 15.2335 11.9981 15.502 11.999L15.4971 12H15.501C15.2369 12.6332 14.8766 13.2164 14.4365 13.7295L17.8535 17.1465C18.0487 17.3417 18.0487 17.6583 17.8535 17.8535C17.68 18.0271 17.4107 18.0461 17.2158 17.9111L17.1465 17.8535L13.7295 14.4365C12.5927 15.4114 11.115 16 9.5 16C5.91015 16 3 13.0899 3 9.5C3 6.4133 5.15192 3.83075 8.03711 3.16699ZM15.4844 6C15.5469 6 15.6081 6.01957 15.6592 6.05566C15.7101 6.09167 15.7487 6.14242 15.7695 6.20117L16.0186 6.9668C16.0959 7.19922 16.2271 7.41078 16.4004 7.58398C16.5737 7.75703 16.7851 7.88761 17.0176 7.96484L17.7832 8.21289L17.7979 8.2168C17.8568 8.23756 17.9082 8.27623 17.9443 8.32715C17.9805 8.37818 18 8.43944 18 8.50195C18 8.56445 17.9805 8.62574 17.9443 8.67676C17.9082 8.72765 17.8567 8.76635 17.7979 8.78711L17.0332 9.03516C16.8005 9.11244 16.5884 9.24281 16.415 9.41602C16.2417 9.58919 16.1116 9.80083 16.0342 10.0332L15.7852 10.7988C15.7643 10.8576 15.7257 10.9083 15.6748 10.9443C15.6237 10.9804 15.5626 11 15.5 11C15.4374 11 15.3763 10.9804 15.3252 10.9443C15.2743 10.9083 15.2357 10.8576 15.2148 10.7988L14.9658 10.0332C14.889 9.80017 14.7592 9.58791 14.5859 9.41406C14.4125 9.2401 14.1999 9.10893 13.9668 9.03125L13.2021 8.7832C13.1432 8.76244 13.0918 8.72377 13.0557 8.67285C13.0195 8.62182 13 8.56056 13 8.49805C13 8.43555 13.0195 8.37426 13.0557 8.32324C13.0918 8.27236 13.1433 8.23365 13.2021 8.21289L13.9668 7.96484C14.1966 7.88555 14.4056 7.75415 14.5762 7.58105C14.7468 7.40795 14.8753 7.19762 14.9512 6.9668L15.1992 6.20117C15.22 6.14249 15.2588 6.0917 15.3096 6.05566C15.3606 6.01962 15.4219 6.00007 15.4844 6ZM12.4785 0C12.5661 0 12.6512 0.0276075 12.7227 0.078125C12.7942 0.128677 12.8488 0.199695 12.8779 0.282227L13.2256 1.35352C13.3339 1.67888 13.5171 1.97432 13.7598 2.2168C14.0024 2.45924 14.2984 2.64176 14.624 2.75L15.6963 3.09863L15.7178 3.10352C15.8003 3.13264 15.8713 3.18738 15.9219 3.25879C15.9724 3.33021 16 3.41546 16 3.50293C15.9999 3.59036 15.9724 3.6757 15.9219 3.74707C15.8713 3.81832 15.8002 3.87226 15.7178 3.90137L14.6455 4.25C14.3199 4.35824 14.0239 4.54075 13.7812 4.7832C13.5386 5.02568 13.3554 5.32112 13.2471 5.64648L12.8994 6.71777C12.8703 6.80031 12.8156 6.87132 12.7441 6.92188C12.6727 6.97241 12.5876 7 12.5 7C12.4124 7 12.3273 6.97241 12.2559 6.92188C12.249 6.91702 12.2419 6.91246 12.2354 6.90723C12.1741 6.85803 12.1269 6.79217 12.1006 6.71777L11.7529 5.64648C11.7342 5.5898 11.7126 5.53427 11.6895 5.47949C11.5795 5.21896 11.4202 4.98141 11.2197 4.78027C11.1817 4.74208 11.1422 4.7051 11.1016 4.66992C10.8833 4.48092 10.6295 4.33582 10.3545 4.24414L9.28223 3.89648C9.19968 3.86736 9.12869 3.81261 9.07812 3.74121C9.02759 3.66979 9 3.58454 9 3.49707C9.00005 3.40964 9.02758 3.3243 9.07812 3.25293C9.12865 3.18168 9.19983 3.12774 9.28223 3.09863L10.3545 2.75C10.6761 2.63894 10.9683 2.45518 11.207 2.21289C11.4457 1.97062 11.6253 1.6765 11.7314 1.35352L12.0791 0.282227C12.1082 0.199695 12.1629 0.128677 12.2344 0.078125C12.3059 0.027586 12.391 1.0639e-05 12.4785 0Z";
constexpr std::wstring_view kFluentIconLightbulbFilament20RegularPath =
    L"M9.5 6.50238C9.5 6.22624 9.72386 6.00238 10 6.00238C10.2761 6.00238 10.5 6.22624 10.5 6.50238V7.50391C10.5 7.78005 10.2761 8.00391 10 8.00391C9.72386 8.00391 9.5 7.78005 9.5 7.50391V6.50238ZM12.8506 7.44332C12.6553 7.24806 12.3388 7.24806 12.1435 7.44332L11.4353 8.15151C11.2401 8.34677 11.2401 8.66335 11.4353 8.85861C11.6306 9.05388 11.9472 9.05388 12.1424 8.85861L12.8506 8.15043C13.0459 7.95517 13.0459 7.63858 12.8506 7.44332ZM7.8521 7.44332C7.65684 7.24806 7.34026 7.24806 7.145 7.44332C6.94973 7.63858 6.94973 7.95517 7.145 8.15043L7.85318 8.85861C8.04844 9.05388 8.36503 9.05388 8.56029 8.85861C8.75555 8.66335 8.75555 8.34677 8.56029 8.15151L7.8521 7.44332ZM10 2C13.3137 2 16 4.59693 16 7.80041C16 9.47737 15.2546 11.0164 13.7961 12.3942C13.7324 12.4544 13.6831 12.5269 13.6512 12.6065L13.6251 12.6883L12.6891 16.6051C12.5048 17.3763 11.8236 17.935 11.0181 17.9947L10.8748 18H9.12546C8.30655 18 7.59 17.4839 7.34866 16.7385L7.31108 16.6047L6.37626 12.6886C6.34955 12.5766 6.29016 12.4745 6.20516 12.3942C4.8153 11.0819 4.07265 9.62354 4.00507 8.03903L4 7.80041L4.00321 7.60894C4.1077 4.49409 6.75257 2 10 2ZM7.955 15L8.27386 16.3344L8.30004 16.4305C8.39695 16.7298 8.67583 16.9517 9.0116 16.993L9.12546 17L10.8379 17.0007L10.9442 16.9974C11.2865 16.9721 11.5726 16.7609 11.6854 16.4718L11.7165 16.3727L12.045 15H7.955ZM10 3C7.36782 3 5.21188 4.95301 5.0151 7.41357L5.00307 7.62569L4.99977 7.77916L5.00416 7.99642C5.05977 9.30026 5.67758 10.5208 6.89167 11.6671C7.07995 11.8449 7.22191 12.0647 7.30572 12.3078L7.34894 12.4564L7.716 14H9.50024V9.49707C9.50024 9.22093 9.7241 8.99707 10.0002 8.99707C10.2764 8.99707 10.5002 9.22093 10.5002 9.49707V14H12.285L12.6722 12.3851L12.7231 12.2343C12.8091 12.0198 12.9409 11.8265 13.1094 11.6673C14.3825 10.4646 15 9.18054 15 7.80041C15 5.15693 12.7689 3 10 3Z";
constexpr std::wstring_view kFluentIconLightbulb20RegularPath =
    L"M10 2C13.3137 2 16 4.59693 16 7.80041C16 9.47737 15.2546 11.0164 13.7961 12.3942C13.7324 12.4544 13.6831 12.5269 13.6512 12.6065L13.6251 12.6883L12.6891 16.6051C12.5048 17.3763 11.8236 17.935 11.0181 17.9947L10.8748 18H9.12546C8.30655 18 7.59 17.4839 7.34866 16.7385L7.31108 16.6047L6.37626 12.6886C6.34955 12.5766 6.29016 12.4745 6.20516 12.3942C4.8153 11.0819 4.07265 9.62354 4.00507 8.03903L4 7.80041L4.00321 7.60894C4.1077 4.49409 6.75257 2 10 2ZM12.045 15H7.955L8.27386 16.3344L8.30004 16.4305C8.39695 16.7298 8.67583 16.9517 9.0116 16.993L9.12546 17L10.8379 17.0007L10.9442 16.9974C11.2865 16.9721 11.5726 16.7609 11.6854 16.4718L11.7165 16.3727L12.045 15ZM10 3C7.36782 3 5.21188 4.95301 5.0151 7.41357L5.00307 7.62569L4.99977 7.77916L5.00416 7.99642C5.05977 9.30026 5.67758 10.5208 6.89167 11.6671C7.07995 11.8449 7.22191 12.0647 7.30572 12.3078L7.34894 12.4564L7.716 14H12.285L12.6722 12.3851L12.7231 12.2343C12.8091 12.0198 12.9409 11.8265 13.1094 11.6673C14.3825 10.4646 15 9.18054 15 7.80041C15 5.15693 12.7689 3 10 3Z";
constexpr std::wstring_view kFluentIconRulesEdit20RegularPath =
    L"M5.75 3H14.25C15.7688 3 17 4.23122 17 5.75V9.00299C16.6586 9.01856 16.3194 9.09477 16 9.23163V5.75C16 4.7835 15.2165 4 14.25 4H5.75C4.7835 4 4 4.7835 4 5.75V14.25C4 15.2165 4.7835 16 5.75 16H9.47466C9.45904 16.0513 9.44469 16.1031 9.43163 16.1554L9.22047 17H5.75C4.23122 17 3 15.7688 3 14.25V5.75C3 4.23122 4.23122 3 5.75 3ZM9.5 13H11.9427L10.9427 14H9.5C9.22386 14 9 13.7761 9 13.5C9 13.2239 9.22386 13 9.5 13ZM7.5 7.25C7.5 7.66421 7.16421 8 6.75 8C6.33579 8 6 7.66421 6 7.25C6 6.83579 6.33579 6.5 6.75 6.5C7.16421 6.5 7.5 6.83579 7.5 7.25ZM6.75 11C7.16421 11 7.5 10.6642 7.5 10.25C7.5 9.83579 7.16421 9.5 6.75 9.5C6.33579 9.5 6 9.83579 6 10.25C6 10.6642 6.33579 11 6.75 11ZM6.75 14C7.16421 14 7.5 13.6642 7.5 13.25C7.5 12.8358 7.16421 12.5 6.75 12.5C6.33579 12.5 6 12.8358 6 13.25C6 13.6642 6.33579 14 6.75 14ZM9.5 7C9.22386 7 9 7.22386 9 7.5C9 7.77614 9.22386 8 9.5 8H13.5C13.7761 8 14 7.77614 14 7.5C14 7.22386 13.7761 7 13.5 7H9.5ZM9.5 10C9.22386 10 9 10.2239 9 10.5C9 10.7761 9.22386 11 9.5 11H13.5C13.7761 11 14 10.7761 14 10.5C14 10.2239 13.7761 10 13.5 10H9.5ZM10.9798 15.3772L15.8092 10.5478C16.5395 9.81741 17.7237 9.81741 18.454 10.5478C19.1843 11.2781 19.1843 12.4622 18.454 13.1926L13.6246 18.022C13.343 18.3036 12.9902 18.5033 12.6039 18.5999L11.106 18.9744C10.4546 19.1372 9.86451 18.5472 10.0274 17.8958L10.4018 16.3979C10.4984 16.0116 10.6982 15.6588 10.9798 15.3772Z";
constexpr std::wstring_view kFluentIconRename20FilledPath =
    L"M8.5 2C8.22386 2 8 2.22386 8 2.5C8 2.77614 8.22386 3 8.5 3H9.5V17H8.5C8.22386 17 8 17.2239 8 17.5C8 17.7761 8.22386 18 8.5 18H11.5C11.7761 18 12 17.7761 12 17.5C12 17.2239 11.7761 17 11.5 17H10.5V3H11.5C11.7761 3 12 2.77614 12 2.5C12 2.22386 11.7761 2 11.5 2H8.5ZM4.5 4H8.5V16H4.5C3.11929 16 2 14.8807 2 13.5V6.5C2 5.11929 3.11929 4 4.5 4ZM15.5 16H11.5V4H15.5C16.8807 4 18 5.11929 18 6.5V13.5C18 14.8807 16.8807 16 15.5 16Z";
constexpr std::wstring_view kFluentIconRectangleLandscape20FilledPath =
    L"M5 4C3.34315 4 2 5.34315 2 7V13C2 14.6569 3.34315 16 5 16H15C16.6569 16 18 14.6569 18 13V7C18 5.34315 16.6569 4 15 4H5Z";
constexpr std::wstring_view kFluentIconFlash20FilledPath =
    L"M7.21231 2C6.73797 2 6.3211 2.31445 6.19079 2.77054L3.94118 10.6442C3.74727 11.3228 4.25687 11.9984 4.96271 11.9984H6.22998L5.06026 16.6773C4.79636 17.7329 6.10113 18.4551 6.85551 17.6726L15.532 8.81506L15.5356 8.81137C16.1764 8.1436 15.7155 7 14.7691 7H12.2053L13.4667 3.40582L13.4693 3.39836C13.6986 2.71043 13.1865 2 12.4614 2H7.21231Z";
constexpr std::wstring_view kFluentIconFlashOff20FilledPath =
    L"M11.8577 12.5648L17.1464 17.8536C17.3417 18.0488 17.6583 18.0488 17.8536 17.8536C18.0488 17.6583 18.0488 17.3417 17.8536 17.1464L2.85355 2.14645C2.65829 1.95118 2.34171 1.95118 2.14645 2.14645C1.95118 2.34171 1.95118 2.65829 2.14645 2.85355L5.27265 5.97975L3.93996 10.6442C3.74605 11.3228 4.25565 11.9984 4.96148 11.9984H6.22876L5.05904 16.6773C4.79514 17.7329 6.09991 18.4551 6.85429 17.6726L11.8577 12.5648ZM15.5308 8.81506L13.2572 11.1361L5.90113 3.78004L6.18956 2.77054C6.31988 2.31445 6.73675 2 7.21109 2H12.4602C13.1853 2 13.6974 2.71043 13.468 3.39836L13.4655 3.40582L12.2041 7H14.7678C15.7143 7 16.1752 8.1436 15.5344 8.81137L15.5308 8.81506Z";
constexpr std::wstring_view kFluentIconLightbulbFilament20FilledPath =
    L"M13.073 15L12.6891 16.6051C12.5048 17.3763 11.8236 17.935 11.0181 17.9947L10.8748 18H9.12546C8.30655 18 7.59 17.4839 7.34866 16.7385L7.31108 16.6047L6.928 15H13.073ZM10 2C13.3137 2 16 4.59693 16 7.80041C16 9.47737 15.2546 11.0164 13.7961 12.3942C13.7324 12.4544 13.6831 12.5269 13.6512 12.6065L13.6251 12.6883L13.311 14H10.5002V9.49707C10.5002 9.22093 10.2764 8.99707 10.0002 8.99707C9.7241 8.99707 9.50024 9.22093 9.50024 9.49707V14H6.689L6.37626 12.6886C6.34955 12.5766 6.29016 12.4745 6.20516 12.3942C4.8153 11.0819 4.07265 9.62354 4.00507 8.03903L4 7.80041L4.00321 7.60894C4.1077 4.49409 6.75257 2 10 2ZM9.5 6.50238V7.50391C9.5 7.78005 9.72386 8.00391 10 8.00391C10.2761 8.00391 10.5 7.78005 10.5 7.50391V6.50238C10.5 6.22624 10.2761 6.00238 10 6.00238C9.72386 6.00238 9.5 6.22624 9.5 6.50238ZM12.8506 7.44332C12.6553 7.24806 12.3388 7.24806 12.1435 7.44332L11.4353 8.15151C11.2401 8.34677 11.2401 8.66335 11.4353 8.85861C11.6306 9.05388 11.9472 9.05388 12.1424 8.85861L12.8506 8.15043C13.0459 7.95517 13.0459 7.63858 12.8506 7.44332ZM7.8521 7.44332C7.65684 7.24806 7.34026 7.24806 7.145 7.44332C6.94973 7.63858 6.94973 7.95517 7.145 8.15043L7.85318 8.85861C8.04844 9.05388 8.36503 9.05388 8.56029 8.85861C8.75555 8.66335 8.75555 8.34677 8.56029 8.15151L7.8521 7.44332Z";
constexpr std::wstring_view kFluentIconLightbulb20FilledPath =
    L"M13.073 15L12.6891 16.6051C12.5048 17.3763 11.8236 17.935 11.0181 17.9947L10.8748 18H9.12546C8.30655 18 7.59 17.4839 7.34866 16.7385L7.31108 16.6047L6.928 15H13.073ZM10 2C13.3137 2 16 4.59693 16 7.80041C16 9.47737 15.2546 11.0164 13.7961 12.3942C13.7324 12.4544 13.6831 12.5269 13.6512 12.6065L13.6251 12.6883L13.311 14H6.689L6.37626 12.6886C6.34955 12.5766 6.29016 12.4745 6.20516 12.3942C4.8153 11.0819 4.07265 9.62354 4.00507 8.03903L4 7.80041L4.00321 7.60894C4.1077 4.49409 6.75257 2 10 2Z";
constexpr std::wstring_view kFluentIconEdit20FilledPath =
    L"M12.9203 2.87317C14.1027 1.69077 16.0271 1.71505 17.1794 2.92689C18.2913 4.09631 18.2681 5.93899 17.1271 7.08003L16.4581 7.74902L12.2512 3.54217L12.9203 2.87317ZM11.5441 4.24927L3.54545 12.2475C3.21763 12.5754 2.99008 12.9899 2.88953 13.4424L2.01191 17.3923C1.97483 17.5592 2.02559 17.7335 2.14649 17.8544C2.26739 17.9753 2.44166 18.026 2.60855 17.9889L6.53494 17.1157C7.00237 17.0118 7.43048 16.7767 7.76907 16.4381L15.751 8.45613L11.5441 4.24927Z";
constexpr std::wstring_view kFluentIconEditOff20FilledPath =
    L"M2.85355 2.14645C2.65829 1.95118 2.34171 1.95118 2.14645 2.14645C1.95118 2.34171 1.95118 2.65829 2.14645 2.85355L7.54304 8.25015L3.54545 12.2475C3.21763 12.5754 2.99008 12.9899 2.88953 13.4424L2.01191 17.3923C1.97483 17.5592 2.02559 17.7335 2.14649 17.8544C2.26739 17.9753 2.44166 18.026 2.60855 17.9889L6.53494 17.1157C7.00237 17.0118 7.43048 16.7767 7.76907 16.4381L11.75 12.4571L17.1464 17.8536C17.3417 18.0488 17.6583 18.0488 17.8536 17.8536C18.0488 17.6583 18.0488 17.3417 17.8536 17.1464L2.85355 2.14645ZM15.751 8.45613L13.1641 11.043L8.95717 6.83608L11.5441 4.24927L15.751 8.45613ZM12.9203 2.87317C14.1027 1.69077 16.0271 1.71505 17.1794 2.92689C18.2913 4.09631 18.2681 5.93899 17.1271 7.08003L16.4581 7.74902L12.2512 3.54217L12.9203 2.87317Z";
constexpr std::wstring_view kFluentIconApprovalsApp20FilledPath =
    L"M9.78033 0.71967C9.48744 0.426777 9.01256 0.426777 8.71967 0.71967C8.42678 1.01256 8.42678 1.48744 8.71967 1.78033L9.93934 3H9.5C5.35786 3 2 6.35786 2 10.5C2 14.6421 5.35786 18 9.5 18C13.6421 18 17 14.6421 17 10.5C17 10.0858 16.6642 9.75 16.25 9.75C15.8358 9.75 15.5 10.0858 15.5 10.5C15.5 13.8137 12.8137 16.5 9.5 16.5C6.18629 16.5 3.5 13.8137 3.5 10.5C3.5 7.20664 6.15341 4.53301 9.43904 4.5003L8.71967 5.21967C8.42678 5.51256 8.42678 5.98744 8.71967 6.28033C9.01256 6.57322 9.48744 6.57322 9.78033 6.28033L12.0303 4.03033C12.3232 3.73744 12.3232 3.26256 12.0303 2.96967L9.78033 0.71967ZM13.5201 6.95963C13.8185 7.24688 13.8276 7.72166 13.5404 8.0201L9.69039 12.0201C9.549 12.167 9.35391 12.25 9.15002 12.25C8.94613 12.25 8.75104 12.167 8.60965 12.0201L6.95963 10.3057C6.67239 10.0073 6.68147 9.53249 6.97991 9.24525C7.27835 8.95801 7.75314 8.96709 8.04038 9.26553L9.15003 10.4185L12.4596 6.9799C12.7469 6.68146 13.2217 6.67239 13.5201 6.95963Z";
constexpr std::wstring_view kFluentIconApprovalsApp20RegularPath =
    L"M9.85355 1.14645C9.65829 0.951184 9.34171 0.951184 9.14645 1.14645C8.95118 1.34171 8.95118 1.65829 9.14645 1.85355L10.2929 3H9.5C5.35786 3 2 6.35786 2 10.5C2 14.6421 5.35786 18 9.5 18C13.6421 18 17 14.6421 17 10.5C17 10.2239 16.7761 10 16.5 10C16.2239 10 16 10.2239 16 10.5C16 14.0899 13.0899 17 9.5 17C5.91015 17 3 14.0899 3 10.5C3 6.91015 5.91015 4 9.5 4H10.2929L9.14645 5.14645C8.95118 5.34171 8.95118 5.65829 9.14645 5.85355C9.34171 6.04882 9.65829 6.04882 9.85355 5.85355L11.8536 3.85355C12.0488 3.65829 12.0488 3.34171 11.8536 3.14645L9.85355 1.14645ZM13.3467 7.13976C13.5457 7.33125 13.5517 7.64778 13.3602 7.84673L9.51027 11.8467C9.41601 11.9447 9.28595 12 9.15002 12C9.0141 12 8.88403 11.9447 8.78978 11.8467L7.13975 10.1324C6.94826 9.93339 6.95431 9.61687 7.15327 9.42537C7.35223 9.23388 7.66876 9.23993 7.86025 9.4389L9.15003 10.779L12.6398 7.15327C12.8313 6.95431 13.1478 6.94826 13.3467 7.13976Z";
constexpr std::wstring_view kFluentIconArrowClockwise20RegularPath =
    L"M4 10C4 6.68629 6.68629 4 10 4C11.7766 4 13.3732 4.77191 14.4723 6H12.5C12.2239 6 12 6.22386 12 6.5C12 6.77614 12.2239 7 12.5 7H15.5C15.7761 7 16 6.77614 16 6.5V3.5C16 3.22386 15.7761 3 15.5 3C15.2239 3 15 3.22386 15 3.5V5.10109C13.7299 3.80499 11.9591 3 10 3C6.13401 3 3 6.13401 3 10C3 13.866 6.13401 17 10 17C13.866 17 17 13.866 17 10C17 9.8191 16.9931 9.6397 16.9796 9.46207C16.9587 9.18673 16.7185 8.98049 16.4431 9.00144C16.1678 9.02239 15.9615 9.26258 15.9825 9.53793C15.9941 9.69034 16 9.84443 16 10C16 13.3137 13.3137 16 10 16C6.68629 16 4 13.3137 4 10Z";
constexpr std::wstring_view kFluentIconClipboard20RegularPath =
    L"M7.08535 3C7.29127 2.4174 7.84689 2 8.5 2H11.5C12.1531 2 12.7087 2.4174 12.9146 3H14.5C15.3284 3 16 3.67157 16 4.5V16.5C16 17.3284 15.3284 18 14.5 18H5.5C4.67157 18 4 17.3284 4 16.5V4.5C4 3.67157 4.67157 3 5.5 3H7.08535ZM8.5 3C8.22386 3 8 3.22386 8 3.5C8 3.77614 8.22386 4 8.5 4H11.5C11.7761 4 12 3.77614 12 3.5C12 3.22386 11.7761 3 11.5 3H8.5ZM7.08535 4H5.5C5.22386 4 5 4.22386 5 4.5V16.5C5 16.7761 5.22386 17 5.5 17H14.5C14.7761 17 15 16.7761 15 16.5V4.5C15 4.22386 14.7761 4 14.5 4H12.9146C12.7087 4.5826 12.1531 5 11.5 5H8.5C7.84689 5 7.29127 4.5826 7.08535 4Z";
constexpr std::wstring_view kFluentIconCloud20RegularPath =
    L"M10 4C12.8166 4 14.4145 5.92329 14.6469 8.24599L14.7179 8.24599C16.5306 8.24599 18 9.75792 18 11.623C18 13.4881 16.5306 15 14.7179 15H5.28205C3.46942 15 2 13.4881 2 11.623C2 9.82009 3.3731 8.34717 5.10199 8.25098L5.35314 8.24599C5.58687 5.90802 7.18335 4 10 4ZM10 5C7.88606 5 6.55108 6.31588 6.34818 8.34547C6.29707 8.85669 5.86688 9.24601 5.3531 9.24599L5.28207 9.24599C4.02819 9.24599 3 10.3039 3 11.623C3 12.9421 4.02819 14 5.28205 14H14.7179C15.9718 14 17 12.9421 17 11.623C17 10.3039 15.9718 9.24599 14.718 9.24599L14.6469 9.24599C14.1332 9.24601 13.703 8.85673 13.6518 8.34554C13.4497 6.32493 12.1085 5 10 5Z";
constexpr std::wstring_view kFluentIconCloudSync20RegularPath =
    L"M10 2C12.8166 2 14.4145 3.92329 14.6469 6.24599L14.7179 6.24599C16.5306 6.24599 18 7.75792 18 9.62299C18 9.71829 17.9962 9.81267 17.9886 9.90598C17.6666 9.50435 17.2919 9.14686 16.8748 8.84404C16.5644 7.91106 15.7121 7.24599 14.718 7.24599L14.6469 7.24599C14.1332 7.24601 13.703 6.85673 13.6518 6.34554C13.4497 4.32493 12.1085 3 10 3C7.88606 3 6.55108 4.31588 6.34818 6.34547C6.29707 6.85669 5.86688 7.24601 5.3531 7.24599L5.28207 7.24599C4.02819 7.24599 3 8.30392 3 9.62299C3 10.9421 4.02819 12 5.28205 12H7.94761C7.86058 12.323 7.80097 12.6572 7.77144 13H5.28205C3.46942 13 2 11.4881 2 9.62299C2 7.82009 3.3731 6.34717 5.10199 6.25098L5.35314 6.24599C5.58687 3.90802 7.18335 2 10 2ZM9 13.5C9 15.9853 11.0147 18 13.5 18C15.9853 18 18 15.9853 18 13.5C18 11.0147 15.9853 9 13.5 9C11.0147 9 9 11.0147 9 13.5ZM15.5 10.5C15.7761 10.5 16 10.7239 16 11V12.5C16 12.7761 15.7761 13 15.5 13H14C13.7239 13 13.5 12.7761 13.5 12.5C13.5 12.2239 13.7239 12 14 12H14.4682C14.4179 11.9722 14.3663 11.9464 14.3135 11.923C14.0682 11.8137 13.8034 11.755 13.535 11.7503C13.2665 11.7456 12.9998 11.795 12.7508 11.8956C12.5018 11.9962 12.2756 12.1459 12.0857 12.3358C11.8905 12.531 11.5739 12.531 11.3786 12.3358C11.1834 12.1405 11.1834 11.8239 11.3786 11.6287C11.6635 11.3439 12.0027 11.1193 12.3762 10.9684C12.7497 10.8175 13.1497 10.7434 13.5524 10.7505C13.9552 10.7575 14.3524 10.8456 14.7203 11.0094C14.8162 11.0521 14.9095 11.0997 15 11.1519V11C15 10.7239 15.2239 10.5 15.5 10.5ZM14.6238 16.0316C14.2503 16.1825 13.8503 16.2566 13.4476 16.2495C13.0448 16.2425 12.6476 16.1544 12.2797 15.9906C12.1838 15.9479 12.0905 15.9003 12 15.8481V16C12 16.2761 11.7761 16.5 11.5 16.5C11.2239 16.5 11 16.2761 11 16V14.5C11 14.2239 11.2239 14 11.5 14H13C13.2761 14 13.5 14.2239 13.5 14.5C13.5 14.7761 13.2761 15 13 15H12.5318C12.5821 15.0278 12.6337 15.0536 12.6865 15.077C12.9318 15.1863 13.1966 15.245 13.465 15.2497C13.7335 15.2544 14.0002 15.205 14.2492 15.1044C14.4982 15.0038 14.7244 14.8541 14.9143 14.6642C15.1095 14.469 15.4261 14.469 15.6214 14.6642C15.8166 14.8595 15.8166 15.1761 15.6214 15.3713C15.3365 15.6561 14.9973 15.8807 14.6238 16.0316Z";
constexpr std::wstring_view kFluentIconArrowUpload20RegularPath =
    L"M15 3.00195C15.2761 3.00195 15.5 2.7781 15.5 2.50195C15.5 2.25649 15.3231 2.05235 15.0899 2.01001L15 2.00195H4C3.72386 2.00195 3.5 2.22581 3.5 2.50195C3.5 2.74741 3.67688 2.95156 3.91012 2.9939L4 3.00195H15ZM9.50014 17.9997C9.7456 17.9997 9.9497 17.8227 9.99197 17.5895L10 17.4996L9.996 5.70574L13.6414 9.35408C13.8148 9.5278 14.0842 9.54734 14.2792 9.41252L14.3485 9.35473C14.5222 9.18132 14.5418 8.91192 14.407 8.71692L14.3492 8.64762L9.85745 4.14762C9.78495 4.07499 9.69568 4.02931 9.60207 4.01059L9.49608 4.00085C9.33511 4.00085 9.19192 4.07697 9.10051 4.19517L4.64386 8.64704C4.44846 8.84217 4.44823 9.15875 4.64336 9.35415C4.8168 9.52784 5.08621 9.54732 5.28117 9.41246L5.35046 9.35465L8.996 5.71374L9 17.4999C9.00008 17.776 9.224 17.9997 9.50014 17.9997Z";
constexpr std::wstring_view kFluentIconArrowDownload20RegularPath =
    L"M15.5 16.9997C15.7761 16.9997 16 17.2236 16 17.4997C16 17.7452 15.8231 17.9494 15.5899 17.9917L15.5 17.9997H4.5C4.22386 17.9997 4 17.7759 4 17.4997C4 17.2543 4.17688 17.0501 4.41012 17.0078L4.5 16.9997H15.5ZM10.0001 2.00195C10.2456 2.00195 10.4497 2.17896 10.492 2.41222L10.5 2.5021L10.496 14.296L14.1414 10.6476C14.3148 10.4739 14.5842 10.4544 14.7792 10.5892L14.8485 10.647C15.0222 10.8204 15.0418 11.0898 14.907 11.2848L14.8492 11.3541L10.3574 15.8541C10.285 15.9267 10.1957 15.9724 10.1021 15.9911L9.99608 16.0008C9.83511 16.0008 9.69192 15.9247 9.60051 15.8065L5.14386 11.3547C4.94846 11.1595 4.94823 10.8429 5.14336 10.6475C5.3168 10.4739 5.58621 10.4544 5.78117 10.5892L5.85046 10.647L9.496 14.288L9.5 2.50181C9.50008 2.22567 9.724 2.00195 10.0001 2.00195Z";
constexpr std::wstring_view kFluentIconArchive20RegularPath =
    L"M8.5 10C8.22386 10 8 10.2239 8 10.5C8 10.7761 8.22386 11 8.5 11H11.5C11.7761 11 12 10.7761 12 10.5C12 10.2239 11.7761 10 11.5 10H8.5ZM2 4.75C2 3.7835 2.7835 3 3.75 3H16.25C17.2165 3 18 3.7835 18 4.75V6.25C18 6.9481 17.5912 7.55073 17 7.83159V14C17 15.6569 15.6569 17 14 17H6C4.34315 17 3 15.6569 3 14V7.83159C2.40876 7.55073 2 6.9481 2 6.25V4.75ZM3.75 4C3.33579 4 3 4.33579 3 4.75V6.25C3 6.66421 3.33579 7 3.75 7H16.25C16.6642 7 17 6.66421 17 6.25V4.75C17 4.33579 16.6642 4 16.25 4H3.75ZM4 8V14C4 15.1046 4.89543 16 6 16H14C15.1046 16 16 15.1046 16 14V8H4Z";
constexpr std::wstring_view kFluentIconHistory20RegularPath =
    L"M10 4C13.3137 4 16 6.68629 16 10C16 13.3137 13.3137 16 10 16C6.68629 16 4 13.3137 4 10C4 9.84443 4.00591 9.69034 4.0175 9.53793C4.03845 9.26258 3.83222 9.02239 3.55687 9.00144C3.28152 8.98049 3.04133 9.18673 3.02038 9.46207C3.00687 9.6397 3 9.8191 3 10C3 13.866 6.13401 17 10 17C13.866 17 17 13.866 17 10C17 6.13401 13.866 3 10 3C8.04094 3 6.27012 3.80499 5 5.10109V3.5C5 3.22386 4.77614 3 4.5 3C4.22386 3 4 3.22386 4 3.5V6.5C4 6.77614 4.22386 7 4.5 7H7.5C7.77614 7 8 6.77614 8 6.5C8 6.22386 7.77614 6 7.5 6H5.52772C6.62683 4.77191 8.2234 4 10 4ZM10 6.5C10 6.22386 9.77614 6 9.5 6C9.22386 6 9 6.22386 9 6.5V10.5C9 10.7761 9.22386 11 9.5 11H12.5C12.7761 11 13 10.7761 13 10.5C13 10.2239 12.7761 10 12.5 10H10V6.5Z";
constexpr std::wstring_view kFluentIconCalendarClock20RegularPath =
    L"M17 5.5C17 4.11929 15.8807 3 14.5 3H5.5C4.11929 3 3 4.11929 3 5.5V14.5C3 15.8807 4.11929 17 5.5 17H9.59971C9.43777 16.6832 9.30564 16.3486 9.20703 16H5.5C4.67157 16 4 15.3284 4 14.5V7H16V9.20703C16.3486 9.30564 16.6832 9.43777 17 9.59971V5.5ZM5.5 4H14.5C15.3284 4 16 4.67157 16 5.5V6H4V5.5C4 4.67157 4.67157 4 5.5 4ZM14.5 19C16.9853 19 19 16.9853 19 14.5C19 12.0147 16.9853 10 14.5 10C12.0147 10 10 12.0147 10 14.5C10 16.9853 12.0147 19 14.5 19ZM14 12.5C14 12.2239 14.2239 12 14.5 12C14.7761 12 15 12.2239 15 12.5V14H16C16.2761 14 16.5 14.2239 16.5 14.5C16.5 14.7761 16.2761 15 16 15H14.5C14.2239 15 14 14.7761 14 14.5V12.5Z";
constexpr std::wstring_view kFluentIconBookDatabase20RegularPath =
    L"M6 5C6 4.44772 6.44772 4 7 4H13C13.5523 4 14 4.44772 14 5V6C14 6.55228 13.5523 7 13 7H7C6.44772 7 6 6.55228 6 6V5ZM7 5V6H13V5H7ZM4 4V16C4 17.1046 4.89543 18 6 18H9.49987C9.2868 17.7032 9.12776 17.3694 9.05104 17H6C5.44772 17 5 16.5523 5 16H9V15H5V4C5 3.44772 5.44772 3 6 3H14C14.5523 3 15 3.44772 15 4V8.01358C15.343 8.03231 15.6774 8.07014 16 8.12555V4C16 2.89543 15.1046 2 14 2H6C4.89543 2 4 2.89543 4 4ZM16.9998 9.42091C16.6567 9.29333 16.2803 9.19127 15.8793 9.11962C15.4446 9.04195 14.9811 9 14.5 9C12.0147 9 10 10.1193 10 11.5C10 12.8807 12.0147 14 14.5 14C16.9853 14 19 12.8807 19 11.5C19 10.6332 18.2059 9.86938 16.9998 9.42091ZM18.1676 14.1419C17.1785 14.6914 15.8799 15 14.5 15C13.1201 15 11.8215 14.6914 10.8324 14.1419C10.54 13.9794 10.2549 13.7831 10 13.5538V16.4994C10 17.8802 12.0147 18.9994 14.5 18.9994C16.9853 18.9994 19 17.8802 19 16.4994C19 15.9071 19.0003 15.3321 19.0007 14.6744C19.0009 14.3282 19.0011 13.9591 19.0013 13.5526C18.7461 13.7824 18.4605 13.9792 18.1676 14.1419Z";
constexpr std::wstring_view kFluentIconBookDismiss20RegularPath =
    L"M6 5C6 4.44772 6.44772 4 7 4H13C13.5523 4 14 4.44772 14 5V6C14 6.55228 13.5523 7 13 7H7C6.44772 7 6 6.55228 6 6V5ZM7 5V6H13V5H7ZM4 4V16C4 17.1046 4.89543 18 6 18H10.2572C10.0035 17.6929 9.78261 17.3578 9.59971 17H6C5.44772 17 5 16.5523 5 16H9.20703C9.11588 15.6777 9.05337 15.3434 9.02242 15H5V4C5 3.44772 5.44772 3 6 3H14C14.5523 3 15 3.44772 15 4V9.02242C15.3434 9.05337 15.6777 9.11588 16 9.20703V4C16 2.89543 15.1046 2 14 2H6C4.89543 2 4 2.89543 4 4ZM19 14.5C19 16.9853 16.9853 19 14.5 19C12.0147 19 10 16.9853 10 14.5C10 12.0147 12.0147 10 14.5 10C16.9853 10 19 12.0147 19 14.5ZM16.3536 13.3536C16.5488 13.1583 16.5488 12.8417 16.3536 12.6464C16.1583 12.4512 15.8417 12.4512 15.6464 12.6464L14.5 13.7929L13.3536 12.6464C13.1583 12.4512 12.8417 12.4512 12.6464 12.6464C12.4512 12.8417 12.4512 13.1583 12.6464 13.3536L13.7929 14.5L12.6464 15.6464C12.4512 15.8417 12.4512 16.1583 12.6464 16.3536C12.8417 16.5488 13.1583 16.5488 13.3536 16.3536L14.5 15.2071L15.6464 16.3536C15.8417 16.5488 16.1583 16.5488 16.3536 16.3536C16.5488 16.1583 16.5488 15.8417 16.3536 15.6464L15.2071 14.5L16.3536 13.3536Z";
constexpr std::wstring_view kFluentIconCalendarEmpty20RegularPath =
    L"M14.5 3C15.8807 3 17 4.11929 17 5.5V14.5C17 15.8807 15.8807 17 14.5 17H5.5C4.11929 17 3 15.8807 3 14.5V5.5C3 4.11929 4.11929 3 5.5 3H14.5ZM16 7H4V14.5C4 15.3284 4.67157 16 5.5 16H14.5C15.3284 16 16 15.3284 16 14.5V7ZM14.5 4H5.5C4.67157 4 4 4.67157 4 5.5V6H16V5.5C16 4.67157 15.3284 4 14.5 4Z";
constexpr std::wstring_view kFluentIconCalendarEdit20RegularPath =
    L"M16.9985 5.49972C16.9985 4.11916 15.8793 3 14.4987 3H5.49972C4.11916 3 3 4.11916 3 5.49972V14.4987C3 15.8793 4.11916 16.9985 5.49972 16.9985H9.21979L9.43092 16.1539C9.44398 16.1017 9.45833 16.0499 9.47395 15.9986H5.49972C4.67139 15.9986 3.99989 15.3271 3.99989 14.4987V6.99956H15.9986V9.23094C16.3179 9.0941 16.6571 9.0179 16.9985 9.00234V5.49972ZM5.49972 3.99989H14.4987C15.3271 3.99989 15.9986 4.67139 15.9986 5.49972V5.99967H3.99989V5.49972C3.99989 4.67139 4.67139 3.99989 5.49972 3.99989ZM15.8078 10.5469L10.9789 15.3758C10.6973 15.6574 10.4976 16.0101 10.401 16.3964L10.0266 17.8942C9.86376 18.5455 10.4537 19.1355 11.1051 18.9726L12.6028 18.5982C12.9891 18.5016 13.3419 18.3019 13.6234 18.0204L18.4523 13.1915C19.1826 12.4612 19.1826 11.2772 18.4523 10.5469C17.722 9.81666 16.538 9.81666 15.8078 10.5469ZM8 10C8 10.5523 7.55228 11 7 11C6.44772 11 6 10.5523 6 10C6 9.44771 6.44772 9 7 9C7.55228 9 8 9.44771 8 10ZM7 14C7.55228 14 8 13.5523 8 13C8 12.4477 7.55228 12 7 12C6.44772 12 6 12.4477 6 13C6 13.5523 6.44772 14 7 14ZM11 10C11 10.5523 10.5523 11 10 11C9.44771 11 9 10.5523 9 10C9 9.44771 9.44771 9 10 9C10.5523 9 11 9.44771 11 10ZM10 14C10.5523 14 11 13.5523 11 13C11 12.4477 10.5523 12 10 12C9.44771 12 9 12.4477 9 13C9 13.5523 9.44771 14 10 14ZM14 10C14 10.5523 13.5523 11 13 11C12.4477 11 12 10.5523 12 10C12 9.44771 12.4477 9 13 9C13.5523 9 14 9.44771 14 10Z";
constexpr std::wstring_view kFluentIconDocument20RegularPath =
    L"M6 2C4.89543 2 4 2.89543 4 4V16C4 17.1046 4.89543 18 6 18H14C15.1046 18 16 17.1046 16 16V7.41421C16 7.01639 15.842 6.63486 15.5607 6.35355L11.6464 2.43934C11.3651 2.15804 10.9836 2 10.5858 2H6ZM5 4C5 3.44772 5.44772 3 6 3H10V6.5C10 7.32843 10.6716 8 11.5 8H15V16C15 16.5523 14.5523 17 14 17H6C5.44772 17 5 16.5523 5 16V4ZM14.7929 7H11.5C11.2239 7 11 6.77614 11 6.5V3.20711L14.7929 7Z";
constexpr std::wstring_view kFluentIconDocumentEdit20RegularPath =
    L"M11.5 8H16V7.41421C16 7.29283 15.9853 7.17296 15.9568 7.057C15.9525 7.03943 15.9479 7.02195 15.943 7.00458L15.9417 7C15.8721 6.75786 15.742 6.53488 15.5607 6.35355L11.6458 2.43867C11.4646 2.25769 11.2418 2.12781 11 2.05832C10.8665 2.01996 10.7272 2 10.5858 2H6C4.89543 2 4 2.89543 4 4V16C4 17.1046 4.89543 18 6 18H8.22126L8.20776 17.9744C8.092 17.7508 8.01983 17.5018 8.00293 17.2392C7.99787 17.1606 7.99776 17.0807 8.00293 17H6C5.44772 17 5 16.5523 5 16V4C5 3.44772 5.44772 3 6 3H10V6.5C10 7.32843 10.6716 8 11.5 8ZM11.5 7C11.2239 7 11 6.77614 11 6.5V3.20711L14.7929 7H11.5ZM14.8092 9.54776C15.5395 8.81741 16.7237 8.81741 17.454 9.54776C18.1843 10.2781 18.1843 11.4622 17.454 12.1926L12.6246 17.022C12.343 17.3036 11.9902 17.5033 11.6039 17.5999L10.106 17.9744C9.45456 18.1372 8.86451 17.5472 9.02737 16.8958L9.40184 15.3979C9.49842 15.0116 9.69818 14.6588 9.97975 14.3772L14.8092 9.54776Z";
constexpr std::wstring_view kFluentIconDocumentTable20RegularPath =
    L"M6 10.5C6 9.67157 6.67157 9 7.5 9H12.5C13.3284 9 14 9.67157 14 10.5V14.5C14 15.3284 13.3284 16 12.5 16H7.5C6.67157 16 6 15.3284 6 14.5V10.5ZM8 15V13H7V14.5C7 14.7761 7.22386 15 7.5 15H8ZM9 12H13V10.5C13 10.2239 12.7761 10 12.5 10H9V12ZM9 15H12.5C12.7761 15 13 14.7761 13 14.5V13H9V15ZM7.5 10C7.22386 10 7 10.2239 7 10.5V12H8V10H7.5ZM6 2C4.89543 2 4 2.89543 4 4V16C4 17.1046 4.89543 18 6 18H14C15.1046 18 16 17.1046 16 16V7.41421C16 7.01639 15.842 6.63486 15.5607 6.35355L11.6464 2.43934C11.3651 2.15804 10.9836 2 10.5858 2H6ZM5 4C5 3.44772 5.44772 3 6 3H10V6.5C10 7.32843 10.6716 8 11.5 8H15V16C15 16.5523 14.5523 17 14 17H6C5.44772 17 5 16.5523 5 16V4ZM14.7929 7H11.5C11.2239 7 11 6.77614 11 6.5V3.20711L14.7929 7Z";
constexpr std::wstring_view kFluentIconDocumentDismiss20RegularPath =
    L"M6 2C4.89543 2 4 2.89543 4 4V9.20703C4.32228 9.11588 4.65659 9.05337 5 9.02242V4C5 3.44772 5.44772 3 6 3H10V6.5C10 7.32843 10.6716 8 11.5 8H15V16C15 16.5523 14.5523 17 14 17H10.4003C10.2174 17.3578 9.99647 17.6929 9.74284 18H14C15.1046 18 16 17.1046 16 16V7.41421C16 7.01639 15.842 6.63486 15.5607 6.35355L11.6464 2.43934C11.3651 2.15804 10.9836 2 10.5858 2H6ZM14.7929 7H11.5C11.2239 7 11 6.77614 11 6.5V3.20711L14.7929 7ZM8.68198 17.682C10.4393 15.9246 10.4393 13.0754 8.68198 11.318C6.92462 9.56066 4.07538 9.56066 2.31802 11.318C0.56066 13.0754 0.56066 15.9246 2.31802 17.682C4.07538 19.4393 6.92462 19.4393 8.68198 17.682ZM3.73202 12.7324C3.92729 12.5371 4.24387 12.5371 4.43913 12.7324L5.49969 13.7929L6.56062 12.732C6.75588 12.5367 7.07246 12.5367 7.26773 12.732C7.46299 12.9273 7.46299 13.2439 7.26773 13.4391L6.20679 14.5L7.26756 15.5608C7.46282 15.7561 7.46282 16.0727 7.26756 16.2679C7.0723 16.4632 6.75571 16.4632 6.56045 16.2679L5.49969 15.2072L4.4393 16.2675C4.24404 16.4628 3.92745 16.4628 3.73219 16.2675C3.53693 16.0723 3.53693 15.7557 3.73219 15.5604L4.79258 14.5L3.73202 13.4395C3.53676 13.2442 3.53676 12.9276 3.73202 12.7324Z";
constexpr std::wstring_view kFluentIconArrowImport20RegularPath =
    L"M17.5 4C17.7761 4 18 4.22386 18 4.5V15.5C18 15.7761 17.7761 16 17.5 16C17.2239 16 17 15.7761 17 15.5V4.5C17 4.22386 17.2239 4 17.5 4ZM2 10C2 9.72386 2.22386 9.5 2.5 9.5H13.2929L10.1464 6.35355C9.95118 6.15829 9.95118 5.84171 10.1464 5.64645C10.3417 5.45118 10.6583 5.45118 10.8536 5.64645L14.8536 9.64645C14.9015 9.69439 14.9377 9.74964 14.9621 9.80861C14.9861 9.86669 14.9996 9.9303 15 9.997L15 10L15 10.003C14.9992 10.13 14.9504 10.2567 14.8536 10.3536L10.8536 14.3536C10.6583 14.5488 10.3417 14.5488 10.1464 14.3536C9.95118 14.1583 9.95118 13.8417 10.1464 13.6464L13.2929 10.5H2.5C2.22386 10.5 2 10.2761 2 10Z";
constexpr std::wstring_view kFluentIconDatabase20RegularPath =
    L"M4 5C4 3.993 4.87513 3.24472 5.90401 2.77705C6.97802 2.28886 8.42664 2 10 2C11.5734 2 13.022 2.28886 14.096 2.77705C15.1249 3.24472 16 3.993 16 5V15C16 16.007 15.1249 16.7553 14.096 17.2229C13.022 17.7111 11.5734 18 10 18C8.42664 18 6.97802 17.7111 5.90401 17.2229C4.87513 16.7553 4 16.007 4 15V5ZM5 5C5 5.37372 5.35608 5.87543 6.31781 6.31258C7.23441 6.72922 8.53579 7 10 7C11.4642 7 12.7656 6.72922 13.6822 6.31258C14.6439 5.87543 15 5.37372 15 5C15 4.62628 14.6439 4.12457 13.6822 3.68742C12.7656 3.27078 11.4642 3 10 3C8.53579 3 7.23441 3.27078 6.31781 3.68742C5.35608 4.12457 5 4.62628 5 5ZM15 6.69813C14.729 6.90046 14.4201 7.07563 14.096 7.22295C13.022 7.71114 11.5734 8 10 8C8.42664 8 6.97802 7.71114 5.90401 7.22295C5.5799 7.07563 5.27105 6.90046 5 6.69813V15C5 15.3737 5.35608 15.8754 6.31781 16.3126C7.23441 16.7292 8.53579 17 10 17C11.4642 17 12.7656 16.7292 13.6822 16.3126C14.6439 15.8754 15 15.3737 15 15V6.69813Z";
constexpr std::wstring_view kFluentIconLibrary20RegularPath =
    L"M2 3.49788C2 2.67062 2.67135 2 3.49951 2H4.49918C5.32733 2 5.99869 2.67062 5.99869 3.49788V16.4795C5.99869 17.3068 5.32733 17.9774 4.49918 17.9774H3.49951C2.67135 17.9774 2 17.3068 2 16.4795V3.49788ZM3.49951 2.99859C3.22346 2.99859 2.99967 3.22213 2.99967 3.49788V16.4795C2.99967 16.7552 3.22346 16.9788 3.49951 16.9788H4.49918C4.77523 16.9788 4.99901 16.7552 4.99901 16.4795V3.49788C4.99901 3.22213 4.77523 2.99859 4.49918 2.99859H3.49951ZM6.99836 3.49788C6.99836 2.67062 7.66971 2 8.49786 2H9.49754C10.3257 2 10.997 2.67062 10.997 3.49788V16.4795C10.997 17.3068 10.3257 17.9774 9.49754 17.9774H8.49786C7.66971 17.9774 6.99836 17.3068 6.99836 16.4795V3.49788ZM8.49786 2.99859C8.22181 2.99859 7.99803 3.22213 7.99803 3.49788V16.4795C7.99803 16.7552 8.22181 16.9788 8.49786 16.9788H9.49754C9.77359 16.9788 9.99737 16.7552 9.99737 16.4795V3.49788C9.99737 3.22213 9.77359 2.99859 9.49754 2.99859H8.49786ZM15.7179 6.15675C15.5259 5.32176 14.6733 4.81743 13.848 5.05077L13.1029 5.26146C12.3477 5.47502 11.8851 6.23427 12.0422 7.00249L14.046 16.8015C14.2174 17.6394 15.0551 18.1642 15.8848 17.9534L16.8698 17.7031C17.6592 17.5025 18.144 16.7091 17.9616 15.9162L15.7179 6.15675ZM14.1203 6.0116C14.3954 5.93382 14.6796 6.10193 14.7436 6.38026L16.9873 16.1397C17.0481 16.404 16.8865 16.6684 16.6234 16.7353L15.6384 16.9856C15.3618 17.0559 15.0826 16.8809 15.0255 16.6016L13.0216 6.80264C12.9693 6.54656 13.1234 6.29348 13.3752 6.22229L14.1203 6.0116Z";
constexpr std::wstring_view kFluentIconLibrary20FilledPath =
    L"M3.49951 2C2.67135 2 2 2.67062 2 3.49786V16.4793C2 17.3066 2.67135 17.9772 3.49951 17.9772H4.49918C5.32733 17.9772 5.99869 17.3066 5.99869 16.4793V3.49786C5.99869 2.67062 5.32733 2 4.49918 2H3.49951ZM8.49786 2C7.66971 2 6.99836 2.67062 6.99836 3.49786V16.4793C6.99836 17.3066 7.66971 17.9772 8.49786 17.9772H9.49754C10.3257 17.9772 10.997 17.3066 10.997 16.4793V3.49786C10.997 2.67062 10.3257 2 9.49754 2H8.49786ZM15.7179 6.15689C15.5259 5.32191 14.6733 4.81758 13.848 5.05092L13.1029 5.26161C12.3477 5.47516 11.8851 6.2344 12.0422 7.00262L14.046 16.8015C14.2174 17.6394 15.0551 18.1642 15.8848 17.9534L16.8698 17.7031C17.6592 17.5025 18.144 16.7092 17.9616 15.9162L15.7179 6.15689Z";
constexpr std::wstring_view kFluentIconEditSettings20FilledPath =
    L"M12.9203 2.87317C14.1027 1.69077 16.0271 1.71505 17.1794 2.92689C18.2913 4.09631 18.2681 5.93899 17.1271 7.08003L16.4581 7.74902L12.2512 3.54217L12.9203 2.87317ZM11.5441 4.24927L3.54545 12.2475C3.21763 12.5754 2.99008 12.9899 2.88953 13.4424L2.01191 17.3923C1.97483 17.5592 2.02559 17.7335 2.14649 17.8544C2.26739 17.9753 2.44166 18.026 2.60855 17.9889L6.53494 17.1157C7.00237 17.0118 7.43048 16.7767 7.76907 16.4381L8.20707 16.0001C8.07218 15.5233 8 15.0201 8 14.5C8 11.4624 10.4624 9 13.5 9C14.02 9 14.5232 9.07217 15.0001 9.20705L15.751 8.45613L11.5441 4.24927ZM11.0667 11.4429C11.37 12.5241 10.724 13.643 9.63604 13.9209L9.175 14.0387C9.16002 14.1906 9.15234 14.3448 9.15234 14.5008C9.15234 14.6885 9.16344 14.8735 9.185 15.0551L9.53456 15.1377C10.654 15.4024 11.32 16.5545 10.9906 17.6567L10.8643 18.0795C11.1215 18.2827 11.4012 18.4569 11.699 18.5974L12.0239 18.2533C12.8138 17.417 14.1445 17.4177 14.9335 18.2548L15.2708 18.6128C15.5632 18.4778 15.8386 18.3105 16.0927 18.1151L15.9365 17.5585C15.6332 16.4773 16.2792 15.3584 17.3672 15.0805L17.8277 14.9629C17.8427 14.811 17.8504 14.6568 17.8504 14.5008C17.8504 14.313 17.8393 14.128 17.8177 13.9462L17.4687 13.8637C16.3492 13.599 15.6832 12.4469 16.0126 11.3447L16.1388 10.9225C15.8815 10.7192 15.6018 10.5449 15.304 10.4044L14.9793 10.7482C14.1895 11.5845 12.8587 11.5837 12.0698 10.7466L11.7324 10.3887C11.44 10.5236 11.1646 10.6909 10.9105 10.8862L11.0667 11.4429ZM13.5014 15.5008C12.9491 15.5008 12.5014 15.0531 12.5014 14.5008C12.5014 13.9485 12.9491 13.5008 13.5014 13.5008C14.0536 13.5008 14.5014 13.9485 14.5014 14.5008C14.5014 15.0531 14.0536 15.5008 13.5014 15.5008Z";
constexpr std::wstring_view kFluentIconArrowClockwise20FilledPath =
    L"M4 10C4 6.68629 6.68629 4 10 4C11.5213 4 12.9107 4.56592 13.9689 5.5H12.75C12.3358 5.5 12 5.83579 12 6.25C12 6.66421 12.3358 7 12.75 7H15.75C16.1642 7 16.5 6.66421 16.5 6.25V3.25C16.5 2.83579 16.1642 2.5 15.75 2.5C15.3358 2.5 15 2.83579 15 3.25V4.40987C13.6736 3.22274 11.9213 2.5 10 2.5C5.85786 2.5 2.5 5.85786 2.5 10C2.5 14.1421 5.85786 17.5 10 17.5C14.1421 17.5 17.5 14.1421 17.5 10C17.5 9.90715 17.4983 9.81467 17.495 9.72258C17.4799 9.30864 17.1321 8.9853 16.7181 9.00038C16.3042 9.01546 15.9809 9.36324 15.9959 9.77718C15.9986 9.85109 16 9.92537 16 10C16 13.3137 13.3137 16 10 16C6.68629 16 4 13.3137 4 10Z";
constexpr std::wstring_view kGitHubMark24Path =
    L"M12 .5C5.648 .5 .5 5.648 .5 12C.5 17.086 3.792 21.391 8.36 22.916C8.935 23.021 9.145 22.666 9.145 22.361C9.145 22.086 9.135 21.359 9.129 20.393C5.932 21.088 5.256 18.852 5.256 18.852C4.733 17.523 3.979 17.168 3.979 17.168C2.935 16.454 4.058 16.469 4.058 16.469C5.212 16.551 5.819 17.654 5.819 17.654C6.845 19.411 8.511 18.904 9.168 18.61C9.272 17.867 9.57 17.36 9.899 17.072C7.347 16.782 4.665 15.796 4.665 11.392C4.665 10.136 5.114 9.109 5.85 8.306C5.731 8.015 5.337 6.845 5.963 5.262C5.963 5.262 6.929 4.953 9.126 6.44C10.043 6.185 11.026 6.058 12 6.053C12.974 6.058 13.958 6.185 14.876 6.44C17.071 4.953 18.035 5.262 18.035 5.262C18.663 6.845 18.269 8.015 18.15 8.306C18.888 9.109 19.334 10.136 19.334 11.392C19.334 15.807 16.648 16.779 14.088 17.064C14.5 17.419 14.868 18.12 14.868 19.193C14.868 20.73 14.854 21.971 14.854 22.361C14.854 22.669 15.061 23.027 15.646 22.914C20.21 21.386 23.5 17.084 23.5 12C23.5 5.648 18.352 .5 12 .5Z";

Grid ShapeStatusIcon(bool full_shape) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);

  root.Children().Append(full_shape
                             ? PathShape(kFluentCircle20FilledPath, kSettingIconVisualSize, 20, 0.86)
                             : PathShape(kFluentWeatherMoon24Path,
                                         kSettingIconVisualSize,
                                         24,
                                         0.90,
                                         -0.040,
                                         0.005,
                                         0.98));
  return root;
}

Grid PunctuationStatusIcon(bool chinese_punctuation) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);

  auto icon = ScaledIconCanvas(kSettingIconVisualSize, 52, 0.96);
  if (chinese_punctuation) {
    auto ring_outer =
        EllipseShape(20.2 - 8.0, 10.2 - 5.0, 14.2, 14.2, SettingsIconBrush());
    auto ring_inner = EllipseShape(23.5 - 8.0,
                                   13.6 - 5.0,
                                   7.7,
                                   7.7,
                                   Brush(CurrentSettingsPalette().icon_backdrop));
    auto comma = RawPathShape(L"M46.3 23.9C42.5 23.9 40.9 26.4 40.9 30.6C40.9 34.7 44.5 36.8 48.2 36C47.5 39 44.6 44 39.6 49H42C49.6 43.5 54 37.7 54 30.9C54 26.4 50.4 24 46.3 23.9Z");
    Canvas::SetLeft(comma, -8.0);
    Canvas::SetTop(comma, -5.0);
    icon.Children().Append(ring_outer);
    icon.Children().Append(ring_inner);
    icon.Children().Append(comma);
  } else {
    auto dot = EllipseShape(20.0 - 8.0, 10.5 - 5.0, 14.0, 13.5, SettingsIconBrush());
    auto comma = RawPathShape(L"M43 26.8H50.9L48.6 34L45.2 45.6H39.2L42.4 26.8H43Z");
    Canvas::SetLeft(comma, -8.0);
    Canvas::SetTop(comma, -5.0);
    icon.Children().Append(dot);
    icon.Children().Append(comma);
  }
  root.Children().Append(icon);
  return root;
}

Grid EmojiStatusIcon() {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(kFluentEmoji24Path, kSettingIconVisualSize, 24, 1.02));
  return root;
}

Grid FluentPathIcon(std::wstring_view path, double scale = 1.0, double view_box_size = 20.0) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(path, kSettingIconVisualSize, view_box_size, scale));
  return root;
}

Grid FluentButtonPathIcon(std::wstring_view path, double scale = 0.76) {
  Grid root;
  root.Width(kInlineButtonIconHostSize);
  root.Height(kInlineButtonIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(path, kInlineButtonIconHostSize, 20, scale));
  return root;
}

Grid ActionButtonPathIcon(std::wstring_view path, double scale = 0.88, double view_box_size = 20.0) {
  Grid root;
  root.Width(kActionButtonIconHostSize);
  root.Height(kActionButtonIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(path, kActionButtonIconHostSize, view_box_size, scale));
  return root;
}

Grid PinyinAssistPathIcon(std::wstring_view path, double scale = 0.82) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(path, kSettingIconVisualSize, 20, scale));
  return root;
}

Grid ChevronStatusIcon(std::wstring_view path, double scale = 0.98) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(path, kSettingIconVisualSize, 20, scale));
  return root;
}

Grid TriangleStatusIcon(bool right) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(right ? kFluentTriangleRight12FilledPath
                                         : kFluentTriangleLeft12FilledPath,
                                   kSettingIconVisualSize,
                                   12,
                                   0.54,
                                   right ? -0.03 : 0.03,
                                   0.035,
                                   0.64));
  return root;
}

std::wstring DefaultInputModeIconText() {
  return ReadStringSetting(L"default_input_mode", L"zh") == L"en" ? L"英" : L"中";
}

std::wstring FirstIconText(std::wstring_view value) {
  if (value.empty()) {
    return L"-";
  }
  return std::wstring(value.substr(0, 1));
}

std::wstring BoolIconText(bool value) {
  return value ? L"开" : L"关";
}

std::wstring ChoiceIconText(const std::vector<std::pair<std::wstring, std::wstring>>& choices,
                            std::wstring_view value) {
  for (const auto& [label, choice_value] : choices) {
    if (choice_value == value) {
      return FirstIconText(label);
    }
  }
  return choices.empty() ? L"-" : FirstIconText(choices.front().first);
}

std::wstring IntChoiceIconText(const std::vector<std::wstring>& labels, int value) {
  if (value >= 0 && value < static_cast<int>(labels.size())) {
    return FirstIconText(labels[static_cast<size_t>(value)]);
  }
  return labels.empty() ? L"-" : FirstIconText(labels.front());
}

std::wstring DefaultCharsetIconText() {
  return ReadStringSetting(L"default_charset", L"simplified") == L"traditional" ? L"繁" : L"简";
}

Grid CandidateLayoutIcon(std::wstring_view layout) {
  Grid root;
  root.Width(kSettingIconHostSize);
  root.Height(kSettingIconHostSize);
  root.HorizontalAlignment(HorizontalAlignment::Center);
  root.VerticalAlignment(VerticalAlignment::Center);
  root.Children().Append(PathShape(layout == L"vertical"
                                       ? kFluentReOrderDotsVertical20RegularPath
                                       : kFluentReOrderDotsHorizontal20RegularPath,
                                   kSettingIconVisualSize,
                                   20,
                                   1.08));
  return root;
}

Grid AutoPinyinCorrectionIcon(bool enabled) {
  return PinyinAssistPathIcon(enabled ? kFluentIconRename20RegularPath
                                      : kFluentIconRectangleLandscape20RegularPath,
                              0.94);
}

Grid SuperAbbrevIcon(bool enabled) {
  return PinyinAssistPathIcon(enabled ? kFluentIconFlash20RegularPath
                                      : kFluentIconFlashOff20RegularPath,
                              0.94);
}

Grid SmartFuzzyPinyinIcon(bool enabled) {
  return PinyinAssistPathIcon(enabled ? kFluentIconLightbulbFilament20RegularPath
                                      : kFluentIconLightbulb20RegularPath,
                              0.94);
}

Grid FuzzyPinyinRulesStateIcon(bool enabled) {
  return PinyinAssistPathIcon(enabled ? kFluentIconEdit20RegularPath
                                      : kFluentIconEditOff20RegularPath,
                              0.94);
}

Grid ImportedLexiconsStateIcon(bool enabled) {
  return FluentPathIcon(enabled ? kFluentIconLibrary20RegularPath
                                : kFluentIconLibrary20FilledPath,
                        0.92);
}

Grid UserLexiconStateIcon(bool) {
  return FluentPathIcon(kFluentIconCalendarEmpty20RegularPath, 0.96);
}

Grid CustomPhrasesStateIcon(bool enabled) {
  return FluentPathIcon(enabled ? kFluentIconDocument20RegularPath
                                : kFluentIconDocumentDismiss20RegularPath,
                        0.94);
}

Grid CandidateFontSizeIcon() {
  return FluentPathIcon(kFluentIconTextFont20RegularPath, 0.94);
}

Grid ToolbarVisibleIcon(bool enabled) {
  return FluentPathIcon(enabled ? kFluentIconToggleRight20RegularPath
                                : kFluentIconToggleLeft20RegularPath,
                        1.06);
}

Grid StatusTipEnabledIcon(bool enabled) {
  return FluentPathIcon(enabled ? kFluentIconComment20RegularPath
                                : kFluentIconCommentOff20RegularPath,
                        0.98);
}

Grid StatusTipBlacklistIcon() {
  return FluentPathIcon(kFluentIconCommentNote20RegularPath, 0.98);
}

Grid ThemeModeIcon(std::wstring_view mode) {
  if (mode == fp::kThemeModeCustom) {
    return FluentPathIcon(kFluentIconKeyboard20RegularPath, 1.02);
  }
  if (mode == fp::kThemeModeLight) {
    return FluentPathIcon(kFluentWeatherSunny20RegularPath, 1.02);
  }
  if (mode == fp::kThemeModeDark) {
    return FluentPathIcon(kFluentWeatherMoon20FilledPath, 0.98);
  }
  return FluentPathIcon(kFluentDarkTheme20RegularPath, 1.02);
}

Grid ThemePresetRowIcon() {
  return FluentPathIcon(kFluentIconKeyboard20RegularPath, 1.02);
}

TextBlock ThemePresetLabel(std::wstring_view preset) {
  return Text(fp::ThemePresetLabelText(preset), 13, FW_SEMIBOLD);
}

Grid ThemePreviewArtwork(std::wstring_view preset,
                         double width,
                         double height,
                         bool compact = false) {
  const auto preview = ThemePreview(preset);
  const double vertical_guard = compact ? 8.0 : 14.0;
  const double shape_height = compact ? 47.0 : 54.0;
  const double scale =
      std::min(width / (compact ? 118.0 : 146.0),
               std::max(1.0, height - vertical_guard) / shape_height);
  const double tile_size = std::floor((compact ? 43.0 : 50.0) * scale);
  const double circle_size = std::floor((compact ? 47.0 : 54.0) * scale);
  const double side_margin = std::max(10.0, (compact ? 14.0 : 18.0) * scale);
  Grid card;
  card.Width(width);
  card.Height(height);
  card.HorizontalAlignment(HorizontalAlignment::Center);
  card.VerticalAlignment(VerticalAlignment::Center);
  card.UseLayoutRounding(true);

  Border background;
  background.CornerRadius(Radius(compact ? 6 : 8));
  background.Background(Brush(preview.background));
  background.BorderBrush(Brush(preview.edge));
  background.BorderThickness(SettingsHairlineThickness());
  card.Children().Append(background);

  Border tile;
  tile.Width(tile_size);
  tile.Height(tile_size);
  tile.HorizontalAlignment(HorizontalAlignment::Left);
  tile.VerticalAlignment(VerticalAlignment::Center);
  tile.Margin(Thickness{side_margin, 0, 0, 0});
  tile.CornerRadius(Radius(compact ? 5 : 6));
  tile.Background(Brush(preview.tile));
  tile.BorderBrush(Brush(preview.edge));
  tile.BorderThickness(SettingsHairlineThickness());
  auto aa = Text(L"Aa", compact ? 20 : 22, FW_SEMIBOLD);
  aa.Foreground(Brush(preview.text));
  aa.HorizontalAlignment(HorizontalAlignment::Center);
  aa.VerticalAlignment(VerticalAlignment::Center);
  aa.TextWrapping(TextWrapping::NoWrap);
  tile.Child(aa);
  card.Children().Append(tile);

  Grid circle;
  circle.Width(circle_size);
  circle.Height(circle_size);
  circle.HorizontalAlignment(HorizontalAlignment::Right);
  circle.VerticalAlignment(VerticalAlignment::Center);
  circle.Margin(Thickness{0, 0, side_margin, 0});
  Shapes::Ellipse disk;
  disk.Fill(Brush(preview.circle));
  disk.Width(circle_size);
  disk.Height(circle_size);
  disk.HorizontalAlignment(HorizontalAlignment::Center);
  disk.VerticalAlignment(VerticalAlignment::Center);
  circle.Children().Append(disk);
  auto keyboard = PathShape(kFluentIconKeyboard20RegularPath,
                            compact ? 24 : 27,
                            20,
                            1.0);
  keyboard.Fill(Brush(preview.symbol));
  circle.Children().Append(keyboard);
  card.Children().Append(circle);
  return card;
}

Border Card(UIElement const& child) {
  return SettingsFrame(child,
                       CurrentSettingsPalette().card,
                       Radius(8),
                       UniformThickness(16),
                       Thickness{0, 0, 0, 10});
}

Border StatusBadge(std::wstring_view status) {
  Border badge;
  badge.CornerRadius(Radius(999));
  badge.Padding(Thickness{8, 2, 8, 3});
  badge.VerticalAlignment(VerticalAlignment::Center);
  badge.UseLayoutRounding(true);

  const auto palette = CurrentSettingsPalette();
  SolidColorBrush background = Brush(palette.button);
  SolidColorBrush foreground = Brush(palette.secondary_text);
  if (status == L"已联动") {
    background = palette.light ? Brush(224, 245, 233) : Brush(42, 72, 55);
    foreground = palette.light ? Brush(31, 112, 66) : Brush(145, 225, 175);
  } else if (status == L"部分联动") {
    background = palette.light ? Brush(247, 240, 218) : Brush(70, 64, 42);
    foreground = palette.light ? Brush(136, 101, 24) : Brush(230, 205, 145);
  } else if (status == L"入口已接通") {
    background = palette.light ? Brush(224, 241, 250) : Brush(45, 65, 82);
    foreground = palette.light ? Brush(38, 103, 138) : Brush(150, 205, 235);
  } else if (status == L"已保存") {
    background = Brush(palette.button);
    foreground = Brush(palette.secondary_text);
  } else if (status == L"占位") {
    background = palette.light ? Brush(250, 229, 229) : Brush(72, 48, 48);
    foreground = palette.light ? Brush(146, 56, 56) : Brush(230, 160, 160);
  }
  badge.Background(background);

  auto label = Text(status, 11, FW_SEMIBOLD);
  label.Foreground(foreground);
  label.TextWrapping(TextWrapping::NoWrap);
  badge.Child(label);
  return badge;
}

StackPanel PageShell(std::wstring_view title, std::wstring_view subtitle) {
  StackPanel page;
  page.Spacing(12);
  page.Padding(Thickness{32, 12, 32, 32});
  page.HorizontalAlignment(HorizontalAlignment::Stretch);
  page.Children().Append(Text(title, 28, FW_SEMIBOLD));
  auto description = Text(subtitle, 14);
  description.Foreground(SettingsSecondaryTextBrush());
  description.Margin(Thickness{0, 0, 0, 8});
  page.Children().Append(description);
  return page;
}

TextBlock SectionHeader(std::wstring_view title, bool first = false) {
  auto header = Text(title, 16, FW_SEMIBOLD);
  header.Foreground(SettingsTextBrush());
  header.Margin(first ? Thickness{2, 2, 0, 0} : Thickness{2, 16, 0, 0});
  header.TextWrapping(TextWrapping::NoWrap);
  return header;
}

Border SettingIconBackdrop(UIElement const& icon_content) {
  Border icon_backdrop;
  icon_backdrop.Width(kSettingIconBackdropSize);
  icon_backdrop.Height(kSettingIconBackdropSize);
  icon_backdrop.CornerRadius(Radius(kSettingIconBackdropRadius));
  icon_backdrop.Background(Brush(CurrentSettingsPalette().icon_backdrop));
  icon_backdrop.VerticalAlignment(VerticalAlignment::Center);
  icon_backdrop.HorizontalAlignment(HorizontalAlignment::Center);
  icon_backdrop.UseLayoutRounding(true);
  icon_backdrop.Child(IconHost(icon_content));
  return icon_backdrop;
}

Border SettingRowWithIcon(std::wstring_view title,
                          std::wstring_view subtitle,
                          UIElement const& control,
                          UIElement const& icon_content,
                          std::wstring_view status = L"",
                          double control_width = 140.0) {
  (void)status;
  Grid grid;
  grid.HorizontalAlignment(HorizontalAlignment::Stretch);
  grid.ColumnSpacing(16);
  grid.RowSpacing(12);

  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::Auto());
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  ColumnDefinition control_column;
  control_column.Width(GridLengthHelper::FromPixels(control_width));
  grid.ColumnDefinitions().Append(icon_column);
  grid.ColumnDefinitions().Append(text_column);
  grid.ColumnDefinitions().Append(control_column);

  Border icon_backdrop = SettingIconBackdrop(icon_content);
  Grid::SetColumn(icon_backdrop, 0);
  grid.Children().Append(icon_backdrop);

  StackPanel text_area;
  text_area.Spacing(2);
  text_area.HorizontalAlignment(HorizontalAlignment::Stretch);

  StackPanel title_line;
  title_line.Orientation(Orientation::Horizontal);
  title_line.Spacing(8);
  title_line.Children().Append(Text(title, 15, FW_SEMIBOLD));
  text_area.Children().Append(title_line);

  auto sub = Text(subtitle, 12);
  sub.Foreground(SettingsSecondaryTextBrush());
  sub.TextWrapping(TextWrapping::Wrap);
  text_area.Children().Append(sub);
  Grid::SetColumn(text_area, 1);
  grid.Children().Append(text_area);

  auto control_element = control.as<FrameworkElement>();
  control_element.VerticalAlignment(VerticalAlignment::Center);
  control_element.HorizontalAlignment(HorizontalAlignment::Right);
  Grid::SetColumn(control_element, 2);
  grid.Children().Append(control);
  return Card(grid);
}

Border SettingRow(std::wstring_view title,
                  std::wstring_view subtitle,
                  UIElement const& control,
                  std::wstring_view glyph,
                  std::wstring_view status = L"") {
  return SettingRowWithIcon(title, subtitle, control, Icon(glyph, 16), status);
}

Border SettingWideRowWithIcon(std::wstring_view title,
                              std::wstring_view subtitle,
                              UIElement const& control,
                              UIElement const& icon_content,
                              std::wstring_view status = L"") {
  (void)status;
  Grid grid;
  grid.HorizontalAlignment(HorizontalAlignment::Stretch);
  grid.ColumnSpacing(16);
  grid.RowSpacing(12);

  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::Auto());
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
  grid.ColumnDefinitions().Append(icon_column);
  grid.ColumnDefinitions().Append(text_column);

  RowDefinition text_row;
  text_row.Height(GridLengthHelper::Auto());
  RowDefinition control_row;
  control_row.Height(GridLengthHelper::Auto());
  grid.RowDefinitions().Append(text_row);
  grid.RowDefinitions().Append(control_row);

  Border icon_backdrop = SettingIconBackdrop(icon_content);
  Grid::SetColumn(icon_backdrop, 0);
  Grid::SetRow(icon_backdrop, 0);
  grid.Children().Append(icon_backdrop);

  StackPanel text_area;
  text_area.Spacing(2);
  text_area.HorizontalAlignment(HorizontalAlignment::Stretch);
  text_area.Children().Append(Text(title, 15, FW_SEMIBOLD));
  auto sub = Text(subtitle, 12);
  sub.Foreground(SettingsSecondaryTextBrush());
  sub.TextWrapping(TextWrapping::Wrap);
  text_area.Children().Append(sub);
  Grid::SetColumn(text_area, 1);
  Grid::SetRow(text_area, 0);
  grid.Children().Append(text_area);

  auto control_element = control.as<FrameworkElement>();
  control_element.VerticalAlignment(VerticalAlignment::Center);
  control_element.HorizontalAlignment(HorizontalAlignment::Stretch);
  Grid::SetColumn(control_element, 1);
  Grid::SetRow(control_element, 1);
  grid.Children().Append(control);
  return Card(grid);
}

Border SettingWideRow(std::wstring_view title,
                      std::wstring_view subtitle,
                      UIElement const& control,
                      std::wstring_view glyph,
                      std::wstring_view status = L"") {
  auto icon = Icon(glyph, 16);
  icon.HorizontalAlignment(HorizontalAlignment::Center);
  icon.VerticalAlignment(VerticalAlignment::Center);
  return SettingWideRowWithIcon(title, subtitle, control, icon, status);
}

void ConfigureSettingCombo(ComboBox const& combo) {
  ApplySettingsUIFont(combo);
  combo.Width(120);
  combo.MinWidth(120);
  combo.HorizontalAlignment(HorizontalAlignment::Right);
  combo.HorizontalContentAlignment(HorizontalAlignment::Center);
  const auto palette = CurrentSettingsPalette();
  combo.Background(Brush(palette.button));
  combo.Foreground(SettingsTextBrush());
  combo.BorderBrush(SettingsBorderBrush());
}

ComboBoxItem ThemeComboItem(std::wstring_view label) {
  const auto palette = CurrentSettingsPalette();
  ComboBoxItem item;
  ApplySettingsUIFont(item);
  auto text = Text(label, 13, FW_SEMIBOLD);
  text.Foreground(SettingsTextBrush());
  text.TextWrapping(TextWrapping::NoWrap);
  item.Content(text);
  item.Background(Brush(palette.card));
  item.Foreground(SettingsTextBrush());
  item.BorderBrush(TransparentBrush());
  item.BorderThickness(UniformThickness(0));
  item.RequestedTheme(CurrentSettingsElementTheme());
  item.HorizontalContentAlignment(HorizontalAlignment::Center);
  return item;
}

ComboBox ChoiceCombo(const std::vector<std::wstring>& labels,
                     int current_index,
                     std::function<void(int)> on_select) {
  ComboBox combo;
  ConfigureSettingCombo(combo);
  for (const auto& label : labels) {
    combo.Items().Append(ThemeComboItem(label));
  }
  if (labels.empty()) {
    return combo;
  }

  combo.SelectedIndex(std::clamp(current_index, 0, static_cast<int>(labels.size()) - 1));
  combo.SelectionChanged([combo, on_select](auto const&, auto const&) {
    const int selected = combo.SelectedIndex();
    if (selected < 0) {
      return;
    }
    if (on_select) {
      on_select(selected);
    }
  });
  return combo;
}

ComboBox StringChoiceComboWithIcon(
    std::wstring_view key,
    const std::vector<std::pair<std::wstring, std::wstring>>& choices,
    std::wstring_view default_value,
    TextBlock const& icon_label,
    std::wstring_view initial_value = L"",
    bool restart_input_core_on_change = false,
    std::function<std::wstring(std::wstring_view)> icon_text = {},
    std::function<void(std::wstring_view)> on_change = {});

ComboBox CandidateCountCombo() {
  const int current =
      ReadIntSetting(L"candidate_count", kDefaultCandidateCount, kMinCandidateCount, kMaxCandidateCount);
  std::vector<std::wstring> labels;
  for (int value = kMinCandidateCount; value <= kMaxCandidateCount; ++value) {
    labels.push_back(std::to_wstring(value));
  }
  return ChoiceCombo(labels, current - kMinCandidateCount, [](int selected) {
    WriteIntSetting(L"candidate_count", kMinCandidateCount + selected);
    RequestCandidateWindowVisualRefresh();
  });
}

ComboBox CandidateCountCombo(TextBlock const& icon_label) {
  const int current =
      ReadIntSetting(L"candidate_count", kDefaultCandidateCount, kMinCandidateCount, kMaxCandidateCount);
  icon_label.Text(std::to_wstring(current));
  std::vector<std::wstring> labels;
  for (int value = kMinCandidateCount; value <= kMaxCandidateCount; ++value) {
    labels.push_back(std::to_wstring(value));
  }
  return ChoiceCombo(labels, current - kMinCandidateCount, [icon_label](int selected) {
    const int value = kMinCandidateCount + selected;
    WriteIntSetting(L"candidate_count", value);
    icon_label.Text(std::to_wstring(value));
    RequestCandidateWindowVisualRefresh();
  });
}

ComboBox CandidateFontSizeCombo() {
  const int current = ReadIntSetting(L"candidate_font_size_level",
                                     kDefaultCandidateFontSizeLevel,
                                     kMinCandidateFontSizeLevel,
                                     kMaxCandidateFontSizeLevel);
  std::vector<std::wstring> labels;
  labels.reserve(kCandidateFontSizeLabels.size());
  for (const auto label : kCandidateFontSizeLabels) {
    labels.emplace_back(label);
  }
  return ChoiceCombo(labels, current, [](int selected) {
    if (selected < kMinCandidateFontSizeLevel || selected > kMaxCandidateFontSizeLevel) {
      return;
    }
    WriteIntSetting(L"candidate_font_size_level", selected);
    RequestCandidateWindowVisualRefresh();
    RequestCandidateWindowVisualRefreshDeferred(120);
  });
}

std::wstring CandidateFontIconText(std::wstring_view value) {
  return value == L"source_han_sans" || value == L"plangothic" ? L"源" : L"米";
}

ComboBox CandidateFontFamilyCombo(TextBlock const& icon_label) {
  return StringChoiceComboWithIcon(
      L"candidate_font_family",
      {{L"MiSans", L"misans"}, {L"思源黑体", L"source_han_sans"}},
      kDefaultCandidateFontFamily,
      icon_label,
      L"",
      false,
      CandidateFontIconText,
      [](std::wstring_view) {
        RequestCandidateWindowVisualRefresh();
        RequestCandidateWindowVisualRefreshDeferred(120);
      });
}

ComboBox IntChoiceCombo(std::wstring_view key,
                        const std::vector<std::wstring>& labels,
                        int default_value,
                        int min_value,
                        int max_value) {
  const int current = ReadIntSetting(key, default_value, min_value, max_value);
  return ChoiceCombo(labels, current, [key = std::wstring(key)](int selected) {
    WriteIntSetting(key, selected);
  });
}

ComboBox IntChoiceCombo(std::wstring_view key,
                        const std::vector<std::wstring>& labels,
                        int default_value,
                        int min_value,
                        int max_value,
                        TextBlock const& icon_label) {
  const int current = ReadIntSetting(key, default_value, min_value, max_value);
  icon_label.Text(IntChoiceIconText(labels, current));
  return ChoiceCombo(labels, current, [key = std::wstring(key), labels, icon_label](int selected) {
    WriteIntSetting(key, selected);
    icon_label.Text(IntChoiceIconText(labels, selected));
  });
}

ComboBox StringChoiceCombo(std::wstring_view key,
                           const std::vector<std::pair<std::wstring, std::wstring>>& choices,
                           std::wstring_view default_value,
                           std::wstring_view initial_value = L"",
                           bool restart_input_core_on_change = false,
                           std::function<void(std::wstring_view)> on_change = {}) {
  const std::wstring current =
      !initial_value.empty()
          ? std::wstring(initial_value)
          : (key == L"candidate_layout" ? ReadCandidateLayoutSetting(default_value)
                                         : ReadStringSetting(key, default_value));
  std::vector<std::wstring> labels;
  int selected_index = 0;
  for (size_t index = 0; index < choices.size(); ++index) {
    labels.push_back(choices[index].first);
    if (choices[index].second == current) {
      selected_index = static_cast<int>(index);
    }
  }

  return ChoiceCombo(labels,
                     selected_index,
                     [choices,
                      key = std::wstring(key),
                      restart_input_core_on_change,
                      on_change](int selected) {
    if (selected < 0 || selected >= static_cast<int>(choices.size())) {
      return;
    }
    const std::wstring value = choices[static_cast<size_t>(selected)].second;
    const std::wstring previous =
        key == L"candidate_layout" ? ReadCandidateLayoutSetting() : ReadStringSetting(key);
    if (key == L"candidate_layout") {
      WriteCandidateLayoutSetting(value);
    } else {
      WriteStringSetting(key, value);
    }
    const bool active_double_pinyin_setting =
        key != L"double_pinyin_scheme" ||
        ReadStringSetting(L"input_scheme", fp::kDefaultInputScheme) == L"double_pinyin";
    if (restart_input_core_on_change && active_double_pinyin_setting && previous != value) {
      RequestApplyInputConfigDeferred();
    }
    if (on_change) {
      on_change(value);
    }
  });
}

ComboBox StringChoiceComboWithIcon(
    std::wstring_view key,
    const std::vector<std::pair<std::wstring, std::wstring>>& choices,
    std::wstring_view default_value,
    TextBlock const& icon_label,
    std::wstring_view initial_value,
    bool restart_input_core_on_change,
    std::function<std::wstring(std::wstring_view)> icon_text,
    std::function<void(std::wstring_view)> on_change) {
  auto resolve_icon = [choices, icon_text](std::wstring_view value) {
    return icon_text ? icon_text(value) : ChoiceIconText(choices, value);
  };
  const std::wstring current =
      !initial_value.empty()
          ? std::wstring(initial_value)
          : (key == L"candidate_layout" ? ReadCandidateLayoutSetting(default_value)
                                         : ReadStringSetting(key, default_value));
  icon_label.Text(resolve_icon(current));
  return StringChoiceCombo(key,
                           choices,
                           default_value,
                           initial_value,
                           restart_input_core_on_change,
                           [icon_label, resolve_icon, on_change](std::wstring_view value) {
                             icon_label.Text(resolve_icon(value));
                             if (on_change) {
                               on_change(value);
                             }
                           });
}

ComboBox BoolChoiceCombo(std::wstring_view key,
                         const std::vector<std::pair<std::wstring, bool>>& choices,
                         bool default_value,
                         std::function<void(bool)> on_change = {}) {
  const bool current = ReadBoolSetting(key, default_value);
  std::vector<std::wstring> labels;
  int selected_index = 0;
  for (size_t index = 0; index < choices.size(); ++index) {
    labels.push_back(choices[index].first);
    if (choices[index].second == current) {
      selected_index = static_cast<int>(index);
    }
  }

  return ChoiceCombo(labels,
                     selected_index,
                     [choices, key = std::wstring(key), default_value, on_change](int selected) {
    if (selected < 0 || selected >= static_cast<int>(choices.size())) {
      return;
    }
    const bool value = choices[static_cast<size_t>(selected)].second;
    const bool previous = ReadBoolSetting(key, default_value);
    WriteBoolSetting(key, value);
    if (on_change && previous != value) {
      on_change(value);
    }
  });
}

void RequestPinyinConfigRestart(bool) {
  RequestApplyInputConfigDeferred();
}

void RequestRimeOptionRefresh(bool) {
  RequestInputStateRefreshDeferred();
}

ComboBox InputSchemeCombo(ComboBox const& double_scheme_combo,
                          std::function<void(std::wstring_view)> on_change = {}) {
  const std::vector<std::pair<std::wstring, std::wstring>> choices{
      {L"全拼", L"pinyin"},
      {L"双拼", L"double_pinyin"},
  };
  const std::wstring current = ReadStringSetting(L"input_scheme", fp::kDefaultInputScheme);
  std::vector<std::wstring> labels;
  int selected_index = 0;
  for (size_t index = 0; index < choices.size(); ++index) {
    labels.push_back(choices[index].first);
    if (choices[index].second == current) {
      selected_index = static_cast<int>(index);
    }
  }
  double_scheme_combo.IsEnabled(current == L"double_pinyin");

  return ChoiceCombo(labels, selected_index, [double_scheme_combo, choices, on_change](int selected) {
    if (selected < 0 || selected >= static_cast<int>(choices.size())) {
      return;
    }
    const std::wstring value = choices[static_cast<size_t>(selected)].second;
    const std::wstring previous = ReadStringSetting(L"input_scheme", fp::kDefaultInputScheme);
    WriteStringSetting(L"input_scheme", value);
    double_scheme_combo.IsEnabled(value == L"double_pinyin");
    if (previous != value) {
      RequestApplyInputConfigDeferred();
    }
    if (on_change) {
      on_change(value);
    }
  });
}

ComboBox InputSchemeCombo(ComboBox const& double_scheme_combo,
                          TextBlock const& icon_label,
                          std::function<void(std::wstring_view)> on_change = {}) {
  icon_label.Text(ReadStringSetting(L"input_scheme", fp::kDefaultInputScheme) == L"double_pinyin"
                      ? L"双"
                      : L"全");
  return InputSchemeCombo(double_scheme_combo,
                          [icon_label, on_change](std::wstring_view value) {
                            icon_label.Text(value == L"double_pinyin" ? L"双" : L"全");
                            if (on_change) {
                              on_change(value);
                            }
                          });
}

TextBox SettingTextBox(std::wstring_view key,
                       std::wstring_view placeholder = L"",
                       double min_width = 220,
                       std::wstring_view default_value = L"") {
  TextBox box;
  ApplySettingsUIFont(box);
  box.MinWidth(min_width);
  box.HorizontalAlignment(HorizontalAlignment::Right);
  box.Text(ReadStringSetting(key, default_value));
  box.PlaceholderText(placeholder);
  box.LostFocus([box, key = std::wstring(key)](auto const&, auto const&) {
    WriteStringSetting(key, box.Text().c_str());
  });
  return box;
}

TextBox SettingTextBox(std::wstring_view key,
                       std::wstring_view placeholder,
                       double min_width,
                       std::wstring_view default_value,
                       TextBlock const& icon_label) {
  icon_label.Text(FirstIconText(ReadStringSetting(key, default_value)));
  auto box = SettingTextBox(key, placeholder, min_width, default_value);
  box.TextChanged([box, icon_label](auto const&, auto const&) {
    icon_label.Text(FirstIconText(std::wstring_view(box.Text())));
  });
  return box;
}

ToggleSwitch SettingSwitch(std::wstring_view key,
                           bool default_value,
                           std::function<void(bool)> on_change = {}) {
  ToggleSwitch toggle;
  ApplySettingsUIFont(toggle);
  toggle.MinWidth(0);
  toggle.HorizontalAlignment(HorizontalAlignment::Right);
  toggle.IsOn(ReadBoolSetting(key, default_value));
  toggle.Toggled([toggle, key = std::wstring(key), on_change](auto const&, auto const&) {
    WriteBoolSetting(key, toggle.IsOn());
    if (on_change) {
      on_change(toggle.IsOn());
    }
  });
  return toggle;
}

ToggleSwitch ToolbarVisibleSwitch(std::function<void(bool)> on_change = {}) {
  ToggleSwitch toggle;
  ApplySettingsUIFont(toggle);
  toggle.MinWidth(0);
  toggle.HorizontalAlignment(HorizontalAlignment::Right);
  toggle.IsOn(ReadBoolSettingMigrated(kToolbarVisibleSetting,
                                      false,
                                      kLegacyToolbarVisibleSetting));
  toggle.Toggled([toggle, on_change](auto const&, auto const&) {
    if (g_syncing_toolbar_visible_switches) {
      return;
    }
    WriteBoolSetting(kToolbarVisibleSetting, toggle.IsOn());
    WriteBoolSetting(kLegacyToolbarVisibleSetting, false);
    RequestInputStateRefresh();
    if (on_change) {
      on_change(toggle.IsOn());
    }
  });
  return toggle;
}

ToggleSwitch SettingSwitch(std::wstring_view key,
                           bool default_value,
                           TextBlock const& icon_label,
                           std::function<std::wstring(bool)> icon_text = {},
                           std::function<void(bool)> on_change = {}) {
  auto resolve_icon = [icon_text](bool value) { return icon_text ? icon_text(value) : BoolIconText(value); };
  const bool current = ReadBoolSetting(key, default_value);
  icon_label.Text(resolve_icon(current));
  return SettingSwitch(key,
                       default_value,
                       [icon_label, resolve_icon, on_change](bool value) {
                         icon_label.Text(resolve_icon(value));
                         if (on_change) {
                           on_change(value);
                         }
                       });
}

UIElement WanxiangModeIcon(std::wstring_view value) {
  if (value == L"En") {
    return FluentPathIcon(kFluentIconTextGrammarCheckmark20RegularPath, 0.94);
  }
  if (value == L"Mix") {
    return FluentPathIcon(kFluentIconSparkleCircle20RegularPath, 0.94);
  }
  if (value == L"词" || value == L"``") {
    return FluentPathIcon(kFluentIconEdit20RegularPath, 0.94);
  }
  if (value == L"统") {
    return FluentPathIcon(kFluentIconDatabase20RegularPath, 0.96);
  }
  if (value == L"命") {
    return FluentPathIcon(kFluentIconKeyboard20RegularPath, 1.02);
  }
  return TextIcon(value, value.size() > 1 ? 13.0 : 15.0);
}

ToggleSwitch RimeConfigSwitch(std::wstring_view key, bool default_value) {
  return SettingSwitch(key, default_value, [](bool) {
    RequestApplyInputConfigDeferred();
  });
}

ToggleSwitch WanxiangModeSwitch(const WanxiangModeDefinition& mode) {
  ToggleSwitch toggle;
  ApplySettingsUIFont(toggle);
  toggle.MinWidth(0);
  toggle.HorizontalAlignment(HorizontalAlignment::Right);
  toggle.IsOn(mode.legacy_key.empty()
                  ? ReadBoolSetting(mode.setting_key, mode.default_value)
                  : ReadBoolSettingMigrated(mode.setting_key,
                                            mode.default_value,
                                            mode.legacy_key));
  toggle.Toggled([toggle, key = std::wstring(mode.setting_key)](auto const&, auto const&) {
    WriteBoolSetting(key, toggle.IsOn());
    RequestApplyInputConfigDeferred();
  });
  return toggle;
}

Button ActionButton(std::wstring_view text, std::wstring_view glyph) {
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(108);
  button.HorizontalAlignment(HorizontalAlignment::Right);
  StackPanel content;
  content.Orientation(Orientation::Horizontal);
  content.Spacing(8);
  content.Children().Append(Icon(glyph, kInlineButtonIconHostSize));
  content.Children().Append(Text(text, 13, FW_SEMIBOLD));
  button.Content(content);
  return button;
}

Button ActionPathButton(std::wstring_view text,
                        std::wstring_view path,
                        double scale = 0.88,
                        double view_box_size = 20.0) {
  const auto palette = CurrentSettingsPalette();
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(108);
  button.HorizontalAlignment(HorizontalAlignment::Right);
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
  Grid content;
  content.ColumnSpacing(8);
  content.VerticalAlignment(VerticalAlignment::Center);
  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::FromPixels(kActionButtonIconHostSize));
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::Auto());
  content.ColumnDefinitions().Append(icon_column);
  content.ColumnDefinitions().Append(text_column);

  auto icon = ActionButtonPathIcon(path, scale, view_box_size);
  Grid::SetColumn(icon, 0);
  content.Children().Append(icon);
  auto label = Text(text, 13, FW_SEMIBOLD);
  label.TextWrapping(TextWrapping::NoWrap);
  label.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(label, 1);
  content.Children().Append(label);
  button.Content(content);
  return button;
}

std::wstring ShortcutConflictTooltip(std::wstring_view current_key,
                                     std::wstring_view display) {
  const std::wstring normalized = NormalizeShortcutDisplay(display);
  if (normalized.empty()) {
    return L"已清除";
  }

  struct HotkeyConflictEntry {
    std::wstring_view key;
    std::wstring_view label;
    std::wstring_view fallback;
  };
  constexpr std::array<HotkeyConflictEntry, 8> shortcuts{{
      {L"shortcut_toolbar_input_mode", L"中/英文模式", L"Shift"},
      {L"shortcut_toolbar_shape", L"全/半角", L"Shift+."},
      {L"shortcut_toolbar_punctuation", L"中/英文标点", L"Ctrl+."},
      {L"shortcut_toolbar_charset", L"简体/繁体", L"Ctrl+Shift+F"},
      {L"shortcut_toolbar_emoji", L"表情符号/符号", L"Win+."},
      {L"shortcut_candidate_expand", L"展开/收起候选框", L"Tab"},
      {L"shortcut_candidate_previous_page", L"上一页", L"PgUp"},
      {L"shortcut_candidate_next_page", L"下一页", L"PgDn"},
  }};

  for (const auto& entry : shortcuts) {
    if (entry.key == current_key) {
      continue;
    }
    if (NormalizeShortcutDisplay(ReadStringSetting(entry.key, entry.fallback)) == normalized) {
      return L"与 " + std::wstring(entry.label) + L" 使用相同热键";
    }
  }
  return L"点击后按新的组合键";
}

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

Button CompactActionButton(std::wstring_view text, std::wstring_view glyph) {
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(86);
  button.Padding(Thickness{10, 6, 10, 7});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  StackPanel content;
  content.Orientation(Orientation::Horizontal);
  content.Spacing(6);
  content.Children().Append(Icon(glyph, kInlineButtonIconHostSize));
  content.Children().Append(Text(text, 12, FW_SEMIBOLD));
  button.Content(content);
  return button;
}

Button CompactPathActionButton(std::wstring_view text,
                               std::wstring_view path,
                               double scale = 0.82) {
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(74);
  button.Padding(Thickness{8, 5, 9, 6});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  Grid content;
  content.ColumnSpacing(6);
  content.VerticalAlignment(VerticalAlignment::Center);
  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::FromPixels(kInlineButtonIconHostSize));
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::Auto());
  content.ColumnDefinitions().Append(icon_column);
  content.ColumnDefinitions().Append(text_column);

  auto icon = FluentButtonPathIcon(path, scale);
  Grid::SetColumn(icon, 0);
  content.Children().Append(icon);
  auto label = Text(text, 12, FW_SEMIBOLD);
  label.TextWrapping(TextWrapping::NoWrap);
  label.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(label, 1);
  content.Children().Append(label);
  button.Content(content);
  return button;
}

Button InlinePathActionButton(std::wstring_view text,
                              std::wstring_view path,
                              double scale = 0.68) {
  const auto palette = CurrentSettingsPalette();
  Button button;
  ApplySettingsUIFont(button);
  button.MinWidth(50);
  button.Height(28);
  button.MinHeight(28);
  button.Padding(Thickness{5, 2, 6, 3});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  button.BorderThickness(UniformThickness(0));
  auto normal = TransparentBrush().as<IInspectable>();
  auto hover = Brush(palette.button_hover).as<IInspectable>();
  auto pressed = Brush(palette.button_pressed).as<IInspectable>();
  button.Resources().Insert(box_value(L"ButtonBackground"), normal);
  button.Resources().Insert(box_value(L"ButtonBackgroundPointerOver"), hover);
  button.Resources().Insert(box_value(L"ButtonBackgroundPressed"), pressed);
  button.Resources().Insert(box_value(L"ButtonBorderBrush"), normal);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPointerOver"), normal);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPressed"), normal);
  const auto foreground = Brush(palette.text).as<IInspectable>();
  button.Resources().Insert(box_value(L"ButtonForeground"), foreground);
  button.Resources().Insert(box_value(L"ButtonForegroundPointerOver"), foreground);
  button.Resources().Insert(box_value(L"ButtonForegroundPressed"), foreground);

  Grid content;
  content.ColumnSpacing(4);
  content.VerticalAlignment(VerticalAlignment::Center);
  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::FromPixels(kInlineButtonIconHostSize));
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::Auto());
  content.ColumnDefinitions().Append(icon_column);
  content.ColumnDefinitions().Append(text_column);

  auto icon = FluentButtonPathIcon(path, scale);
  Grid::SetColumn(icon, 0);
  content.Children().Append(icon);
  auto label = Text(text, 13, FW_SEMIBOLD);
  label.TextWrapping(TextWrapping::NoWrap);
  label.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(label, 1);
  content.Children().Append(label);
  button.Content(content);
  return button;
}

Border StableInlinePathActionButton(std::wstring_view text,
                                    std::wstring_view path,
                                    double scale,
                                    std::function<void()> on_click) {
  const auto palette = CurrentSettingsPalette();
  Border button;
  button.MinWidth(60);
  button.Height(28);
  button.Padding(Thickness{8, 2, 9, 3});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  button.VerticalAlignment(VerticalAlignment::Center);
  button.Background(Brush(palette.button));
  button.BorderBrush(SettingsBorderBrush());
  button.BorderThickness(SettingsHairlineThickness());
  button.CornerRadius(Radius(8));
  button.UseLayoutRounding(true);

  Grid content;
  content.ColumnSpacing(5);
  content.VerticalAlignment(VerticalAlignment::Center);
  ColumnDefinition icon_column;
  icon_column.Width(GridLengthHelper::FromPixels(kInlineButtonIconHostSize));
  ColumnDefinition text_column;
  text_column.Width(GridLengthHelper::Auto());
  content.ColumnDefinitions().Append(icon_column);
  content.ColumnDefinitions().Append(text_column);

  auto icon = FluentButtonPathIcon(path, scale);
  Grid::SetColumn(icon, 0);
  content.Children().Append(icon);
  auto label = Text(text, 12, FW_SEMIBOLD);
  label.TextWrapping(TextWrapping::NoWrap);
  label.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(label, 1);
  content.Children().Append(label);
  button.Child(content);

  auto hovered = std::make_shared<bool>(false);
  auto pressed = std::make_shared<bool>(false);
  auto apply_visual = std::make_shared<std::function<void()>>();
  *apply_visual = [button, label, hovered, pressed, palette]() {
    if (*pressed) {
      button.Background(Brush(palette.button_pressed));
      button.BorderBrush(SettingsBorderHoverBrush());
    } else if (*hovered) {
      button.Background(Brush(palette.button_hover));
      button.BorderBrush(SettingsBorderHoverBrush());
    } else {
      button.Background(Brush(palette.button));
      button.BorderBrush(SettingsBorderBrush());
    }
    label.Foreground(Brush(palette.text));
  };

  button.PointerEntered([hovered, apply_visual](auto const&, auto const&) {
    *hovered = true;
    (*apply_visual)();
  });
  button.PointerExited([hovered, pressed, apply_visual](auto const&, auto const&) {
    *hovered = false;
    *pressed = false;
    (*apply_visual)();
  });
  button.PointerPressed([pressed, apply_visual](auto const&, auto const&) {
    *pressed = true;
    (*apply_visual)();
  });
  button.PointerReleased([pressed, apply_visual](auto const&, auto const&) {
    *pressed = false;
    (*apply_visual)();
  });
  button.Tapped([pressed, apply_visual, on_click = std::move(on_click)](auto const&, auto const&) {
    *pressed = false;
    (*apply_visual)();
    if (on_click) {
      on_click();
    }
  });
  (*apply_visual)();
  return button;
}

Border StableIconToolButton(std::wstring_view path,
                            std::wstring_view tooltip,
                            double scale,
                            std::function<void()> on_click,
                            double width = 36.0,
                            double height = 36.0) {
  const auto palette = CurrentSettingsPalette();
  Border button;
  button.Width(width);
  button.Height(height);
  button.MinWidth(width);
  button.Padding(Thickness{0, 0, 0, 1});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  button.VerticalAlignment(VerticalAlignment::Center);
  button.Background(Brush(palette.button));
  button.BorderBrush(SettingsBorderBrush());
  button.BorderThickness(SettingsHairlineThickness());
  button.CornerRadius(Radius(8));
  button.UseLayoutRounding(true);
  button.Child(ActionButtonPathIcon(path, scale));
  SetSettingsToolTip(button, tooltip);

  auto hovered = std::make_shared<bool>(false);
  auto pressed = std::make_shared<bool>(false);
  auto apply_visual = std::make_shared<std::function<void()>>();
  *apply_visual = [button, hovered, pressed, palette]() {
    if (*pressed) {
      button.Background(Brush(palette.button_pressed));
      button.BorderBrush(SettingsBorderHoverBrush());
    } else if (*hovered) {
      button.Background(Brush(palette.button_hover));
      button.BorderBrush(SettingsBorderHoverBrush());
    } else {
      button.Background(Brush(palette.button));
      button.BorderBrush(SettingsBorderBrush());
    }
  };

  button.PointerEntered([hovered, apply_visual](auto const&, auto const&) {
    *hovered = true;
    (*apply_visual)();
  });
  button.PointerExited([hovered, pressed, apply_visual](auto const&, auto const&) {
    *hovered = false;
    *pressed = false;
    (*apply_visual)();
  });
  button.PointerPressed([pressed, apply_visual](auto const&, auto const&) {
    *pressed = true;
    (*apply_visual)();
  });
  button.PointerReleased([pressed, apply_visual](auto const&, auto const&) {
    *pressed = false;
    (*apply_visual)();
  });
  button.Tapped([pressed, apply_visual, on_click = std::move(on_click)](auto const&, auto const&) {
    *pressed = false;
    (*apply_visual)();
    if (on_click) {
      on_click();
    }
  });
  (*apply_visual)();
  return button;
}

Button IconToolButton(std::wstring_view path, std::wstring_view tooltip, double scale = 1.0) {
  const auto palette = CurrentSettingsPalette();
  Button button;
  ApplySettingsUIFont(button);
  button.Width(36);
  button.Height(36);
  button.MinWidth(36);
  button.Padding(Thickness{0, 0, 0, 1});
  button.HorizontalAlignment(HorizontalAlignment::Right);
  button.VerticalAlignment(VerticalAlignment::Center);
  button.Background(Brush(palette.button));
  button.BorderBrush(SettingsBorderBrush());
  button.BorderThickness(SettingsHairlineThickness());
  button.CornerRadius(Radius(8));
  const auto background = Brush(palette.button).as<IInspectable>();
  const auto hover = Brush(palette.button_hover).as<IInspectable>();
  const auto pressed = Brush(palette.button_pressed).as<IInspectable>();
  const auto border = SettingsBorderBrush().as<IInspectable>();
  const auto border_hover = SettingsBorderHoverBrush().as<IInspectable>();
  button.Resources().Insert(box_value(L"ButtonBackground"), background);
  button.Resources().Insert(box_value(L"ButtonBackgroundPointerOver"), hover);
  button.Resources().Insert(box_value(L"ButtonBackgroundPressed"), pressed);
  button.Resources().Insert(box_value(L"ButtonBorderBrush"), border);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPointerOver"), border_hover);
  button.Resources().Insert(box_value(L"ButtonBorderBrushPressed"), border_hover);
  button.Content(ActionButtonPathIcon(path, scale));
  SetSettingsToolTip(button, tooltip);
  return button;
}

NavigationViewItem NavItem(std::wstring_view title,
                           std::wstring_view tag,
                           std::wstring_view glyph) {
  NavigationViewItem item;
  ApplySettingsUIFont(item);
  auto label = Text(title, 16, FW_SEMIBOLD);
  label.Foreground(SettingsTextBrush());
  label.TextWrapping(TextWrapping::NoWrap);
  item.Content(label);
  item.Tag(box_value(tag));
  auto nav_icon = Icon(glyph, 20);
  nav_icon.Foreground(SettingsIconBrush());
  item.Icon(nav_icon);
  item.Foreground(SettingsTextBrush());
  return item;
}

void ResetDefaultSettings() {
  std::error_code error;
  std::filesystem::remove(SettingsPath(), error);
  ResetSettingsCache();

  WriteIntSetting(L"candidate_count", kDefaultCandidateCount);
  WriteIntSetting(L"candidate_font_size_level", kDefaultCandidateFontSizeLevel);
  WriteStringSetting(L"candidate_font_family", kDefaultCandidateFontFamily);
  WriteBoolSetting(L"candidate_horizontal", true);
  WriteStringSetting(L"candidate_layout", L"horizontal");
  WriteBoolSetting(L"status_tip_enabled", true);
  WriteStringSetting(L"status_tip_blacklist", kDefaultStatusTipBlacklist);
  WriteBoolSetting(kToolbarVisibleSetting, false);
  WriteBoolSetting(kLegacyToolbarVisibleSetting, false);
  WriteStringSetting(fp::kLegacyThemeSetting, fp::kThemeModeSystem);
  WriteStringSetting(fp::kThemeModeSetting, fp::kThemeModeSystem);
  WriteStringSetting(fp::kThemePresetSetting, DefaultPresetForThemeMode(fp::kThemeModeSystem));
  WriteStringSetting(L"input_scheme", fp::kDefaultInputScheme);
  WriteStringSetting(L"double_pinyin_scheme", fp::kDefaultDoublePinyinScheme);
  WriteStringSetting(L"input_profile", L"base");
  WriteStringSetting(L"default_input_mode", L"zh");
  WriteStringSetting(L"default_charset", L"simplified");
  WriteBoolSetting(L"default_shape_half", true);
  WriteBoolSetting(L"default_chinese_punctuation", true);
  WriteBoolSetting(L"auto_pinyin_correction", true);
  WriteBoolSetting(L"super_abbrev", true);
  WriteBoolSetting(L"smart_fuzzy_pinyin", true);
  WriteBoolSetting(L"fuzzy_pinyin", false);
  WriteStringSetting(L"fuzzy_pinyin_rules", kDefaultFuzzyPinyinRules);
  WriteStringSetting(L"fuzzy_pinyin_custom_rules", L"");
  WriteBoolSetting(L"user_lexicon_enabled", true);
  WriteBoolSetting(L"custom_phrases_enabled", true);
  WriteBoolSetting(L"imported_lexicons_enabled", true);
  WriteBoolSetting(L"name_input", true);
  WriteBoolSetting(L"u_mode", true);
  WriteBoolSetting(L"v_mode", true);
  for (const auto& mode : WanxiangModeDefinitions()) {
    WriteBoolSetting(mode.setting_key, mode.default_value);
  }

  WriteStringSetting(L"shortcut_toolbar_input_mode", L"Shift");
  WriteStringSetting(L"shortcut_toolbar_shape", L"Shift+.");
  WriteStringSetting(L"shortcut_toolbar_punctuation", L"Ctrl+.");
  WriteStringSetting(L"shortcut_toolbar_charset", L"Ctrl+Shift+F");
  WriteStringSetting(L"shortcut_toolbar_emoji", L"Win+.");
  WriteStringSetting(L"shortcut_candidate_expand", L"Tab");
  WriteStringSetting(L"shortcut_candidate_previous_page", L"PgUp");
  WriteStringSetting(L"shortcut_candidate_next_page", L"PgDn");
  WriteBoolSetting(L"sync_clipboard", false);
  WriteBoolSetting(L"sync_user_data", false);
  WriteStringSetting(L"sync_provider", L"object");
  WriteBoolSetting(L"sync_auto_enabled", false);
  WriteIntSetting(L"sync_auto_interval_minutes", 30);
  WriteStringSetting(L"sync_object_region", L"auto");
  WriteStringSetting(L"sync_object_key", fp::sync::DefaultObjectKey());
}

class SettingsApp : public ApplicationT<SettingsApp, Markup::IXamlMetadataProvider> {
 public:
  SettingsApp() {
    RequestedTheme(CurrentSettingsApplicationTheme());
    UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& args) {
      const std::wstring message = L"WinUI 3 未处理异常：\n" + std::wstring(args.Message());
      MessageBoxW(nullptr, message.c_str(), L"流畅拼音 设置", MB_ICONERROR);
    });
  }

  void OnLaunched(LaunchActivatedEventArgs const&) {
    Resources().MergedDictionaries().Append(XamlControlsResources());
    window_ = Window();
    g_settings_window_hwnd = GetWindowHandle(window_);
    ApplySettingsResources(Resources());
    window_.Title(kSettingsAppTitle);
    window_.ExtendsContentIntoTitleBar(true);
    window_.Content(BuildRoot(InitialPageTag()));
    if (title_bar_drag_region_ != nullptr) {
      window_.SetTitleBar(title_bar_drag_region_);
    }
    window_.Activate();
    window_.AppWindow().Title(kSettingsAppTitle);
    ApplyWindowIcons(window_);
    ApplyTitleBarColors(window_);
    ApplyDwmWindowFrame(window_);
    const auto default_window_size = DefaultWindowSize(window_);
    ApplyMinimumWindowSize(
        window_, SizeInt32{kMinSettingsWindowWidthDips, kMinSettingsWindowHeightDips});
    window_.AppWindow().Resize(default_window_size);
    CenterWindowOnMonitor(window_);
  }

  Markup::IXamlType GetXamlType(TypeName const& type) {
    return metadata_provider_.GetXamlType(type);
  }

  Markup::IXamlType GetXamlType(hstring const& full_name) {
    return metadata_provider_.GetXamlType(full_name);
  }

  com_array<Markup::XmlnsDefinition> GetXmlnsDefinitions() {
    return metadata_provider_.GetXmlnsDefinitions();
  }

 private:
  void ApplyThemeAndRefreshCurrentPage() {
    if (window_ == nullptr) {
      return;
    }
    g_settings_window_hwnd = GetWindowHandle(window_);
    hstring current_tag = hstring(current_page_tag_);
    if (nav_ != nullptr) {
      if (auto item = nav_.SelectedItem().try_as<NavigationViewItem>()) {
        current_tag = unbox_value<hstring>(item.Tag());
      }
    }

    ResetSettingsCache();
    const auto palette = CurrentSettingsPalette();
    SetSettingsPaletteOverride(palette);
    RequestedTheme(CurrentSettingsApplicationTheme());
    root_ = nullptr;
    nav_ = nullptr;
    title_bar_drag_region_ = nullptr;
    page_cache_.clear();
    window_.Content(UIElement{nullptr});
    ApplySettingsResources(Resources());
    window_.Content(BuildRoot(current_tag.c_str()));
    if (title_bar_drag_region_ != nullptr) {
      window_.SetTitleBar(title_bar_drag_region_);
    }
    ApplyTitleBarColors(window_);
    ApplyDwmWindowFrame(window_);
    ClearSettingsPaletteOverride();
  }

  UIElement BuildRoot(std::wstring_view initial_page) {
    Grid root;
    root.RequestedTheme(CurrentSettingsElementTheme());
    root.Background(SettingsSurfaceBrush());
    root.UseLayoutRounding(true);
    root_ = root;

    nav_ = NavigationView();
    ApplySettingsUIFont(nav_);
    nav_.RequestedTheme(CurrentSettingsElementTheme());
    nav_.Background(SettingsSurfaceBrush());
    nav_.UseLayoutRounding(true);
    nav_.PaneDisplayMode(NavigationViewPaneDisplayMode::Left);
    nav_.OpenPaneLength(kNavigationOpenPaneLength);
    nav_.CompactPaneLength(52);
    nav_.IsBackButtonVisible(NavigationViewBackButtonVisible::Collapsed);
    nav_.IsSettingsVisible(false);
    nav_.AlwaysShowHeader(false);
    nav_.PaneTitle(L"流畅拼音");
    const std::wstring selected_page =
        initial_page.empty() ? InitialPageTag() : std::wstring(initial_page);

    auto general = NavItem(L"常规", L"general", L"\uE115");
    nav_.MenuItems().Append(general);
    nav_.MenuItems().Append(NavItem(L"外观", L"appearance", L"\uE790"));
    nav_.MenuItems().Append(NavItem(L"词库", L"lexicon", L"\uE82D"));
    nav_.MenuItems().Append(NavItem(L"热键", L"hotkeys", L"\uE144"));
    nav_.MenuItems().Append(NavItem(L"同步", L"sync", L"\uE117"));
    nav_.MenuItems().Append(NavItem(L"高级", L"advanced", L"\uE90F"));
    nav_.MenuItems().Append(NavItem(L"关于", L"about", L"\uE946"));
    SelectNavItem(selected_page);
    SetPage(hstring(selected_page));
    nav_.SelectionChanged([this](NavigationView const&,
                                 NavigationViewSelectionChangedEventArgs const& args) {
      if (auto item = args.SelectedItem().try_as<NavigationViewItem>()) {
        SetPage(unbox_value<hstring>(item.Tag()));
      }
    });

    root.Children().Append(nav_);
    title_bar_drag_region_ = Grid();
    title_bar_drag_region_.Height(kTitleBarDragHeight);
    title_bar_drag_region_.Margin(Thickness{kNavigationOpenPaneLength,
                                            0,
                                            kTitleBarCaptionButtonReservedWidth,
                                            0});
    title_bar_drag_region_.HorizontalAlignment(HorizontalAlignment::Stretch);
    title_bar_drag_region_.VerticalAlignment(VerticalAlignment::Top);
    title_bar_drag_region_.Background(TransparentBrush());
    root.Children().Append(title_bar_drag_region_);
    return root;
  }

  void SetPage(hstring const& tag) {
    const std::wstring key(tag);
    current_page_tag_ = key;
    if (const auto cached = page_cache_.find(key); cached != page_cache_.end()) {
      nav_.Content(cached->second);
      return;
    }

    UIElement page{nullptr};
    if (tag == L"general") {
      page = BuildGeneralPage();
    } else if (tag == L"advanced") {
      page = BuildAdvancedPage();
    } else if (tag == L"appearance") {
      page = BuildAppearancePage();
    } else if (tag == L"lexicon") {
      page = BuildLexiconPage();
    } else if (tag == L"hotkeys") {
      page = BuildHotkeysPage();
    } else if (tag == L"sync") {
      page = BuildSyncPage();
    } else {
      page = BuildAboutPage();
    }
    UIElement framed_page = ContentFrame(page);
    page_cache_.insert_or_assign(key, framed_page);
    nav_.Content(framed_page);
  }

  UIElement ContentFrame(UIElement const& page) {
    Border frame = SettingsFrame(page,
                                 CurrentSettingsPalette().background,
                                 Radius(8),
                                 UniformThickness(0),
                                 Thickness{kContentFrameLeftInset,
                                           kContentFrameTopInset,
                                           kContentFrameRightInset,
                                           kContentFrameBottomInset});
    frame.HorizontalAlignment(HorizontalAlignment::Stretch);
    frame.VerticalAlignment(VerticalAlignment::Stretch);
    return frame;
  }

  void SelectNavItem(std::wstring_view tag) {
    for (IInspectable const& item_object : nav_.MenuItems()) {
      if (auto item = item_object.try_as<NavigationViewItem>()) {
        if (unbox_value<hstring>(item.Tag()) == tag) {
          nav_.SelectedItem(item);
          return;
        }
      }
    }
  }

  ScrollViewer Scroll(UIElement const& content) {
    ScrollViewer viewer;
    viewer.Content(content);
    viewer.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    viewer.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    viewer.VerticalScrollMode(ScrollMode::Enabled);
    viewer.HorizontalScrollMode(ScrollMode::Disabled);
    viewer.ZoomMode(ZoomMode::Disabled);
    viewer.IsScrollInertiaEnabled(true);
    viewer.BringIntoViewOnFocusChange(false);
    viewer.HorizontalAlignment(HorizontalAlignment::Stretch);
    viewer.Background(SettingsSurfaceBrush());
    return viewer;
  }

  void ShowFuzzyPinyinRulesDialog(ToggleSwitch const& fuzzy_switch,
                                  Grid const& state_icon,
                                  bool require_enabled,
                                  std::shared_ptr<bool> suppress_toggle_dialog) {
    ContentDialog dialog;
    ApplySettingsDialogBase(dialog, window_.Content().as<FrameworkElement>().XamlRoot());
    dialog.PrimaryButtonText(L"完成");
    dialog.CloseButtonText(L"取消");
    dialog.DefaultButton(ContentDialogButton::Primary);
    ApplySettingsResources(dialog.Resources());
    dialog.Resources().Insert(box_value(L"ContentDialogPadding"),
                              box_value(Thickness{24, 16, 24, 8}));
    dialog.Resources().Insert(box_value(L"ContentDialogCommandSpaceMargin"),
                              box_value(Thickness{0, 6, 0, 0}));
    dialog.Resources().Insert(box_value(L"ContentDialogTitleMargin"),
                              box_value(Thickness{0, 0, 0, 10}));

    StackPanel content;
    content.Width(kSettingsDialogContentWidth);
    content.MaxWidth(kSettingsDialogContentWidth);
    content.Spacing(7);
    content.HorizontalAlignment(HorizontalAlignment::Stretch);
    content.Children().Append(Text(L"模糊拼音规则", 20, FW_SEMIBOLD));

    auto description = Text(L"勾选或添加要启用的规则。", 12);
    description.Foreground(SettingsSecondaryTextBrush());
    content.Children().Append(description);

    Grid mode_row;
    mode_row.ColumnSpacing(12);
    mode_row.HorizontalAlignment(HorizontalAlignment::Stretch);
    ColumnDefinition mode_title_column;
    mode_title_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition mode_combo_column;
    mode_combo_column.Width(GridLengthHelper::Auto());
    mode_row.ColumnDefinitions().Append(mode_title_column);
    mode_row.ColumnDefinitions().Append(mode_combo_column);

    auto mode_title = Text(L"编辑内容", 13, FW_SEMIBOLD);
    mode_title.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(mode_title, 0);
    mode_row.Children().Append(mode_title);

    const std::array<std::wstring_view, 2> mode_labels{L"预设规则", L"自定义规则"};
    auto mode_index =
        std::make_shared<int>(CurrentFuzzyPinyinCustomRuleCount() > 0 ? 1 : 0);
    auto update_visible_panel = std::make_shared<std::function<void()>>();

    Border mode_selector_frame;
    mode_selector_frame.Width(244);
    mode_selector_frame.Height(38);
    mode_selector_frame.HorizontalAlignment(HorizontalAlignment::Right);
    mode_selector_frame.VerticalAlignment(VerticalAlignment::Center);

    Grid mode_selector_grid;
    mode_selector_grid.ColumnSpacing(3);
    for (int index = 0; index < 2; ++index) {
      ColumnDefinition column;
      column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
      mode_selector_grid.ColumnDefinitions().Append(column);
    }

    auto mode_item_frames = std::make_shared<std::vector<Border>>();
    auto mode_item_labels = std::make_shared<std::vector<TextBlock>>();
    auto update_mode_item_visuals = std::make_shared<std::function<void()>>();
    auto hovered_mode_item = std::make_shared<int>(-1);
    auto pressed_mode_item = std::make_shared<int>(-1);
    auto apply_mode_item_visual = [mode_index, hovered_mode_item, pressed_mode_item](
                                       Border const& item,
                                       TextBlock const& label,
                                       int index) {
      const auto palette = CurrentSettingsPalette();
      const bool selected = index == *mode_index;
      const bool hovered = index == *hovered_mode_item;
      const bool pressed = index == *pressed_mode_item;
      if (pressed) {
        item.Background(selected ? Brush(palette.button_pressed) : Brush(palette.button_hover));
      } else if (hovered) {
        item.Background(selected ? Brush(palette.button_hover) : Brush(palette.icon_backdrop));
      } else {
        item.Background(selected ? Brush(palette.button) : TransparentBrush());
      }
      label.Foreground(selected ? Brush(palette.text) : Brush(palette.secondary_text));
    };
    auto add_mode_item = [&](int index) {
      Border item;
      item.Height(32);
      item.Padding(Thickness{8, 0, 8, 1});
      item.HorizontalAlignment(HorizontalAlignment::Stretch);
      item.CornerRadius(Radius(6));
      item.BorderThickness(UniformThickness(0));

      auto label = Text(mode_labels[static_cast<size_t>(index)], 13, FW_SEMIBOLD);
      label.TextWrapping(TextWrapping::NoWrap);
      label.HorizontalAlignment(HorizontalAlignment::Center);
      label.VerticalAlignment(VerticalAlignment::Center);
      label.HorizontalTextAlignment(TextAlignment::Center);
      item.Child(label);
      apply_mode_item_visual(item, label, index);

      item.PointerEntered([hovered_mode_item,
                           mode_item_frames,
                           mode_item_labels,
                           update_mode_item_visuals,
                           index](auto const&, auto const&) {
        *hovered_mode_item = index;
        if (*update_mode_item_visuals) {
          (*update_mode_item_visuals)();
        }
      });
      item.PointerExited([hovered_mode_item,
                          pressed_mode_item,
                          mode_item_frames,
                          mode_item_labels,
                          update_mode_item_visuals,
                          index](auto const&, auto const&) {
        if (*hovered_mode_item == index) {
          *hovered_mode_item = -1;
        }
        if (*pressed_mode_item == index) {
          *pressed_mode_item = -1;
        }
        if (*update_mode_item_visuals) {
          (*update_mode_item_visuals)();
        }
      });
      item.PointerPressed([pressed_mode_item,
                           update_mode_item_visuals,
                           index](auto const&, auto const&) {
        *pressed_mode_item = index;
        if (*update_mode_item_visuals) {
          (*update_mode_item_visuals)();
        }
      });
      item.PointerReleased([pressed_mode_item, update_mode_item_visuals](auto const&,
                                                                        auto const&) {
        *pressed_mode_item = -1;
        if (*update_mode_item_visuals) {
          (*update_mode_item_visuals)();
        }
      });
      item.Tapped([mode_index,
                  pressed_mode_item,
                  mode_item_frames,
                  mode_item_labels,
                  update_visible_panel,
                  update_mode_item_visuals,
                  index](auto const&, auto const&) {
        *mode_index = index;
        *pressed_mode_item = -1;
        if (*update_mode_item_visuals) {
          (*update_mode_item_visuals)();
        } else {
          for (size_t item_index = 0; item_index < mode_item_frames->size(); ++item_index) {
            const auto palette = CurrentSettingsPalette();
            mode_item_frames->at(item_index).Background(
                static_cast<int>(item_index) == *mode_index
                    ? Brush(palette.button)
                    : TransparentBrush());
            mode_item_labels->at(item_index).Foreground(
                static_cast<int>(item_index) == *mode_index ? Brush(palette.text)
                                                            : Brush(palette.secondary_text));
          }
        }
        if (*update_visible_panel) {
          (*update_visible_panel)();
        }
      });
      mode_item_frames->push_back(item);
      mode_item_labels->push_back(label);
      Grid::SetColumn(item, index);
      mode_selector_grid.Children().Append(item);
    };
    add_mode_item(0);
    add_mode_item(1);
    *update_mode_item_visuals = [mode_index,
                                 mode_item_frames,
                                 mode_item_labels,
                                 apply_mode_item_visual]() {
      for (size_t index = 0; index < mode_item_frames->size(); ++index) {
        apply_mode_item_visual(mode_item_frames->at(index),
                               mode_item_labels->at(index),
                               static_cast<int>(index));
      }
    };
    mode_selector_frame = SettingsFrame(mode_selector_grid,
                                        CurrentSettingsPalette().card,
                                        Radius(8),
                                        UniformThickness(3));
    mode_selector_frame.Width(244);
    mode_selector_frame.Height(38);
    mode_selector_frame.HorizontalAlignment(HorizontalAlignment::Right);
    mode_selector_frame.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(mode_selector_frame, 1);
    mode_row.Children().Append(mode_selector_frame);
    content.Children().Append(mode_row);

    constexpr double kRulesListHeight = kSettingsDialogListHeight;
    constexpr double kBuiltInRuleRowHeight = kRulesListHeight / 5.0;
    constexpr double kBuiltInRulesHeight = kRulesListHeight;
    constexpr double kCustomRulesHeight = kRulesListHeight;

    Grid built_in_content;
    built_in_content.RowSpacing(6);
    RowDefinition built_in_header_row;
    built_in_header_row.Height(GridLengthHelper::Auto());
    RowDefinition built_in_rules_row;
    built_in_rules_row.Height(GridLengthHelper::FromPixels(kBuiltInRulesHeight));
    built_in_content.RowDefinitions().Append(built_in_header_row);
    built_in_content.RowDefinitions().Append(built_in_rules_row);

    Grid built_in_header;
    built_in_header.ColumnSpacing(8);
    built_in_header.MinHeight(32);
    ColumnDefinition built_in_title_column;
    built_in_title_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition built_in_actions_column;
    built_in_actions_column.Width(GridLengthHelper::Auto());
    built_in_header.ColumnDefinitions().Append(built_in_title_column);
    built_in_header.ColumnDefinitions().Append(built_in_actions_column);

    auto built_in_title = Text(L"预设规则", 13, FW_SEMIBOLD);
    built_in_title.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(built_in_title, 0);
    built_in_header.Children().Append(built_in_title);

    StackPanel actions;
    actions.Orientation(Orientation::Horizontal);
    actions.Spacing(4);
    actions.VerticalAlignment(VerticalAlignment::Center);

    auto checks = std::make_shared<std::vector<std::pair<std::wstring, CheckBox>>>();
    const auto selected_rules = CurrentFuzzyPinyinRules(true);
    auto all = StableInlinePathActionButton(
        L"全选",
        kFluentIconCheckmarkCircle20RegularPath,
        0.66,
        [checks]() {
          SetFuzzyPinyinRuleChecks(
              *checks,
              ParseFuzzyPinyinRuleSetting(kDefaultFuzzyPinyinRules, true));
        });
    actions.Children().Append(all);
    auto common = StableInlinePathActionButton(
        L"常用",
        kFluentIconSparkleCircle20RegularPath,
        0.64,
        [checks]() {
          SetFuzzyPinyinRuleChecks(
              *checks,
              ParseFuzzyPinyinRuleSetting(kCommonFuzzyPinyinRules, false));
        });
    actions.Children().Append(common);
    auto clear = StableInlinePathActionButton(
        L"清空",
        kFluentIconDismissCircle20RegularPath,
        0.66,
        [checks]() {
          SetFuzzyPinyinRuleChecks(*checks, {});
        });
    actions.Children().Append(clear);
    Grid::SetColumn(actions, 1);
    built_in_header.Children().Append(actions);
    Grid::SetRow(built_in_header, 0);
    built_in_content.Children().Append(built_in_header);

    Grid rules_grid;
    rules_grid.ColumnSpacing(8);
    rules_grid.RowSpacing(0);
    ColumnDefinition left_rules_column;
    left_rules_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition right_rules_column;
    right_rules_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    rules_grid.ColumnDefinitions().Append(left_rules_column);
    rules_grid.ColumnDefinitions().Append(right_rules_column);
    for (int row_index = 0; row_index < 5; ++row_index) {
      RowDefinition row_definition;
      row_definition.Height(GridLengthHelper::FromPixels(kBuiltInRuleRowHeight));
      rules_grid.RowDefinitions().Append(row_definition);
    }

    for (size_t index = 0; index < FuzzyPinyinRuleDefinitions().size(); ++index) {
      const auto& rule = FuzzyPinyinRuleDefinitions()[index];
      Grid rule_cell;
      rule_cell.Height(kBuiltInRuleRowHeight);
      rule_cell.ColumnSpacing(6);
      rule_cell.HorizontalAlignment(HorizontalAlignment::Stretch);
      ColumnDefinition check_column;
      check_column.Width(GridLengthHelper::FromPixels(28));
      ColumnDefinition name_column;
      name_column.Width(GridLengthHelper::FromPixels(62));
      ColumnDefinition example_column;
      example_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
      rule_cell.ColumnDefinitions().Append(check_column);
      rule_cell.ColumnDefinitions().Append(name_column);
      rule_cell.ColumnDefinitions().Append(example_column);

      CheckBox check;
      ApplySettingsUIFont(check);
      check.MinWidth(0);
      check.MinHeight(0);
      check.Width(24);
      check.Height(24);
      check.Margin(Thickness{0, 0, 0, 0});
      check.VerticalAlignment(VerticalAlignment::Center);
      check.IsChecked(NullableBool(FuzzyPinyinRuleSelected(selected_rules, rule.id)));
      Grid::SetColumn(check, 0);
      rule_cell.Children().Append(check);
      checks->emplace_back(std::wstring(rule.id), check);

      auto name = Text(rule.name, 12, FW_SEMIBOLD);
      name.TextWrapping(TextWrapping::NoWrap);
      name.VerticalAlignment(VerticalAlignment::Center);
      Grid::SetColumn(name, 1);
      rule_cell.Children().Append(name);

      auto example = Text(rule.example, 11);
      example.Foreground(Brush(CurrentSettingsPalette().muted_text));
      example.TextWrapping(TextWrapping::NoWrap);
      example.VerticalAlignment(VerticalAlignment::Center);
      Grid::SetColumn(example, 2);
      rule_cell.Children().Append(example);

      Grid::SetColumn(rule_cell, static_cast<int>(index % 2));
      Grid::SetRow(rule_cell, static_cast<int>(index / 2));
      rules_grid.Children().Append(rule_cell);
    }
    rules_grid.Height(kBuiltInRulesHeight);
    Grid::SetRow(rules_grid, 1);
    built_in_content.Children().Append(rules_grid);

    Border table_frame = SettingsFrame(built_in_content,
                                       CurrentSettingsPalette().card,
                                       Radius(8),
                                       Thickness{10, 8, 10, 8});
    table_frame.HorizontalAlignment(HorizontalAlignment::Stretch);
    content.Children().Append(table_frame);

    Grid custom_content;
    custom_content.RowSpacing(6);
    RowDefinition custom_header_row;
    custom_header_row.Height(GridLengthHelper::Auto());
    RowDefinition custom_list_row;
    custom_list_row.Height(GridLengthHelper::Auto());
    custom_content.RowDefinitions().Append(custom_header_row);
    custom_content.RowDefinitions().Append(custom_list_row);

    Grid custom_header;
    custom_header.ColumnSpacing(12);
    custom_header.MinHeight(32);
    ColumnDefinition custom_title_column;
    custom_title_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition custom_add_column;
    custom_add_column.Width(GridLengthHelper::Auto());
    custom_header.ColumnDefinitions().Append(custom_title_column);
    custom_header.ColumnDefinitions().Append(custom_add_column);

    auto custom_title = Text(L"自定义规则", 13, FW_SEMIBOLD);
    custom_title.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(custom_title, 0);
    custom_header.Children().Append(custom_title);

    auto add_custom_row = std::make_shared<std::function<void(std::wstring, std::wstring)>>();
    auto add_custom = StableInlinePathActionButton(
        L"添加",
        kFluentIconAddCircle20RegularPath,
        0.66,
        [add_custom_row]() {
          if (*add_custom_row) {
            (*add_custom_row)(L"", L"");
          }
        });
    Grid::SetColumn(add_custom, 1);
    custom_header.Children().Append(add_custom);
    Grid::SetRow(custom_header, 0);
    custom_content.Children().Append(custom_header);

    StackPanel custom_rows_panel;
    custom_rows_panel.Spacing(6);
    custom_rows_panel.Margin(Thickness{0, 0, 16, 0});
    Grid custom_empty_state;
    custom_empty_state.Height(kCustomRulesHeight);
    custom_empty_state.HorizontalAlignment(HorizontalAlignment::Stretch);
    custom_empty_state.VerticalAlignment(VerticalAlignment::Stretch);
    StackPanel custom_empty_text;
    custom_empty_text.Spacing(4);
    custom_empty_text.HorizontalAlignment(HorizontalAlignment::Center);
    custom_empty_text.VerticalAlignment(VerticalAlignment::Center);
    auto custom_empty_title = Text(L"暂无自定义规则", 14, FW_SEMIBOLD);
    custom_empty_title.HorizontalTextAlignment(TextAlignment::Center);
    custom_empty_text.Children().Append(custom_empty_title);
    auto custom_empty_note = Text(L"点击“添加”创建一条规则。", 12);
    custom_empty_note.Foreground(SettingsSecondaryTextBrush());
    custom_empty_note.HorizontalTextAlignment(TextAlignment::Center);
    custom_empty_text.Children().Append(custom_empty_note);
    custom_empty_state.Children().Append(custom_empty_text);
    auto custom_rows = std::make_shared<std::vector<std::shared_ptr<CustomFuzzyPinyinRuleRow>>>();
    auto update_custom_empty_state = std::make_shared<std::function<void()>>();
    *update_custom_empty_state = [custom_rows, custom_empty_state]() {
      const bool has_visible_row =
          std::any_of(custom_rows->begin(), custom_rows->end(), [](const auto& row) {
            return row && !row->removed;
          });
      custom_empty_state.Visibility(has_visible_row ? Visibility::Collapsed : Visibility::Visible);
    };
    *add_custom_row = [custom_rows_panel, custom_rows, add_custom_row, update_custom_empty_state](
                          std::wstring left_value,
                          std::wstring right_value) {
      auto row_state = std::make_shared<CustomFuzzyPinyinRuleRow>();
      Grid row;
      row.MinHeight(32);
      row.ColumnSpacing(8);
      row.HorizontalAlignment(HorizontalAlignment::Stretch);
      ColumnDefinition left_column;
      left_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
      ColumnDefinition separator_column;
      separator_column.Width(GridLengthHelper::FromPixels(18));
      ColumnDefinition right_column;
      right_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
      ColumnDefinition remove_column;
      remove_column.Width(GridLengthHelper::FromPixels(36));
      row.ColumnDefinitions().Append(left_column);
      row.ColumnDefinitions().Append(separator_column);
      row.ColumnDefinitions().Append(right_column);
      row.ColumnDefinitions().Append(remove_column);

      TextBox left;
      ApplySettingsUIFont(left);
      left.MinWidth(88);
      left.MinHeight(32);
      left.Height(32);
      left.HorizontalAlignment(HorizontalAlignment::Stretch);
      left.PlaceholderText(L"拼音 A");
      left.Text(left_value);
      SetSettingsToolTip(left, L"例如 n");
      Grid::SetColumn(left, 0);
      row.Children().Append(left);

      auto separator = Text(L"=", 14, FW_SEMIBOLD);
      separator.Width(18);
      separator.HorizontalTextAlignment(TextAlignment::Center);
      separator.VerticalAlignment(VerticalAlignment::Center);
      Grid::SetColumn(separator, 1);
      row.Children().Append(separator);

      TextBox right;
      ApplySettingsUIFont(right);
      right.MinWidth(88);
      right.MinHeight(32);
      right.Height(32);
      right.HorizontalAlignment(HorizontalAlignment::Stretch);
      right.PlaceholderText(L"拼音 B");
      right.Text(right_value);
      SetSettingsToolTip(right, L"例如 l");
      Grid::SetColumn(right, 2);
      row.Children().Append(right);

      row_state->row = row;
      row_state->left = left;
      row_state->right = right;
      auto remove = StableIconToolButton(
          kFluentIconDelete20RegularPath,
          L"删除",
          0.82,
          [custom_rows_panel, row_state, update_custom_empty_state]() {
        row_state->removed = true;
        const auto children = custom_rows_panel.Children();
        for (uint32_t index = 0; index < children.Size(); ++index) {
          if (children.GetAt(index) == row_state->row) {
            children.RemoveAt(index);
            break;
          }
        }
        (*update_custom_empty_state)();
          },
          34,
          32);
      Grid::SetColumn(remove, 3);
      row.Children().Append(remove);

      custom_rows->push_back(row_state);
      custom_rows_panel.Children().Append(row);
      (*update_custom_empty_state)();
    };

    for (const auto& rule : ParseFuzzyPinyinCustomRuleSetting(
             ReadStringSetting(L"fuzzy_pinyin_custom_rules", L""))) {
      const auto [left, right] = SplitFuzzyPinyinCustomRule(rule);
      (*add_custom_row)(left, right);
    }
    (*update_custom_empty_state)();

    Grid custom_list_host;
    custom_list_host.Height(kCustomRulesHeight);
    ScrollViewer custom_scroller;
    custom_scroller.Content(custom_rows_panel);
    custom_scroller.Height(kCustomRulesHeight);
    custom_scroller.MinHeight(kCustomRulesHeight);
    custom_scroller.MaxHeight(kCustomRulesHeight);
    custom_scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    custom_scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    custom_scroller.VerticalScrollMode(ScrollMode::Enabled);
    custom_scroller.HorizontalScrollMode(ScrollMode::Disabled);
    custom_scroller.ZoomMode(ZoomMode::Disabled);
    custom_scroller.BringIntoViewOnFocusChange(false);
    custom_list_host.Children().Append(custom_scroller);
    custom_list_host.Children().Append(custom_empty_state);
    Grid::SetRow(custom_list_host, 1);
    custom_content.Children().Append(custom_list_host);
    Border custom_frame = SettingsFrame(custom_content,
                                        CurrentSettingsPalette().card,
                                        Radius(8),
                                        Thickness{10, 8, 10, 8});
    custom_frame.HorizontalAlignment(HorizontalAlignment::Stretch);
    content.Children().Append(custom_frame);

    *update_visible_panel = [table_frame, custom_frame, mode_index]() {
      const bool show_custom = *mode_index == 1;
      table_frame.Visibility(show_custom ? Visibility::Collapsed : Visibility::Visible);
      custom_frame.Visibility(show_custom ? Visibility::Visible : Visibility::Collapsed);
    };
    (*update_visible_panel)();

    dialog.Content(content);
    auto operation = dialog.ShowAsync();
    operation.Completed([this,
                         dialog,
                         checks,
                         custom_rows,
                         fuzzy_switch,
                         state_icon,
                         require_enabled,
                         suppress_toggle_dialog](auto const& async, auto const& status) {
      if (status != Windows::Foundation::AsyncStatus::Completed ||
          async.GetResults() != ContentDialogResult::Primary) {
        if (require_enabled) {
          if (suppress_toggle_dialog) {
            *suppress_toggle_dialog = true;
          }
          fuzzy_switch.IsOn(false);
          SetIconChild(state_icon, FuzzyPinyinRulesStateIcon(false));
          if (suppress_toggle_dialog) {
            *suppress_toggle_dialog = false;
          }
          WriteBoolSetting(L"fuzzy_pinyin", false);
        }
        return;
      }

      std::vector<std::wstring> selected;
      for (const auto& [id, check] : *checks) {
        if (IsChecked(check)) {
          selected.push_back(id);
        }
      }

      std::vector<std::wstring> custom_rules;
      for (const auto& row : *custom_rows) {
        if (!row || row->removed) {
          continue;
        }
        std::wstring pair_text = std::wstring(row->left.Text()) + L"=" +
                                 std::wstring(row->right.Text());
        std::wstring normalized = NormalizeFuzzyPinyinCustomRuleToken(pair_text);
        if (!normalized.empty() &&
            std::find(custom_rules.begin(), custom_rules.end(), normalized) ==
                custom_rules.end()) {
          custom_rules.push_back(normalized);
        }
      }

      if (selected.empty() && custom_rules.empty()) {
        WriteStringSetting(L"fuzzy_pinyin_rules", L"");
        WriteStringSetting(L"fuzzy_pinyin_custom_rules", L"");
        if (suppress_toggle_dialog) {
          *suppress_toggle_dialog = true;
        }
        fuzzy_switch.IsOn(false);
        SetIconChild(state_icon, FuzzyPinyinRulesStateIcon(false));
        if (suppress_toggle_dialog) {
          *suppress_toggle_dialog = false;
        }
        WriteBoolSetting(L"fuzzy_pinyin", false);
        RequestApplyInputConfigDeferred();
        return;
      }

      WriteStringSetting(L"fuzzy_pinyin_rules", JoinFuzzyPinyinRules(selected));
      WriteStringSetting(L"fuzzy_pinyin_custom_rules",
                         JoinFuzzyPinyinCustomRules(custom_rules, L','));
      if (require_enabled || fuzzy_switch.IsOn()) {
        if (suppress_toggle_dialog) {
          *suppress_toggle_dialog = true;
        }
        fuzzy_switch.IsOn(true);
        SetIconChild(state_icon, FuzzyPinyinRulesStateIcon(true));
        if (suppress_toggle_dialog) {
          *suppress_toggle_dialog = false;
        }
        WriteBoolSetting(L"fuzzy_pinyin", true);
        RequestApplyInputConfigDeferred();
      }
    });
  }

  void ShowStatusTipBlacklistDialog(TextBlock const& count_icon) {
    ContentDialog dialog;
    ApplySettingsDialogBase(dialog, window_.Content().as<FrameworkElement>().XamlRoot());
    dialog.PrimaryButtonText(L"完成");
    dialog.CloseButtonText(L"取消");
    dialog.DefaultButton(ContentDialogButton::Primary);
    ApplySettingsResources(dialog.Resources());
    dialog.Resources().Insert(box_value(L"ContentDialogPadding"),
                              box_value(Thickness{24, 16, 24, 8}));
    dialog.Resources().Insert(box_value(L"ContentDialogCommandSpaceMargin"),
                              box_value(Thickness{0, 6, 0, 0}));
    dialog.Resources().Insert(box_value(L"ContentDialogTitleMargin"),
                              box_value(Thickness{0, 0, 0, 10}));

    StackPanel content;
    content.Width(kSettingsDialogContentWidth);
    content.MaxWidth(kSettingsDialogContentWidth);
    content.Spacing(7);
    content.HorizontalAlignment(HorizontalAlignment::Stretch);
    content.Children().Append(Text(L"状态提示黑名单", 20, FW_SEMIBOLD));

    auto description = Text(L"每行一个进程名或通配符。", 12);
    description.Foreground(SettingsSecondaryTextBrush());
    content.Children().Append(description);

    Grid header;
    header.ColumnSpacing(12);
    header.MinHeight(32);
    ColumnDefinition title_column;
    title_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition actions_column;
    actions_column.Width(GridLengthHelper::Auto());
    header.ColumnDefinitions().Append(title_column);
    header.ColumnDefinitions().Append(actions_column);

    auto title = Text(L"进程列表", 13, FW_SEMIBOLD);
    title.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(title, 0);
    header.Children().Append(title);

    StackPanel actions;
    actions.Orientation(Orientation::Horizontal);
    actions.Spacing(4);
    actions.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(actions, 1);
    header.Children().Append(actions);

    constexpr double kBlacklistRowsHeight = kSettingsDialogListHeight;
    Grid list_content;
    list_content.RowSpacing(6);
    RowDefinition list_header_row;
    list_header_row.Height(GridLengthHelper::Auto());
    RowDefinition list_body_row;
    list_body_row.Height(GridLengthHelper::FromPixels(kBlacklistRowsHeight));
    list_content.RowDefinitions().Append(list_header_row);
    list_content.RowDefinitions().Append(list_body_row);
    Grid::SetRow(header, 0);
    list_content.Children().Append(header);

    Grid list_host;
    list_host.Height(kBlacklistRowsHeight);
    list_host.HorizontalAlignment(HorizontalAlignment::Stretch);

    StackPanel rows_panel;
    rows_panel.Spacing(6);
    rows_panel.Margin(Thickness{0, 0, 16, 0});

    Grid empty_state;
    empty_state.Height(kBlacklistRowsHeight);
    empty_state.HorizontalAlignment(HorizontalAlignment::Stretch);
    empty_state.VerticalAlignment(VerticalAlignment::Stretch);
    StackPanel empty_text;
    empty_text.Spacing(4);
    empty_text.HorizontalAlignment(HorizontalAlignment::Center);
    empty_text.VerticalAlignment(VerticalAlignment::Center);
    auto empty_title = Text(L"暂无黑名单项目", 14, FW_SEMIBOLD);
    empty_title.HorizontalTextAlignment(TextAlignment::Center);
    empty_text.Children().Append(empty_title);
    auto empty_note = Text(L"点击“添加”创建一条进程规则。", 12);
    empty_note.Foreground(SettingsSecondaryTextBrush());
    empty_note.HorizontalTextAlignment(TextAlignment::Center);
    empty_text.Children().Append(empty_note);
    empty_state.Children().Append(empty_text);

    auto rows = std::make_shared<std::vector<std::shared_ptr<StatusTipBlacklistRow>>>();
    auto update_empty_state = std::make_shared<std::function<void()>>();
    *update_empty_state = [rows, empty_state]() {
      const bool has_visible_row =
          std::any_of(rows->begin(), rows->end(), [](const auto& row) {
            return row && !row->removed;
          });
      empty_state.Visibility(has_visible_row ? Visibility::Collapsed : Visibility::Visible);
    };

    auto add_blacklist_row = std::make_shared<std::function<void(std::wstring)>>();
    *add_blacklist_row =
        [rows_panel, rows, update_empty_state](std::wstring process_value) {
      auto row_state = std::make_shared<StatusTipBlacklistRow>();
      Grid row;
      row.MinHeight(32);
      row.ColumnSpacing(8);
      row.HorizontalAlignment(HorizontalAlignment::Stretch);
      ColumnDefinition value_column;
      value_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
      ColumnDefinition remove_column;
      remove_column.Width(GridLengthHelper::FromPixels(36));
      row.ColumnDefinitions().Append(value_column);
      row.ColumnDefinitions().Append(remove_column);

      TextBox process;
      ApplySettingsUIFont(process);
      process.MinWidth(220);
      process.MinHeight(32);
      process.Height(32);
      process.HorizontalAlignment(HorizontalAlignment::Stretch);
      process.PlaceholderText(L"process.exe 或 *.exe");
      process.Text(process_value);
      SetSettingsToolTip(process, L"例如 explorer.exe 或 *.exe");
      Grid::SetColumn(process, 0);
      row.Children().Append(process);

      row_state->row = row;
      row_state->process = process;
      auto remove = StableIconToolButton(
          kFluentIconDelete20RegularPath,
          L"删除",
          0.82,
          [rows_panel, row_state, update_empty_state]() {
        row_state->removed = true;
        const auto children = rows_panel.Children();
        for (uint32_t index = 0; index < children.Size(); ++index) {
          if (children.GetAt(index) == row_state->row) {
            children.RemoveAt(index);
            break;
          }
        }
        (*update_empty_state)();
          },
          34,
          32);
      Grid::SetColumn(remove, 1);
      row.Children().Append(remove);

      rows->push_back(row_state);
      rows_panel.Children().Append(row);
      (*update_empty_state)();
        };

    auto clear_rows = std::make_shared<std::function<void()>>();
    *clear_rows = [rows_panel, rows, update_empty_state]() {
      for (const auto& row : *rows) {
        if (row) {
          row->removed = true;
        }
      }
      rows_panel.Children().Clear();
      (*update_empty_state)();
    };

    actions.Children().Append(StableInlinePathActionButton(
        L"默认",
        kFluentIconArrowClockwise20RegularPath,
        0.66,
        [clear_rows, add_blacklist_row]() {
          (*clear_rows)();
          for (const auto& item : ParseStatusTipBlacklistSetting(kDefaultStatusTipBlacklist)) {
            (*add_blacklist_row)(item);
          }
        }));
    actions.Children().Append(StableInlinePathActionButton(
        L"清空",
        kFluentIconDismissCircle20RegularPath,
        0.66,
        [clear_rows]() {
          (*clear_rows)();
        }));
    actions.Children().Append(StableInlinePathActionButton(
        L"添加",
        kFluentIconAddCircle20RegularPath,
        0.66,
        [add_blacklist_row]() {
          (*add_blacklist_row)(L"");
        }));

    for (const auto& item : CurrentStatusTipBlacklistItems()) {
      (*add_blacklist_row)(item);
    }
    (*update_empty_state)();

    ScrollViewer scroller;
    scroller.Content(rows_panel);
    scroller.Height(kBlacklistRowsHeight);
    scroller.MinHeight(kBlacklistRowsHeight);
    scroller.MaxHeight(kBlacklistRowsHeight);
    scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    scroller.VerticalScrollMode(ScrollMode::Enabled);
    scroller.HorizontalScrollMode(ScrollMode::Disabled);
    scroller.ZoomMode(ZoomMode::Disabled);
    scroller.BringIntoViewOnFocusChange(false);
    list_host.Children().Append(scroller);
    list_host.Children().Append(empty_state);
    Grid::SetRow(list_host, 1);
    list_content.Children().Append(list_host);

    Border list_frame = SettingsFrame(list_content,
                                      CurrentSettingsPalette().card,
                                      Radius(8),
                                      Thickness{10, 8, 10, 8});
    list_frame.HorizontalAlignment(HorizontalAlignment::Stretch);
    content.Children().Append(list_frame);

    dialog.Content(content);
    auto operation = dialog.ShowAsync();
    operation.Completed([dialog, rows, count_icon](auto const& async, auto const& status) {
      if (status != Windows::Foundation::AsyncStatus::Completed ||
          async.GetResults() != ContentDialogResult::Primary) {
        return;
      }

      std::vector<std::wstring> items;
      for (const auto& row : *rows) {
        if (!row || row->removed) {
          continue;
        }
        const std::wstring normalized =
            NormalizeStatusTipBlacklistToken(std::wstring(row->process.Text()));
        if (!normalized.empty() && !StatusTipBlacklistContains(items, normalized)) {
          items.push_back(normalized);
        }
      }

      WriteStringSetting(L"status_tip_blacklist", JoinStatusTipBlacklistItems(items));
      count_icon.Text(std::to_wstring(items.size()));
      RequestInputStateRefreshDeferred();
    });
  }

  void ShowSyncMessage(std::wstring_view title, std::wstring_view message, UINT flags = MB_OK) {
    MessageBoxW(g_settings_window_hwnd,
                std::wstring(message).c_str(),
                std::wstring(title).c_str(),
                flags);
  }

  fp::sync::SyncConfig CurrentSyncConfigFromSettings() {
    ResetSettingsCache();
    return fp::sync::LoadConfig(SettingsPath());
  }

  void SaveProtectedSyncSetting(std::wstring_view key, std::wstring_view value) {
    if (auto protected_value = fp::sync::ProtectSecretText(value)) {
      WriteStringSetting(std::wstring(key) + L"_protected", *protected_value);
      WriteStringSetting(key, L"");
    } else {
      WriteStringSetting(key, value);
    }
  }

  void ApplyAutoSyncSchedule(bool enabled, int interval_minutes) {
    WriteBoolSetting(L"sync_auto_enabled", enabled);
    WriteIntSetting(L"sync_auto_interval_minutes", interval_minutes);
    const auto result = enabled
                            ? fp::sync::InstallScheduledSync(SiblingExe(L"fluent-pinyin-settings.exe"),
                                                             interval_minutes)
                            : fp::sync::RemoveScheduledSync();
    if (!result.success) {
      ShowSyncMessage(L"定时同步", result.message, MB_OK | MB_ICONERROR);
    }
  }

  void RunSyncActionAsync(std::wstring title,
                          std::function<fp::sync::SyncResult()> action,
                          bool refresh_input_config) {
    auto hwnd = g_settings_window_hwnd;
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

  TextBox SyncDialogTextBox(std::wstring_view text,
                            std::wstring_view placeholder,
                            bool password = false) {
    (void)password;
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

  Border SyncDialogSubPanel(StackPanel const& panel) {
    return SettingsFrame(panel,
                         CurrentSettingsPalette().card,
                         Radius(8),
                         Thickness{12, 10, 12, 10});
  }

  void ShowSyncProviderDialog() {
    ContentDialog dialog;
    ApplySettingsDialogBase(dialog, window_.Content().as<FrameworkElement>().XamlRoot());
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
    auto object_key = SyncDialogTextBox(config.object_key.empty() ? fp::sync::DefaultObjectKey()
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
    auto webdav_key = SyncDialogTextBox(config.object_key.empty() ? fp::sync::DefaultObjectKey()
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
      WriteStringSetting(L"sync_provider", webdav ? L"webdav" : L"object");
      WriteStringSetting(L"sync_object_endpoint", object_endpoint.Text().c_str());
      WriteStringSetting(L"sync_object_bucket", object_bucket.Text().c_str());
      WriteStringSetting(L"sync_object_region", object_region.Text().c_str());
      WriteStringSetting(L"sync_object_access_key", object_access_key.Text().c_str());
      SaveProtectedSyncSetting(L"sync_object_secret_key", object_secret_key.Password().c_str());
      WriteStringSetting(L"sync_webdav_url", webdav_url.Text().c_str());
      WriteStringSetting(L"sync_webdav_username", webdav_username.Text().c_str());
      SaveProtectedSyncSetting(L"sync_webdav_password", webdav_password.Password().c_str());
      WriteStringSetting(L"sync_object_key",
                         webdav ? std::wstring(webdav_key.Text()) : std::wstring(object_key.Text()));
      SaveProtectedSyncSetting(L"sync_encryption_secret", encryption_secret.Password().c_str());
      ResetSettingsCache();
    });
  }

  void ShowManagedDictionariesDialog(Grid const& state_icon) {
    ContentDialog dialog;
    ApplySettingsDialogBase(dialog, window_.Content().as<FrameworkElement>().XamlRoot());
    dialog.PrimaryButtonText(L"完成");
    dialog.CloseButtonText(L"取消");
    dialog.DefaultButton(ContentDialogButton::Primary);
    ApplySettingsResources(dialog.Resources());
    dialog.Resources().Insert(box_value(L"ContentDialogPadding"),
                              box_value(Thickness{24, 16, 24, 8}));
    dialog.Resources().Insert(box_value(L"ContentDialogCommandSpaceMargin"),
                              box_value(Thickness{0, 6, 0, 0}));
    dialog.Resources().Insert(box_value(L"ContentDialogTitleMargin"),
                              box_value(Thickness{0, 0, 0, 10}));

    StackPanel content;
    content.Width(kSettingsDialogContentWidth);
    content.MaxWidth(kSettingsDialogContentWidth);
    content.Spacing(7);
    content.HorizontalAlignment(HorizontalAlignment::Stretch);
    content.Children().Append(Text(L"导入词库", 20, FW_SEMIBOLD));

    auto description = Text(L"管理已导入的词库文件。", 12);
    description.Foreground(SettingsSecondaryTextBrush());
    content.Children().Append(description);

    auto rows = std::make_shared<std::vector<std::shared_ptr<ManagedDictionaryRow>>>();
    StackPanel rows_panel;
    rows_panel.Spacing(6);
    rows_panel.Margin(Thickness{0, 0, 16, 0});

    Grid empty_state;
    empty_state.Height(kSettingsDialogListHeight);
    empty_state.HorizontalAlignment(HorizontalAlignment::Stretch);
    empty_state.VerticalAlignment(VerticalAlignment::Stretch);
    StackPanel empty_text;
    empty_text.Spacing(4);
    empty_text.HorizontalAlignment(HorizontalAlignment::Center);
    empty_text.VerticalAlignment(VerticalAlignment::Center);
    auto empty_title = Text(L"暂无导入词库", 14, FW_SEMIBOLD);
    empty_title.HorizontalTextAlignment(TextAlignment::Center);
    empty_text.Children().Append(empty_title);
    auto empty_note = Text(L"点击“导入”选择 .dict.yaml 文件。", 12);
    empty_note.Foreground(SettingsSecondaryTextBrush());
    empty_note.HorizontalTextAlignment(TextAlignment::Center);
    empty_text.Children().Append(empty_note);
    empty_state.Children().Append(empty_text);

    auto update_empty_state = std::make_shared<std::function<void()>>();
    *update_empty_state = [rows, empty_state]() {
      const bool has_visible_row =
          std::any_of(rows->begin(), rows->end(), [](const auto& row) {
            return row && !row->removed;
          });
      empty_state.Visibility(has_visible_row ? Visibility::Collapsed : Visibility::Visible);
    };

    auto add_dictionary_row =
        std::make_shared<std::function<void(std::string, bool)>>();
    *add_dictionary_row = [rows_panel, rows, update_empty_state](
                              std::string name,
                              bool enabled) {
      auto row_state = std::make_shared<ManagedDictionaryRow>();
      row_state->name = name;

      Grid row;
      row.MinHeight(36);
      row.ColumnSpacing(8);
      row.HorizontalAlignment(HorizontalAlignment::Stretch);
      ColumnDefinition name_column;
      name_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
      ColumnDefinition switch_column;
      switch_column.Width(GridLengthHelper::FromPixels(70));
      ColumnDefinition remove_column;
      remove_column.Width(GridLengthHelper::FromPixels(36));
      row.ColumnDefinitions().Append(name_column);
      row.ColumnDefinitions().Append(switch_column);
      row.ColumnDefinitions().Append(remove_column);

      auto label = Text(fp::Utf8ToWide(name), 13, FW_SEMIBOLD);
      label.TextWrapping(TextWrapping::NoWrap);
      label.TextTrimming(TextTrimming::CharacterEllipsis);
      label.VerticalAlignment(VerticalAlignment::Center);
      SetSettingsToolTip(label, fp::Utf8ToWide(name));
      Grid::SetColumn(label, 0);
      row.Children().Append(label);

      ToggleSwitch toggle;
      ApplySettingsUIFont(toggle);
      toggle.MinWidth(0);
      toggle.IsOn(enabled);
      toggle.HorizontalAlignment(HorizontalAlignment::Right);
      toggle.VerticalAlignment(VerticalAlignment::Center);
      row_state->toggle = toggle;
      Grid::SetColumn(toggle, 1);
      row.Children().Append(toggle);

      row_state->row = row;
      auto remove = StableIconToolButton(
          kFluentIconDelete20RegularPath,
          L"删除",
          0.82,
          [rows_panel, row_state, update_empty_state]() {
        row_state->removed = true;
        const auto children = rows_panel.Children();
        for (uint32_t index = 0; index < children.Size(); ++index) {
          if (children.GetAt(index) == row_state->row) {
            children.RemoveAt(index);
            break;
          }
        }
        (*update_empty_state)();
          },
          34,
          32);
      Grid::SetColumn(remove, 2);
      row.Children().Append(remove);

      rows->push_back(row_state);
      rows_panel.Children().Append(row);
      (*update_empty_state)();
    };

    for (const auto& entry : ReadManagedDictionaryManifest()) {
      (*add_dictionary_row)(entry.name, entry.enabled);
    }
    (*update_empty_state)();

    Grid header;
    header.ColumnSpacing(12);
    header.MinHeight(34);
    ColumnDefinition title_column;
    title_column.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition add_column;
    add_column.Width(GridLengthHelper::Auto());
    header.ColumnDefinitions().Append(title_column);
    header.ColumnDefinitions().Append(add_column);

    auto title = Text(L"已导入词库", 13, FW_SEMIBOLD);
    title.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(title, 0);
    header.Children().Append(title);

    auto import_button = StableInlinePathActionButton(
        L"导入",
        kFluentIconArrowImport20RegularPath,
        0.66,
        [this, rows, rows_panel, add_dictionary_row, update_empty_state, state_icon]() {
      const auto selected = PickRimeDictionaryFile(g_settings_window_hwnd);
      if (!selected) {
        return;
      }

      std::wstring error;
      if (!ImportManagedDictionary(*selected, &error)) {
        MessageBoxW(g_settings_window_hwnd,
                    error.empty() ? L"导入词库失败。" : error.c_str(),
                    L"流畅拼音 设置",
                    MB_ICONWARNING);
        return;
      }

      rows->clear();
      rows_panel.Children().Clear();
      for (const auto& entry : ReadManagedDictionaryManifest()) {
        (*add_dictionary_row)(entry.name, entry.enabled);
      }
      (*update_empty_state)();
      SetIconChild(state_icon,
                   ImportedLexiconsStateIcon(ReadBoolSetting(L"imported_lexicons_enabled", true)));
      RequestApplyInputConfigDeferred();
        });
    Grid::SetColumn(import_button, 1);
    header.Children().Append(import_button);

    Grid list_host;
    list_host.Height(kSettingsDialogListHeight);
    ScrollViewer scroller;
    scroller.Content(rows_panel);
    scroller.Height(kSettingsDialogListHeight);
    scroller.MinHeight(kSettingsDialogListHeight);
    scroller.MaxHeight(kSettingsDialogListHeight);
    scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    scroller.VerticalScrollMode(ScrollMode::Enabled);
    scroller.HorizontalScrollMode(ScrollMode::Disabled);
    scroller.ZoomMode(ZoomMode::Disabled);
    scroller.BringIntoViewOnFocusChange(false);
    list_host.Children().Append(scroller);
    list_host.Children().Append(empty_state);

    Grid table;
    table.RowSpacing(6);
    RowDefinition header_row;
    header_row.Height(GridLengthHelper::Auto());
    RowDefinition list_row;
    list_row.Height(GridLengthHelper::Auto());
    table.RowDefinitions().Append(header_row);
    table.RowDefinitions().Append(list_row);
    Grid::SetRow(header, 0);
    table.Children().Append(header);
    Grid::SetRow(list_host, 1);
    table.Children().Append(list_host);

    Border table_frame = SettingsFrame(table,
                                       CurrentSettingsPalette().card,
                                       Radius(8),
                                       Thickness{10, 8, 10, 8});
    table_frame.HorizontalAlignment(HorizontalAlignment::Stretch);
    content.Children().Append(table_frame);

    dialog.Content(content);
    auto operation = dialog.ShowAsync();
    operation.Completed([rows, state_icon](auto const& async, auto const& status) {
      if (status != Windows::Foundation::AsyncStatus::Completed ||
          async.GetResults() != ContentDialogResult::Primary) {
        return;
      }

      std::vector<ManagedDictionaryEntry> entries;
      for (const auto& row : *rows) {
        if (!row) {
          continue;
        }
        if (row->removed) {
          RemoveManagedDictionaryFile(row->name);
          continue;
        }
        if (IsSafeRimeDictName(row->name)) {
          entries.push_back({row->name, row->toggle.IsOn()});
        }
      }
      WriteManagedDictionaryIntegrationFiles(entries);
      SetIconChild(state_icon,
                   ImportedLexiconsStateIcon(ReadBoolSetting(L"imported_lexicons_enabled", true)));
      RequestApplyInputConfigDeferred();
    });
  }

  void ShowPhraseLexiconDialog(std::wstring_view title_text,
                               std::wstring_view description_text,
                               bool user_lexicon,
                               Grid const& state_icon) {
    ContentDialog dialog;
    ApplySettingsDialogBase(dialog, window_.Content().as<FrameworkElement>().XamlRoot());
    dialog.PrimaryButtonText(L"完成");
    dialog.CloseButtonText(L"取消");
    dialog.DefaultButton(ContentDialogButton::Primary);
    ApplySettingsResources(dialog.Resources());
    dialog.Resources().Insert(box_value(L"ContentDialogPadding"),
                              box_value(Thickness{24, 16, 24, 8}));
    dialog.Resources().Insert(box_value(L"ContentDialogCommandSpaceMargin"),
                              box_value(Thickness{0, 6, 0, 0}));
    dialog.Resources().Insert(box_value(L"ContentDialogTitleMargin"),
                              box_value(Thickness{0, 0, 0, 10}));

    StackPanel content;
    content.Width(kSettingsDialogContentWidth);
    content.MaxWidth(kSettingsDialogContentWidth);
    content.Spacing(7);
    content.HorizontalAlignment(HorizontalAlignment::Stretch);
    content.Children().Append(Text(title_text, 20, FW_SEMIBOLD));
    auto description = Text(description_text, 12);
    description.Foreground(SettingsSecondaryTextBrush());
    content.Children().Append(description);

    StackPanel rows_panel;
    rows_panel.Spacing(6);
    rows_panel.Margin(Thickness{0, 0, 16, 0});
    auto rows = std::make_shared<std::vector<std::shared_ptr<LexiconPhraseRow>>>();
    auto update_empty_state = std::make_shared<std::function<void()>>();
    Grid empty_state;
    empty_state.Height(kSettingsDialogListHeight);
    empty_state.HorizontalAlignment(HorizontalAlignment::Stretch);
    empty_state.VerticalAlignment(VerticalAlignment::Stretch);
    StackPanel empty_text;
    empty_text.Spacing(4);
    empty_text.HorizontalAlignment(HorizontalAlignment::Center);
    empty_text.VerticalAlignment(VerticalAlignment::Center);
    auto empty_title = Text(L"暂无内容", 14, FW_SEMIBOLD);
    empty_title.HorizontalTextAlignment(TextAlignment::Center);
    empty_text.Children().Append(empty_title);
    auto empty_note = Text(L"点击“添加”创建一条记录。", 12);
    empty_note.Foreground(SettingsSecondaryTextBrush());
    empty_note.HorizontalTextAlignment(TextAlignment::Center);
    empty_text.Children().Append(empty_note);
    empty_state.Children().Append(empty_text);

    *update_empty_state = [rows, empty_state]() {
      const bool has_visible_row =
          std::any_of(rows->begin(), rows->end(), [](const auto& row) {
            return row && !row->removed;
          });
      empty_state.Visibility(has_visible_row ? Visibility::Collapsed : Visibility::Visible);
    };

    auto add_row = std::make_shared<std::function<void(PhraseEntry)>>();
    *add_row = [rows_panel, rows, update_empty_state](PhraseEntry entry) {
      auto row_state = std::make_shared<LexiconPhraseRow>();
      Grid row;
      row.MinHeight(32);
      row.ColumnSpacing(8);
      row.HorizontalAlignment(HorizontalAlignment::Stretch);
      const double weights[] = {1.4, 1.0};
      for (int index = 0; index < 4; ++index) {
        ColumnDefinition column;
        if (index < 2) {
          column.Width(GridLengthHelper::FromValueAndType(weights[index], GridUnitType::Star));
        } else {
          column.Width(GridLengthHelper::FromPixels(index == 2 ? 70 : 36));
        }
        row.ColumnDefinitions().Append(column);
      }

      TextBox phrase;
      ApplySettingsUIFont(phrase);
      phrase.MinWidth(130);
      phrase.MinHeight(32);
      phrase.Height(32);
      phrase.PlaceholderText(L"词条");
      phrase.Text(entry.phrase);
      phrase.HorizontalAlignment(HorizontalAlignment::Stretch);
      Grid::SetColumn(phrase, 0);
      row.Children().Append(phrase);

      TextBox code;
      ApplySettingsUIFont(code);
      code.MinWidth(96);
      code.MinHeight(32);
      code.Height(32);
      code.PlaceholderText(L"编码");
      code.Text(entry.code);
      code.HorizontalAlignment(HorizontalAlignment::Stretch);
      Grid::SetColumn(code, 1);
      row.Children().Append(code);

      TextBox weight;
      ApplySettingsUIFont(weight);
      weight.MinWidth(58);
      weight.MinHeight(32);
      weight.Height(32);
      weight.PlaceholderText(L"权重");
      weight.Text(NormalizePhraseWeight(entry.weight));
      weight.HorizontalAlignment(HorizontalAlignment::Stretch);
      Grid::SetColumn(weight, 2);
      row.Children().Append(weight);

      row_state->row = row;
      row_state->phrase = phrase;
      row_state->code = code;
      row_state->weight = weight;
      auto remove = StableIconToolButton(
          kFluentIconDelete20RegularPath,
          L"删除",
          0.82,
          [rows_panel, row_state, update_empty_state]() {
        row_state->removed = true;
        const auto children = rows_panel.Children();
        for (uint32_t index = 0; index < children.Size(); ++index) {
          if (children.GetAt(index) == row_state->row) {
            children.RemoveAt(index);
            break;
          }
        }
        (*update_empty_state)();
          },
          34,
          32);
      Grid::SetColumn(remove, 3);
      row.Children().Append(remove);

      rows->push_back(row_state);
      rows_panel.Children().Append(row);
      (*update_empty_state)();
    };

    auto add_button = StableInlinePathActionButton(
        L"添加",
        kFluentIconAddCircle20RegularPath,
        0.66,
        [add_row]() {
      if (*add_row) {
        (*add_row)(PhraseEntry{});
      }
        });

    Grid header;
    header.ColumnSpacing(12);
    header.MinHeight(34);
    ColumnDefinition header_title;
    header_title.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    ColumnDefinition header_action;
    header_action.Width(GridLengthHelper::Auto());
    header.ColumnDefinitions().Append(header_title);
    header.ColumnDefinitions().Append(header_action);
    auto table_title = Text(user_lexicon ? L"用户词条" : L"自定义短语", 13, FW_SEMIBOLD);
    table_title.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(table_title, 0);
    header.Children().Append(table_title);
    Grid::SetColumn(add_button, 1);
    header.Children().Append(add_button);

    for (const auto& entry : (user_lexicon ? ReadUserLexiconEntries()
                                           : ReadCustomPhraseEntries())) {
      (*add_row)(entry);
    }
    (*update_empty_state)();

    Grid list_host;
    list_host.Height(kSettingsDialogListHeight);
    ScrollViewer scroller;
    scroller.Content(rows_panel);
    scroller.Height(kSettingsDialogListHeight);
    scroller.MinHeight(kSettingsDialogListHeight);
    scroller.MaxHeight(kSettingsDialogListHeight);
    scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    scroller.VerticalScrollMode(ScrollMode::Enabled);
    scroller.HorizontalScrollMode(ScrollMode::Disabled);
    scroller.ZoomMode(ZoomMode::Disabled);
    scroller.BringIntoViewOnFocusChange(false);
    list_host.Children().Append(scroller);
    list_host.Children().Append(empty_state);

    Grid table;
    table.RowSpacing(6);
    for (int index = 0; index < 2; ++index) {
      RowDefinition row;
      row.Height(GridLengthHelper::Auto());
      table.RowDefinitions().Append(row);
    }
    Grid::SetRow(header, 0);
    table.Children().Append(header);
    Grid::SetRow(list_host, 1);
    table.Children().Append(list_host);
    content.Children().Append(SettingsFrame(table,
                                           CurrentSettingsPalette().card,
                                           Radius(8),
                                           Thickness{10, 8, 10, 8}));

    dialog.Content(content);
    auto operation = dialog.ShowAsync();
    operation.Completed([rows, user_lexicon, state_icon](auto const& async, auto const& status) {
      if (status != Windows::Foundation::AsyncStatus::Completed ||
          async.GetResults() != ContentDialogResult::Primary) {
        return;
      }
      std::vector<PhraseEntry> entries;
      for (const auto& row : *rows) {
        if (!row || row->removed) {
          continue;
        }
        PhraseEntry entry;
        entry.phrase = TrimLexiconText(row->phrase.Text().c_str());
        entry.code = TrimLexiconText(row->code.Text().c_str());
        entry.weight = NormalizePhraseWeight(row->weight.Text().c_str());
        if (!entry.phrase.empty() && !entry.code.empty()) {
          entries.push_back(std::move(entry));
        }
      }
      if (user_lexicon) {
        WriteUserLexiconEntries(entries);
        WriteManagedDictionaryIntegrationFiles(ReadManagedDictionaryManifest());
        SetIconChild(state_icon,
                     UserLexiconStateIcon(ReadBoolSetting(L"user_lexicon_enabled", true)));
      } else {
        WriteCustomPhraseEntries(entries);
        SetIconChild(state_icon,
                     CustomPhrasesStateIcon(ReadBoolSetting(L"custom_phrases_enabled", true)));
      }
      RequestApplyInputConfigDeferred();
    });
  }
  void ShowThemePresetDialog() {
    constexpr double kThemePresetDialogWidth = 500.0;
    constexpr double kThemePresetPreviewWidth = 176.0;
    constexpr double kThemePresetPreviewHeight = 78.0;
    constexpr double kThemePresetCardHeight = 96.0;
    constexpr double kThemePresetListHeight = 232.0;

    ContentDialog dialog;
    ApplySettingsDialogBase(dialog, window_.Content().as<FrameworkElement>().XamlRoot());
    dialog.PrimaryButtonText(L"完成");
    dialog.CloseButtonText(L"取消");
    dialog.DefaultButton(ContentDialogButton::Primary);
    ApplySettingsResources(dialog.Resources());
    dialog.Resources().Insert(box_value(L"ContentDialogPadding"),
                              box_value(Thickness{22, 18, 22, 10}));
    dialog.Resources().Insert(box_value(L"ContentDialogCommandSpaceMargin"),
                              box_value(Thickness{0, 8, 0, 0}));
    dialog.Resources().Insert(box_value(L"ContentDialogTitleMargin"),
                              box_value(Thickness{0, 0, 0, 0}));

    StackPanel content;
    content.Width(kThemePresetDialogWidth);
    content.MaxWidth(kThemePresetDialogWidth);
    content.Spacing(12);
    content.HorizontalAlignment(HorizontalAlignment::Stretch);
    content.Children().Append(Text(L"输入法预设", 22, FW_SEMIBOLD));

    auto selected = std::make_shared<std::wstring>(CurrentThemePresetSetting());
    auto frames = std::make_shared<std::vector<std::pair<std::wstring, Border>>>();
    auto update_visuals = std::make_shared<std::function<void()>>();
    auto apply_card_visual = [selected](std::wstring_view id, Border const& frame) {
      const auto palette = CurrentSettingsPalette();
      const bool is_selected = *selected == id;
      frame.BorderBrush(SettingsBorderBrush());
      frame.Background(is_selected ? Brush(30,
                                           palette.accent.red,
                                           palette.accent.green,
                                           palette.accent.blue)
                                   : Brush(palette.card));
    };

    Grid grid;
    grid.ColumnSpacing(10);
    grid.RowSpacing(10);
    grid.Margin(Thickness{0, 0, 16, 0});
    for (int column = 0; column < 2; ++column) {
      ColumnDefinition definition;
      definition.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
      grid.ColumnDefinitions().Append(definition);
    }
    for (int row = 0; row < 4; ++row) {
      RowDefinition definition;
      definition.Height(GridLengthHelper::Auto());
      grid.RowDefinitions().Append(definition);
    }

    for (size_t index = 0; index < fp::kThemePresetDefinitions.size(); ++index) {
      const auto preset = fp::kThemePresetDefinitions[index];
      Border frame = SettingsFrame(ThemePreviewArtwork(preset.id,
                                                       kThemePresetPreviewWidth,
                                                       kThemePresetPreviewHeight,
                                                       true),
                                   CurrentSettingsPalette().card,
                                   Radius(8),
                                   UniformThickness(5));
      frame.HorizontalAlignment(HorizontalAlignment::Stretch);
      frame.Height(kThemePresetCardHeight);
      frame.UseLayoutRounding(true);
      SetSettingsToolTip(frame, preset.label);
      apply_card_visual(preset.id, frame);

      frame.Tapped([selected,
                    frames,
                    update_visuals,
                    id = std::wstring(preset.id)](auto const&, auto const&) {
        *selected = id;
        if (*update_visuals) {
          (*update_visuals)();
        }
      });
      frame.PointerEntered([frame](auto const&, auto const&) {
        frame.Opacity(0.94);
      });
      frame.PointerExited([frame](auto const&, auto const&) {
        frame.Opacity(1.0);
      });

      frames->push_back({std::wstring(preset.id), frame});
      Grid::SetColumn(frame, static_cast<int>(index % 2));
      Grid::SetRow(frame, static_cast<int>(index / 2));
      grid.Children().Append(frame);
    }

    *update_visuals = [frames, apply_card_visual]() {
      for (const auto& [id, frame] : *frames) {
        apply_card_visual(id, frame);
      }
    };

    ScrollViewer scroller;
    scroller.Content(grid);
    scroller.Height(kThemePresetListHeight);
    scroller.MaxHeight(kThemePresetListHeight);
    scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Visible);
    scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    scroller.VerticalScrollMode(ScrollMode::Enabled);
    scroller.HorizontalScrollMode(ScrollMode::Disabled);
    content.Children().Append(scroller);
    dialog.Content(content);
    auto operation = dialog.ShowAsync();
    operation.Completed([this, dialog, selected](auto const& async, auto const& status) {
      if (status != Windows::Foundation::AsyncStatus::Completed ||
          async.GetResults() != ContentDialogResult::Primary) {
        return;
      }
      WriteStringSetting(fp::kThemePresetSetting, *selected);
      WriteStringSetting(fp::kThemeModeSetting, fp::kThemeModeCustom);
      WriteStringSetting(fp::kLegacyThemeSetting, fp::kThemeModeCustom);
      RequestInputStateRefreshDeferred(80);
      RequestToolbarHostRefresh();
      ApplyThemeAndRefreshCurrentPage();
    });
  }

  UIElement BuildGeneralPage() {
    auto page = PageShell(L"常规", L"输入、候选、拼音辅助和默认设置。");
    const std::vector<std::pair<std::wstring, std::wstring>> double_pinyin_choices{
        {L"自然码", L"zrm"},
        {L"小鹤双拼", L"flypy"},
        {L"微软双拼", L"mspy"},
        {L"搜狗双拼", L"sogou"},
        {L"智能 ABC", L"abc"},
        {L"紫光双拼", L"ziguang"},
        {L"拼音加加", L"pyjj"},
        {L"国标双拼", L"gbpy"},
        {L"乱序 17", L"lxsq"},
    };
    auto input_scheme_icon =
        TextIcon(ReadStringSetting(L"input_scheme", fp::kDefaultInputScheme) == L"double_pinyin"
                     ? L"双"
                     : L"全");
    auto double_pinyin_scheme_icon =
        TextIcon(ChoiceIconText(
            double_pinyin_choices,
            ReadStringSetting(L"double_pinyin_scheme", fp::kDefaultDoublePinyinScheme)));
    auto double_pinyin_scheme = StringChoiceComboWithIcon(
        L"double_pinyin_scheme",
        double_pinyin_choices,
        fp::kDefaultDoublePinyinScheme,
        double_pinyin_scheme_icon,
        L"",
        true);
    auto double_pinyin_scheme_row = SettingRowWithIcon(
        L"双拼方案",
        L"仅在双拼下生效。",
        double_pinyin_scheme,
        double_pinyin_scheme_icon,
        L"已联动");
    double_pinyin_scheme_row.Visibility(
        ReadStringSetting(L"input_scheme", fp::kDefaultInputScheme) == L"double_pinyin"
            ? Visibility::Visible
            : Visibility::Collapsed);
    page.Children().Append(SectionHeader(L"拼音设置", true));
    page.Children().Append(SettingRowWithIcon(
        L"输入方案",
        L"选择全拼或双拼。",
        InputSchemeCombo(double_pinyin_scheme,
                         [input_scheme_icon, double_pinyin_scheme_row](std::wstring_view value) {
                           input_scheme_icon.Text(value == L"double_pinyin" ? L"双" : L"全");
                           double_pinyin_scheme_row.Visibility(value == L"double_pinyin"
                                                                   ? Visibility::Visible
                                                                   : Visibility::Collapsed);
                         }),
        input_scheme_icon,
        L"已联动"));
    page.Children().Append(double_pinyin_scheme_row);
    page.Children().Append(SectionHeader(L"默认模式"));
    auto input_mode_icon = TextIcon(DefaultInputModeIconText());
    page.Children().Append(SettingRowWithIcon(
        L"输入模式",
        L"设置默认中文或英文状态。",
        StringChoiceCombo(L"default_input_mode",
                          {{L"中文", L"zh"}, {L"英文", L"en"}},
                          L"zh",
                          L"",
                          false,
                          [input_mode_icon](std::wstring_view value) {
                            input_mode_icon.Text(value == L"en" ? L"英" : L"中");
                            RequestInputStateRefresh();
                            RequestToolbarHostRefresh();
                          }),
        input_mode_icon,
        L"已联动"));
    auto charset_icon = TextIcon(DefaultCharsetIconText());
    page.Children().Append(SettingRowWithIcon(
        L"输入字符",
        L"设置默认输出简体或繁体。",
        StringChoiceCombo(L"default_charset",
                          {{L"简体", L"simplified"}, {L"繁体", L"traditional"}},
                          L"simplified",
                          L"",
                          false,
                          [charset_icon](std::wstring_view value) {
                            charset_icon.Text(value == L"traditional" ? L"繁" : L"简");
                            RequestInputStateRefresh();
                            RequestToolbarHostRefresh();
                          }),
        charset_icon,
        L"已联动"));
    Grid shape_icon;
    shape_icon.Width(kSettingIconHostSize);
    shape_icon.Height(kSettingIconHostSize);
    shape_icon.HorizontalAlignment(HorizontalAlignment::Center);
    shape_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(shape_icon, ShapeStatusIcon(!ReadBoolSetting(L"default_shape_half", true)));
    page.Children().Append(SettingRowWithIcon(
        L"全角/半角",
        L"设置默认全角或半角状态。",
        BoolChoiceCombo(L"default_shape_half",
                        {{L"半角", true}, {L"全角", false}},
                        true,
                        [shape_icon](bool half_shape) {
                          SetIconChild(shape_icon, ShapeStatusIcon(!half_shape));
                          RequestInputStateRefresh();
                          RequestToolbarHostRefresh();
                        }),
        shape_icon,
        L"已联动"));
    Grid punctuation_icon;
    punctuation_icon.Width(kSettingIconHostSize);
    punctuation_icon.Height(kSettingIconHostSize);
    punctuation_icon.HorizontalAlignment(HorizontalAlignment::Center);
    punctuation_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(punctuation_icon,
                 PunctuationStatusIcon(ReadBoolSetting(L"default_chinese_punctuation", true)));
    page.Children().Append(SettingRowWithIcon(
        L"标点符号",
        L"设置默认中文或英文标点。",
        BoolChoiceCombo(L"default_chinese_punctuation",
                        {{L"中文标点", true}, {L"英文标点", false}},
                        true,
                        [punctuation_icon](bool chinese_punctuation) {
                          SetIconChild(punctuation_icon, PunctuationStatusIcon(chinese_punctuation));
                          RequestInputStateRefresh();
                          RequestToolbarHostRefresh();
                        }),
        punctuation_icon,
        L"已联动"));
    page.Children().Append(SectionHeader(L"候选窗口"));
    auto candidate_count_icon =
        TextIcon(std::to_wstring(ReadIntSetting(L"candidate_count",
                                                kDefaultCandidateCount,
                                                kMinCandidateCount,
                                                kMaxCandidateCount)));
    page.Children().Append(SettingRowWithIcon(L"候选词数",
                                              L"设置每页候选数量，范围 3 到 9。",
                                              CandidateCountCombo(candidate_count_icon),
                                              candidate_count_icon,
                                              L"已联动"));
    Grid candidate_layout_icon;
    candidate_layout_icon.Width(kSettingIconHostSize);
    candidate_layout_icon.Height(kSettingIconHostSize);
    candidate_layout_icon.HorizontalAlignment(HorizontalAlignment::Center);
    candidate_layout_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(candidate_layout_icon, CandidateLayoutIcon(ReadCandidateLayoutSetting()));
    page.Children().Append(SettingRowWithIcon(
        L"候选排列",
        L"选择横排或竖排候选窗口。",
        StringChoiceCombo(L"candidate_layout",
                          {{L"横排", L"horizontal"}, {L"竖排", L"vertical"}},
                          L"horizontal",
                          L"",
                          false,
                          [candidate_layout_icon](std::wstring_view value) {
                            SetIconChild(candidate_layout_icon, CandidateLayoutIcon(value));
                            RequestCandidateWindowVisualRefresh();
                            RequestCandidateWindowVisualRefreshDeferred(120);
                          }),
        candidate_layout_icon,
        L"已联动"));
    page.Children().Append(SectionHeader(L"拼音辅助"));
    Grid auto_correction_icon;
    auto_correction_icon.Width(kSettingIconHostSize);
    auto_correction_icon.Height(kSettingIconHostSize);
    auto_correction_icon.HorizontalAlignment(HorizontalAlignment::Center);
    auto_correction_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(auto_correction_icon,
                 AutoPinyinCorrectionIcon(ReadBoolSetting(L"auto_pinyin_correction", true)));
    page.Children().Append(SettingRowWithIcon(
        L"自动拼音纠错",
        L"修正常见拼写偏差。",
        SettingSwitch(L"auto_pinyin_correction",
                      true,
                      [auto_correction_icon](bool enabled) {
                        SetIconChild(auto_correction_icon, AutoPinyinCorrectionIcon(enabled));
                        RequestPinyinConfigRestart(enabled);
                      }),
        auto_correction_icon,
        L"已联动"));
    Grid super_abbrev_icon;
    super_abbrev_icon.Width(kSettingIconHostSize);
    super_abbrev_icon.Height(kSettingIconHostSize);
    super_abbrev_icon.HorizontalAlignment(HorizontalAlignment::Center);
    super_abbrev_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(super_abbrev_icon,
                 SuperAbbrevIcon(ReadBoolSetting(L"super_abbrev", true)));
    page.Children().Append(SettingRowWithIcon(
        L"超级简拼",
        L"使用更短的声母组合快速出词。",
        SettingSwitch(L"super_abbrev",
                      true,
                      [super_abbrev_icon](bool enabled) {
                        SetIconChild(super_abbrev_icon, SuperAbbrevIcon(enabled));
                        RequestRimeOptionRefresh(enabled);
                      }),
        super_abbrev_icon,
        L"已联动"));
    Grid smart_fuzzy_icon;
    smart_fuzzy_icon.Width(kSettingIconHostSize);
    smart_fuzzy_icon.Height(kSettingIconHostSize);
    smart_fuzzy_icon.HorizontalAlignment(HorizontalAlignment::Center);
    smart_fuzzy_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(smart_fuzzy_icon,
                 SmartFuzzyPinyinIcon(ReadBoolSetting(L"smart_fuzzy_pinyin", true)));
    page.Children().Append(SettingRowWithIcon(
        L"智能模糊拼音",
        L"自动处理常见模糊音。",
        SettingSwitch(L"smart_fuzzy_pinyin",
                      true,
                      [smart_fuzzy_icon](bool enabled) {
                        SetIconChild(smart_fuzzy_icon, SmartFuzzyPinyinIcon(enabled));
                        RequestPinyinConfigRestart(enabled);
                      }),
        smart_fuzzy_icon,
        L"已联动"));
    Grid fuzzy_rules_icon;
    fuzzy_rules_icon.Width(kSettingIconHostSize);
    fuzzy_rules_icon.Height(kSettingIconHostSize);
    fuzzy_rules_icon.HorizontalAlignment(HorizontalAlignment::Center);
    fuzzy_rules_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(fuzzy_rules_icon,
                 FuzzyPinyinRulesStateIcon(ReadBoolSetting(L"fuzzy_pinyin", false)));
    auto suppress_fuzzy_dialog = std::make_shared<bool>(false);
    ToggleSwitch fuzzy_rules_switch;
    ApplySettingsUIFont(fuzzy_rules_switch);
    fuzzy_rules_switch.MinWidth(0);
    fuzzy_rules_switch.HorizontalAlignment(HorizontalAlignment::Right);
    fuzzy_rules_switch.IsOn(ReadBoolSetting(L"fuzzy_pinyin", false));
    fuzzy_rules_switch.Toggled(
        [this, fuzzy_rules_switch, fuzzy_rules_icon, suppress_fuzzy_dialog](auto const&,
                                                                           auto const&) {
      WriteBoolSetting(L"fuzzy_pinyin", fuzzy_rules_switch.IsOn());
      SetIconChild(fuzzy_rules_icon, FuzzyPinyinRulesStateIcon(fuzzy_rules_switch.IsOn()));
      if (*suppress_fuzzy_dialog) {
        return;
      }
      if (fuzzy_rules_switch.IsOn()) {
        ShowFuzzyPinyinRulesDialog(fuzzy_rules_switch,
                                   fuzzy_rules_icon,
                                   true,
                                   suppress_fuzzy_dialog);
      } else {
        RequestApplyInputConfigDeferred();
      }
    });
    auto fuzzy_rules_edit = StableIconToolButton(
        kFluentIconEditSettings20RegularPath,
        L"编辑规则",
        0.92,
        [this, fuzzy_rules_switch, fuzzy_rules_icon, suppress_fuzzy_dialog]() {
          ShowFuzzyPinyinRulesDialog(fuzzy_rules_switch,
                                     fuzzy_rules_icon,
                                     false,
                                     suppress_fuzzy_dialog);
        });
    StackPanel fuzzy_rules_controls;
    fuzzy_rules_controls.Orientation(Orientation::Horizontal);
    fuzzy_rules_controls.Spacing(10);
    fuzzy_rules_controls.HorizontalAlignment(HorizontalAlignment::Right);
    fuzzy_rules_controls.Children().Append(fuzzy_rules_edit);
    fuzzy_rules_controls.Children().Append(fuzzy_rules_switch);
    page.Children().Append(SettingRowWithIcon(L"模糊拼音规则",
                                              L"选择启用的模糊音规则。",
                                              fuzzy_rules_controls,
                                              fuzzy_rules_icon,
                                              L"已联动",
                                              176));
    page.Children().Append(SectionHeader(L"恢复默认"));
    auto reset_button = StableIconToolButton(
        kFluentIconArrowClockwise20RegularPath,
        L"重置",
        0.92,
        [this]() {
      ResetDefaultSettings();
      RequestInputStateRefresh();
      RequestToolbarHostRefresh();
      RequestApplyInputConfigDeferred();
      g_toolbar_visible_switches.clear();
      page_cache_.clear();
      SelectNavItem(L"general");
      SetPage(hstring(L"general"));
    });
    page.Children().Append(SettingRowWithIcon(
        L"重置到默认状态",
        L"恢复设置，不清除词库。",
        reset_button,
        FluentPathIcon(kFluentIconApprovalsApp20RegularPath, 0.96),
        L"已联动"));
    return Scroll(page);
  }

  UIElement BuildAdvancedPage() {
    auto page = PageShell(L"高级", L"扩展输入模式。");
    page.Children().Append(SectionHeader(L"进阶输入", true));
    page.Children().Append(SettingRowWithIcon(L"人名输入",
                                              L"启用人名候选偏好。",
                                              RimeConfigSwitch(L"name_input", true),
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

  UIElement BuildAppearancePage() {
    auto page = PageShell(L"外观", L"候选窗口、状态提示、工具栏和主题。");
    page.Children().Append(SectionHeader(L"候选窗口", true));
    auto candidate_font_icon = TextIcon(
        CandidateFontIconText(ReadStringSetting(L"candidate_font_family",
                                                kDefaultCandidateFontFamily)));
    page.Children().Append(SettingRowWithIcon(
        L"候选项字体",
        L"切换候选窗口字体。",
        CandidateFontFamilyCombo(candidate_font_icon),
        candidate_font_icon,
        L"已联动"));
    page.Children().Append(SettingRowWithIcon(
        L"候选项字体大小",
        L"按小、中、大、特大档位调整。",
        CandidateFontSizeCombo(),
        CandidateFontSizeIcon(),
        L"已联动"));
    page.Children().Append(SectionHeader(L"桌面显示"));
    Grid toolbar_icon;
    toolbar_icon.Width(kSettingIconHostSize);
    toolbar_icon.Height(kSettingIconHostSize);
    toolbar_icon.HorizontalAlignment(HorizontalAlignment::Center);
    toolbar_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(toolbar_icon,
                 ToolbarVisibleIcon(ReadBoolSettingMigrated(kToolbarVisibleSetting,
                                                            false,
                                                            kLegacyToolbarVisibleSetting)));
    auto toolbar_switch = ToolbarVisibleSwitch([toolbar_icon](bool visible) {
      SetIconChild(toolbar_icon, ToolbarVisibleIcon(visible));
      if (!visible) {
        RequestToolbarHostShutdown();
      }
      RequestToolbarHostRefresh();
    });
    toolbar_switch.Toggled([toolbar_switch, toolbar_icon](auto const&, auto const&) {
      SetIconChild(toolbar_icon, ToolbarVisibleIcon(toolbar_switch.IsOn()));
    });
    g_toolbar_visible_switches.push_back(toolbar_switch);
    page.Children().Append(SettingRowWithIcon(L"输入法工具栏",
                                              L"在桌面显示可拖动的输入法工具栏。",
                                              toolbar_switch,
                                              toolbar_icon,
                                              L"已联动"));
    Grid status_tip_icon;
    status_tip_icon.Width(kSettingIconHostSize);
    status_tip_icon.Height(kSettingIconHostSize);
    status_tip_icon.HorizontalAlignment(HorizontalAlignment::Center);
    status_tip_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(status_tip_icon,
                 StatusTipEnabledIcon(ReadBoolSetting(L"status_tip_enabled", true)));
    page.Children().Append(SettingRowWithIcon(
        L"输入状态提示",
        L"切换输入状态时，在鼠标附近显示紧凑提示。",
        SettingSwitch(L"status_tip_enabled",
                      true,
                      [status_tip_icon](bool enabled) {
                        SetIconChild(status_tip_icon, StatusTipEnabledIcon(enabled));
                        RequestInputStateRefreshDeferred(80);
                      }),
        status_tip_icon,
        L"已联动"));
    auto status_tip_blacklist_edit = StableIconToolButton(
        kFluentIconCommentEdit20RegularPath,
        L"编辑黑名单",
        0.92,
        [this]() {
          TextBlock unused_count_icon;
          ShowStatusTipBlacklistDialog(unused_count_icon);
        });
    page.Children().Append(SettingRowWithIcon(
        L"状态提示黑名单",
        L"对指定进程关闭状态提示。",
        status_tip_blacklist_edit,
        StatusTipBlacklistIcon(),
        L"已联动"));
    page.Children().Append(SectionHeader(L"主题和材质"));
    Grid theme_icon;
    theme_icon.Width(kSettingIconHostSize);
    theme_icon.Height(kSettingIconHostSize);
    theme_icon.HorizontalAlignment(HorizontalAlignment::Center);
    theme_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(theme_icon, ThemeModeIcon(CurrentThemeModeSetting()));
    ComboBox theme_combo;
    ConfigureSettingCombo(theme_combo);
    const std::array<std::wstring_view, 4> theme_labels{L"跟随系统", L"浅色", L"深色", L"预设"};
    for (const auto label : theme_labels) {
      theme_combo.Items().Append(ThemeComboItem(label));
    }
    auto suppress_theme_selection = std::make_shared<bool>(false);
    theme_combo.SelectedIndex(ThemeModeIndex(CurrentThemeModeSetting()));
    theme_combo.SelectionChanged([this,
                                   theme_combo,
                                   theme_icon,
                                   suppress_theme_selection](auto const&, auto const&) {
      if (*suppress_theme_selection) {
        return;
      }
      const int selected = theme_combo.SelectedIndex();
      if (selected < 0) {
        return;
      }
      const std::wstring value = ThemeModeValueForIndex(selected);
      if (value == fp::kThemeModeCustom) {
        ShowThemePresetDialog();
        return;
      }
      WriteStringSetting(fp::kThemeModeSetting, value);
      WriteStringSetting(fp::kLegacyThemeSetting, value);
      WriteStringSetting(fp::kThemePresetSetting, DefaultPresetForThemeMode(value));
      SetIconChild(theme_icon, ThemeModeIcon(value));
      RequestInputStateRefreshDeferred(80);
      RequestToolbarHostRefresh();
      ApplyThemeAndRefreshCurrentPage();
    });
    auto theme_preset_button = StableIconToolButton(
        kFluentIconKeyboard20RegularPath,
        L"预设",
        1.02,
        [this]() { ShowThemePresetDialog(); });
    StackPanel theme_controls;
    theme_controls.Orientation(Orientation::Horizontal);
    theme_controls.Spacing(8);
    theme_controls.HorizontalAlignment(HorizontalAlignment::Right);
    theme_controls.VerticalAlignment(VerticalAlignment::Center);
    theme_controls.Children().Append(theme_combo);
    theme_controls.Children().Append(theme_preset_button);
    page.Children().Append(SettingRowWithIcon(
        L"主题",
        L"选择系统、浅色、深色或预设。",
        theme_controls,
        theme_icon,
        L"已联动",
        176.0));
    return Scroll(page);
  }

  UIElement LexiconControls(ToggleSwitch const& toggle, Border const& edit_button) {
    StackPanel controls;
    controls.Orientation(Orientation::Horizontal);
    controls.Spacing(10);
    controls.HorizontalAlignment(HorizontalAlignment::Right);
    controls.VerticalAlignment(VerticalAlignment::Center);
    controls.Children().Append(edit_button);
    controls.Children().Append(toggle);
    return controls;
  }

  UIElement BuildLexiconPage() {
    auto page = PageShell(L"词库", L"用户词、短语和导入词库。");

    page.Children().Append(SectionHeader(L"用户词库", true));
    Grid user_icon;
    user_icon.Width(kSettingIconHostSize);
    user_icon.Height(kSettingIconHostSize);
    user_icon.HorizontalAlignment(HorizontalAlignment::Center);
    user_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(user_icon,
                 UserLexiconStateIcon(ReadBoolSetting(L"user_lexicon_enabled", true)));
    ToggleSwitch user_switch;
    ApplySettingsUIFont(user_switch);
    user_switch.MinWidth(0);
    user_switch.HorizontalAlignment(HorizontalAlignment::Right);
    user_switch.IsOn(ReadBoolSetting(L"user_lexicon_enabled", true));
    user_switch.Toggled([user_switch, user_icon](auto const&, auto const&) {
      WriteBoolSetting(L"user_lexicon_enabled", user_switch.IsOn());
      WriteManagedDictionaryIntegrationFiles(ReadManagedDictionaryManifest());
      SetIconChild(user_icon, UserLexiconStateIcon(user_switch.IsOn()));
      RequestApplyInputConfigDeferred();
    });
    auto user_edit = StableIconToolButton(
        kFluentIconCalendarEdit20RegularPath,
        L"编辑用户词库",
        0.92,
        [this, user_icon]() {
          ShowPhraseLexiconDialog(L"用户词库",
                                  L"管理手动添加的词条。",
                                  true,
                                  user_icon);
        });
    page.Children().Append(SettingRowWithIcon(
        L"用户词库",
        L"维护常用词条。",
        LexiconControls(user_switch, user_edit),
        user_icon,
        L"已联动",
        176));

    page.Children().Append(SectionHeader(L"自定义短语"));
    Grid phrase_icon;
    phrase_icon.Width(kSettingIconHostSize);
    phrase_icon.Height(kSettingIconHostSize);
    phrase_icon.HorizontalAlignment(HorizontalAlignment::Center);
    phrase_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(phrase_icon,
                 CustomPhrasesStateIcon(ReadBoolSetting(L"custom_phrases_enabled", true)));
    ToggleSwitch phrase_switch;
    ApplySettingsUIFont(phrase_switch);
    phrase_switch.MinWidth(0);
    phrase_switch.HorizontalAlignment(HorizontalAlignment::Right);
    phrase_switch.IsOn(ReadBoolSetting(L"custom_phrases_enabled", true));
    phrase_switch.Toggled([phrase_switch, phrase_icon](auto const&, auto const&) {
      WriteBoolSetting(L"custom_phrases_enabled", phrase_switch.IsOn());
      SetIconChild(phrase_icon, CustomPhrasesStateIcon(phrase_switch.IsOn()));
      RequestApplyInputConfigDeferred();
    });
    auto phrase_edit = StableIconToolButton(
        kFluentIconDocumentEdit20RegularPath,
        L"编辑自定义短语",
        0.92,
        [this, phrase_icon]() {
          ShowPhraseLexiconDialog(L"自定义短语",
                                  L"管理固定编码短语。",
                                  false,
                                  phrase_icon);
        });
    page.Children().Append(SettingRowWithIcon(
        L"自定义短语",
        L"维护固定编码短语。",
        LexiconControls(phrase_switch, phrase_edit),
        phrase_icon,
        L"已联动",
        176));

    page.Children().Append(SectionHeader(L"导入和管理"));
    Grid import_icon;
    import_icon.Width(kSettingIconHostSize);
    import_icon.Height(kSettingIconHostSize);
    import_icon.HorizontalAlignment(HorizontalAlignment::Center);
    import_icon.VerticalAlignment(VerticalAlignment::Center);
    SetIconChild(import_icon,
                 ImportedLexiconsStateIcon(ReadBoolSetting(L"imported_lexicons_enabled", true)));
    ToggleSwitch import_switch;
    ApplySettingsUIFont(import_switch);
    import_switch.MinWidth(0);
    import_switch.HorizontalAlignment(HorizontalAlignment::Right);
    import_switch.IsOn(ReadBoolSetting(L"imported_lexicons_enabled", true));
    import_switch.Toggled([import_switch, import_icon](auto const&, auto const&) {
      WriteBoolSetting(L"imported_lexicons_enabled", import_switch.IsOn());
      WriteManagedDictionaryIntegrationFiles(ReadManagedDictionaryManifest());
      SetIconChild(import_icon, ImportedLexiconsStateIcon(import_switch.IsOn()));
      RequestApplyInputConfigDeferred();
    });
    auto import_edit = StableIconToolButton(
        kFluentIconEdit20RegularPath,
        L"管理导入词库",
        0.92,
        [this, import_icon]() {
          ShowManagedDictionariesDialog(import_icon);
        });
    page.Children().Append(SettingRowWithIcon(
        L"导入词库",
        L"导入、启用或删除词库。",
        LexiconControls(import_switch, import_edit),
        import_icon,
        L"已联动",
        176));
    return Scroll(page);
  }
  Border HotkeyBindingRow(std::wstring_view title,
                          std::wstring_view key,
                          std::wstring_view fallback,
                          std::wstring_view glyph) {
    return HotkeyBindingRowWithIcon(title, key, fallback, Icon(glyph, 16));
  }

  Border HotkeyBindingRowWithIcon(std::wstring_view title,
                                  std::wstring_view key,
                                  std::wstring_view fallback,
                                  UIElement const& icon_content) {
    StackPanel controls;
    controls.Orientation(Orientation::Horizontal);
    controls.Spacing(8);
    auto editor = HotkeyRecorderButton(key, fallback);
    controls.Children().Append(editor);

    auto clear = StableInlinePathActionButton(
        L"清除",
        kFluentIconDismissCircle20RegularPath,
        0.84,
        [editor, key = std::wstring(key)]() {
          WriteStringSetting(key, L"");
          SetHotkeyRecorderDisplay(editor, key, L"");
          RequestInputStateRefreshDeferred(80);
        });
    SetSettingsToolTip(clear, L"清除");
    controls.Children().Append(clear);

    auto reset = StableInlinePathActionButton(
        L"恢复默认",
        kFluentIconArrowClockwise20RegularPath,
        0.84,
        [editor, key = std::wstring(key), fallback = std::wstring(fallback)]() {
          WriteStringSetting(key, fallback);
          SetHotkeyRecorderDisplay(editor, key, fallback);
          RequestInputStateRefreshDeferred(80);
        });
    SetSettingsToolTip(reset, L"恢复默认");
    controls.Children().Append(reset);

    return SettingWideRowWithIcon(title,
                                  L"点击后按下新的组合键，或恢复默认值。",
                                  controls,
                                  icon_content,
                                  L"已联动");
  }

  UIElement BuildHotkeysPage() {
    auto page = PageShell(L"热键", L"状态切换和候选导航。");
    page.Children().Append(SectionHeader(L"状态切换", true));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"中/英文模式", L"shortcut_toolbar_input_mode", L"Shift", TextIcon(L"中")));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"全/半角", L"shortcut_toolbar_shape", L"Shift+.", ShapeStatusIcon(false)));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"中/英文标点", L"shortcut_toolbar_punctuation", L"Ctrl+.", PunctuationStatusIcon(true)));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"简体/繁体", L"shortcut_toolbar_charset", L"Ctrl+Shift+F", TextIcon(L"简")));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"表情符号/符号", L"shortcut_toolbar_emoji", L"Win+.", EmojiStatusIcon()));
    page.Children().Append(SectionHeader(L"候选导航"));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"展开/收起候选框",
        L"shortcut_candidate_expand",
        L"Tab",
        ChevronStatusIcon(kFluentChevronDown20Path)));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"上一页",
        L"shortcut_candidate_previous_page",
        L"PgUp",
        TriangleStatusIcon(false)));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"下一页",
        L"shortcut_candidate_next_page",
        L"PgDn",
        TriangleStatusIcon(true)));
    return Scroll(page);
  }

  UIElement BuildSyncPage() {
    auto page = PageShell(L"同步", L"剪贴板、配置、词库和备份。");
    page.Children().Append(SectionHeader(L"同步内容", true));
    page.Children().Append(SettingRowWithIcon(L"剪贴板同步",
                                              L"在设备之间同步剪贴板。",
                                              SettingSwitch(L"sync_clipboard", false),
                                              FluentPathIcon(kFluentIconClipboard20RegularPath, 0.94),
                                              L"已保存"));
    page.Children().Append(SettingRowWithIcon(L"配置和词库同步",
                                              L"同步设置、词库和输入数据。",
                                              SettingSwitch(L"sync_user_data", false),
                                              FluentPathIcon(kFluentIconBookDatabase20RegularPath, 0.90),
                                              L"已保存"));
    page.Children().Append(SectionHeader(L"云端服务"));
    auto edit_provider = StableIconToolButton(
        kFluentIconEditSettings20RegularPath,
        L"同步配置",
        0.86,
        [this]() { ShowSyncProviderDialog(); });
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
    upload.Click([this](auto const&, auto const&) {
      const auto config = CurrentSyncConfigFromSettings();
      RunSyncActionAsync(L"同步上传",
                         [config]() { return fp::sync::UploadNow(config); },
                         false);
    });
    auto download = ActionPathButton(L"下载", kFluentIconArrowDownload20RegularPath, 0.84);
    download.Click([this](auto const&, auto const&) {
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
    const int current_interval = ReadIntSetting(L"sync_auto_interval_minutes", 30, 5, 1440);
    auto auto_sync_switch = SettingSwitch(
        L"sync_auto_enabled",
        false,
        [this, current_interval](bool enabled) {
          ApplyAutoSyncSchedule(enabled,
                                ReadIntSetting(L"sync_auto_interval_minutes",
                                               current_interval,
                                               5,
                                               1440));
        });
    page.Children().Append(SettingRowWithIcon(L"自动同步",
                                              L"按固定间隔自动同步。",
                                              auto_sync_switch,
                                              FluentPathIcon(kFluentIconCalendarClock20RegularPath, 0.92),
                                              L"计划任务"));
    page.Children().Append(SettingRowWithIcon(
        L"同步间隔",
        L"设置自动同步频率。",
        StringChoiceCombo(L"sync_auto_interval_minutes",
                          {{L"每 15 分钟", L"15"},
                           {L"每 30 分钟", L"30"},
                           {L"每 1 小时", L"60"},
                           {L"每 6 小时", L"360"}},
                          L"30",
                          L"",
                          false,
                          [this](std::wstring_view value) {
                            if (!ReadBoolSetting(L"sync_auto_enabled", false)) {
                              return;
                            }
                            int interval = 30;
                            try {
                              interval = std::stoi(std::wstring(value));
                            } catch (...) {
                              interval = 30;
                            }
                            ApplyAutoSyncSchedule(true, interval);
                          }),
        FluentPathIcon(kFluentIconCalendarClock20RegularPath, 0.92),
        L"已保存"));

    page.Children().Append(SectionHeader(L"备份和恢复"));
    StackPanel actions;
    actions.Orientation(Orientation::Horizontal);
    actions.Spacing(10);
    auto backup = ActionPathButton(L"备份配置", kFluentIconArchive20RegularPath, 0.84);
    backup.Click([this](auto const&, auto const&) {
      const auto selected = PickSyncBackupSaveFile(g_settings_window_hwnd);
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
    restore.Click([this](auto const&, auto const&) {
      const auto selected = PickSyncBackupFile(g_settings_window_hwnd);
      if (!selected) {
        return;
      }
      const int confirm = MessageBoxW(g_settings_window_hwnd,
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

  Window window_{nullptr};
  Grid root_{nullptr};
  Grid title_bar_drag_region_{nullptr};
  NavigationView nav_{nullptr};
  XamlTypeInfo::XamlControlsXamlMetaDataProvider metadata_provider_;
  std::wstring current_page_tag_{L"general"};
  std::unordered_map<std::wstring, UIElement> page_cache_;
};

int RunWinUiApp() {
  try {
    PACKAGE_VERSION min_version{};
    min_version.Major = 8000;
    min_version.Minor = 806;
    min_version.Build = 2252;
    min_version.Revision = 0;
    const HRESULT bootstrap_result =
        MddBootstrapInitialize2(0x00010008, nullptr, min_version, MddBootstrapInitializeOptions_None);
    if (FAILED(bootstrap_result)) {
      ShowWindowsAppRuntimeMissingMessage(bootstrap_result);
      return static_cast<int>(bootstrap_result);
    }

    init_apartment(apartment_type::single_threaded);
    Application::Start([](auto&&) {
      make<SettingsApp>();
    });
    MddBootstrapShutdown();
    return 0;
  } catch (hresult_error const& error) {
    const std::wstring message = L"WinUI 3 设置面板启动失败：0x" +
                                 std::to_wstring(static_cast<unsigned long>(error.code())) +
                                 L"\n" + std::wstring(error.message());
    MessageBoxW(nullptr, message.c_str(), L"流畅拼音 设置", MB_ICONERROR);
    MddBootstrapShutdown();
    return static_cast<int>(error.code());
  } catch (...) {
    MessageBoxW(nullptr, L"WinUI 3 设置面板启动失败。", L"流畅拼音 设置", MB_ICONERROR);
    MddBootstrapShutdown();
    return E_FAIL;
  }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  EnsureUiFontsLoaded();
  if (HasCommandLineSwitch(L"--auto-sync")) {
    return RunAutoSyncCommand();
  }

  const std::wstring mutex_name(kSettingsSingleInstanceMutexName);
  HANDLE single_instance_mutex = CreateMutexW(nullptr, TRUE, mutex_name.c_str());
  const DWORD mutex_error = single_instance_mutex != nullptr ? GetLastError() : ERROR_SUCCESS;
  if (single_instance_mutex != nullptr && mutex_error == ERROR_ALREADY_EXISTS) {
    ActivateExistingSettingsWindow(40);
    CloseHandle(single_instance_mutex);
    return 0;
  }

  const int result = RunWinUiApp();
  if (single_instance_mutex != nullptr) {
    ReleaseMutex(single_instance_mutex);
    CloseHandle(single_instance_mutex);
  }
  return result;
}
