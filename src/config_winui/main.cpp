#include "common/broadcast_messages.h"
#include "common/constants.h"
#include "common/candidate_font.h"
#include "common/encoding.h"
#include "common/theme.h"
#include "config_winui/about_page.h"
#include "config_winui/advanced_page.h"
#include "config_winui/appearance_page.h"
#include "config_winui/app_paths.h"
#include "config_winui/candidate_layout_settings.h"
#include "config_winui/default_settings.h"
#include "config_winui/file_dialogs.h"
#include "config_winui/fuzzy_pinyin_custom_rules.h"
#include "config_winui/fuzzy_pinyin_rules.h"
#include "config_winui/general_page.h"
#include "config_winui/hotkey_recorder_controls.h"
#include "config_winui/hotkeys_page.h"
#include "config_winui/lexicon_page.h"
#include "config_winui/settings_app_lifecycle.h"
#include "config_winui/settings_binding.h"
#include "config_winui/settings_controls.h"
#include "config_winui/settings_navigation.h"
#include "config_winui/settings_options.h"
#include "config_winui/settings_refresh.h"
#include "config_winui/settings_ui_helpers.h"
#include "config_winui/settings_value_controls.h"
#include "config_winui/settings_icons.h"
#include "config_winui/shell_actions.h"
#include "config_winui/status_tip_blacklist.h"
#include "config_winui/sync_page.h"
#include "config_winui/theme_helpers.h"
#include "config_winui/theme_preset_dialog.h"
#include "config_winui/toolbar_visibility_state.h"
#include "config_winui/wanxiang_modes.h"
#include "config_winui/window_helpers.h"
#include <windows.h>
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
#include <array>
#include <cmath>
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
namespace XamlTypeInfo = Microsoft::UI::Xaml::XamlTypeInfo;
using Windows::Foundation::IInspectable;
using Windows::Foundation::IReference;
using Windows::Graphics::SizeInt32;
using Windows::UI::Xaml::Interop::TypeName;
using fp::config_winui::ReadBoolSetting;
using fp::config_winui::ReadBoolSettingMigrated;
using fp::config_winui::ReadIntSetting;
using fp::config_winui::ReadStringSetting;
using fp::config_winui::RimeUserDataPath;
using fp::config_winui::ResetSettingsCache;
using fp::config_winui::SettingsPath;
using fp::config_winui::SiblingExe;
using fp::config_winui::InitialSettingsPageTag;
using fp::config_winui::NormalizeSettingsPageTag;
using fp::config_winui::OpenPath;
using fp::config_winui::ClearSettingsPaletteOverride;
using fp::config_winui::CurrentSettingsPalette;
using fp::config_winui::BoolIconText;
using fp::config_winui::ChoiceIconText;
using fp::config_winui::FirstIconText;
using fp::config_winui::IntChoiceIconText;
using fp::config_winui::ApplySettingsDialogBase;
using fp::config_winui::ApplySettingsResources;
using fp::config_winui::ApplySettingsUIFont;
using fp::config_winui::Brush;
using fp::config_winui::ColorReference;
using fp::config_winui::CurrentSettingsApplicationTheme;
using fp::config_winui::CurrentSettingsElementTheme;
using fp::config_winui::Radius;
using fp::config_winui::SetSettingsWindowHandle;
using fp::config_winui::SettingsBorderBrush;
using fp::config_winui::SettingsBorderHoverBrush;
using fp::config_winui::SettingsCardBrush;
using fp::config_winui::SettingsFrame;
using fp::config_winui::SettingsHairlineThickness;
using fp::config_winui::SettingsIconBrush;
using fp::config_winui::SettingsSecondaryTextBrush;
using fp::config_winui::SettingsSurfaceBrush;
using fp::config_winui::SettingsTextBrush;
using fp::config_winui::SettingsUiFontFamily;
using fp::config_winui::SettingsWindowHandle;
using fp::config_winui::EllipseShape;
using fp::config_winui::Icon;
using fp::config_winui::IconHost;
using fp::config_winui::PathShape;
using fp::config_winui::RawPathShape;
using fp::config_winui::ScaledIconCanvas;
using fp::config_winui::SetIconChild;
using fp::config_winui::SetSettingsToolTip;
using fp::config_winui::Text;
using fp::config_winui::TextIcon;
using fp::config_winui::TransparentBrush;
using fp::config_winui::UniformThickness;
using fp::config_winui::RequestApplyInputConfig;
using fp::config_winui::RequestApplyInputConfigDeferred;
using fp::config_winui::RequestCandidateWindowVisualRefresh;
using fp::config_winui::RequestCandidateWindowVisualRefreshDeferred;
using fp::config_winui::RequestInputStateRefresh;
using fp::config_winui::RequestInputStateRefreshDeferred;
using fp::config_winui::RequestToolbarHostRefresh;
using fp::config_winui::RequestToolbarHostShutdown;
using fp::config_winui::SetSettingsPaletteOverride;
using fp::config_winui::ShowThemePresetDialog;
using fp::config_winui::WanxiangModeDefinition;
using fp::config_winui::ApplyDwmWindowFrame;
using fp::config_winui::ApplyTitleBarColors;
using fp::config_winui::ApplyWindowIcons;
using fp::config_winui::CenterWindowOnMonitor;
using fp::config_winui::DefaultWindowSize;
using fp::config_winui::GetWindowHandle;
using fp::config_winui::WriteBoolSetting;
using fp::config_winui::WriteIntSetting;
using fp::config_winui::WriteStringSetting;
using fp::config_winui::EnsureUiFontsLoaded;
using fp::config_winui::ModuleDirectory;
using fp::config_winui::IntChoiceCombo;
using fp::config_winui::SettingSwitch;
using fp::config_winui::SettingTextBox;
using fp::config_winui::StringChoiceCombo;
using fp::config_winui::NavItem;
using fp::config_winui::ClearToolbarVisibleSwitches;
using fp::config_winui::ActivateExistingSettingsWindow;
using fp::config_winui::HasCommandLineSwitch;
using fp::config_winui::InitialPageTagFromProcess;
using fp::config_winui::kSettingsAppTitle;
using fp::config_winui::kSettingsSingleInstanceMutexName;
using fp::config_winui::RunAutoSyncCommand;
using fp::config_winui::ShowWindowsAppRuntimeMissingMessage;
using fp::config_winui::Card;
using fp::config_winui::ChoiceCombo;
using fp::config_winui::ActionButton;
using fp::config_winui::ActionPathButton;
using fp::config_winui::CompactActionButton;
using fp::config_winui::CompactPathActionButton;
using fp::config_winui::IconToolButton;
using fp::config_winui::InlinePathActionButton;
using fp::config_winui::StableIconToolButton;
using fp::config_winui::StableInlinePathActionButton;
using fp::config_winui::ConfigureSettingCombo;
using fp::config_winui::PageShell;
using fp::config_winui::SectionHeader;
using fp::config_winui::SettingIconBackdrop;
using fp::config_winui::SettingRow;
using fp::config_winui::SettingRowWithIcon;
using fp::config_winui::SettingWideRow;
using fp::config_winui::SettingWideRowWithIcon;
using fp::config_winui::StatusBadge;
using fp::config_winui::ThemeComboItem;
using fp::config_winui::ActionButtonPathIcon;
using fp::config_winui::ChevronStatusIcon;
using fp::config_winui::CustomPhrasesStateIcon;
using fp::config_winui::EmojiStatusIcon;
using fp::config_winui::FluentButtonPathIcon;
using fp::config_winui::FluentPathIcon;
using fp::config_winui::ImportedLexiconsStateIcon;
using fp::config_winui::kFluentChevronDown20Path;
using fp::config_winui::kFluentChevronLeft20Path;
using fp::config_winui::kFluentChevronRight20Path;
using fp::config_winui::kFluentChevronUp20Path;
using fp::config_winui::kFluentCircle20FilledPath;
using fp::config_winui::kFluentCircle20RegularPath;
using fp::config_winui::kFluentDarkTheme20RegularPath;
using fp::config_winui::kFluentEmoji24Path;
using fp::config_winui::kFluentIconAddCircle20RegularPath;
using fp::config_winui::kFluentIconApprovalsApp20FilledPath;
using fp::config_winui::kFluentIconApprovalsApp20RegularPath;
using fp::config_winui::kFluentIconArchive20RegularPath;
using fp::config_winui::kFluentIconArrowClockwise20FilledPath;
using fp::config_winui::kFluentIconArrowClockwise20RegularPath;
using fp::config_winui::kFluentIconArrowDownload20RegularPath;
using fp::config_winui::kFluentIconArrowImport20RegularPath;
using fp::config_winui::kFluentIconArrowUpload20RegularPath;
using fp::config_winui::kFluentIconBookDatabase20RegularPath;
using fp::config_winui::kFluentIconBookDismiss20RegularPath;
using fp::config_winui::kFluentIconCalendarClock20RegularPath;
using fp::config_winui::kFluentIconCalendarEdit20RegularPath;
using fp::config_winui::kFluentIconCalendarEmpty20RegularPath;
using fp::config_winui::kFluentIconCheckmarkCircle20RegularPath;
using fp::config_winui::kFluentIconClipboard20RegularPath;
using fp::config_winui::kFluentIconCloud20RegularPath;
using fp::config_winui::kFluentIconCloudSync20RegularPath;
using fp::config_winui::kFluentIconComment20RegularPath;
using fp::config_winui::kFluentIconCommentEdit20RegularPath;
using fp::config_winui::kFluentIconCommentNote20RegularPath;
using fp::config_winui::kFluentIconCommentOff20RegularPath;
using fp::config_winui::kFluentIconDatabase20RegularPath;
using fp::config_winui::kFluentIconDelete20RegularPath;
using fp::config_winui::kFluentIconDismissCircle20RegularPath;
using fp::config_winui::kFluentIconDocument20RegularPath;
using fp::config_winui::kFluentIconDocumentDismiss20RegularPath;
using fp::config_winui::kFluentIconDocumentEdit20RegularPath;
using fp::config_winui::kFluentIconDocumentTable20RegularPath;
using fp::config_winui::kFluentIconEdit20FilledPath;
using fp::config_winui::kFluentIconEdit20RegularPath;
using fp::config_winui::kFluentIconEditOff20FilledPath;
using fp::config_winui::kFluentIconEditOff20RegularPath;
using fp::config_winui::kFluentIconEditSettings20FilledPath;
using fp::config_winui::kFluentIconEditSettings20RegularPath;
using fp::config_winui::kFluentIconFlash20FilledPath;
using fp::config_winui::kFluentIconFlash20RegularPath;
using fp::config_winui::kFluentIconFlashOff20FilledPath;
using fp::config_winui::kFluentIconFlashOff20RegularPath;
using fp::config_winui::kFluentIconHistory20RegularPath;
using fp::config_winui::kFluentIconKeyboard20RegularPath;
using fp::config_winui::kFluentIconLibrary20FilledPath;
using fp::config_winui::kFluentIconLibrary20RegularPath;
using fp::config_winui::kFluentIconLightbulb20FilledPath;
using fp::config_winui::kFluentIconLightbulb20RegularPath;
using fp::config_winui::kFluentIconLightbulbFilament20FilledPath;
using fp::config_winui::kFluentIconLightbulbFilament20RegularPath;
using fp::config_winui::kFluentIconRectangleLandscape20FilledPath;
using fp::config_winui::kFluentIconRectangleLandscape20RegularPath;
using fp::config_winui::kFluentIconRename20FilledPath;
using fp::config_winui::kFluentIconRename20RegularPath;
using fp::config_winui::kFluentIconRulesEdit20RegularPath;
using fp::config_winui::kFluentIconSearchSparkle20RegularPath;
using fp::config_winui::kFluentIconSparkleCircle20RegularPath;
using fp::config_winui::kFluentIconTextFont20RegularPath;
using fp::config_winui::kFluentIconTextGrammarCheckmark20RegularPath;
using fp::config_winui::kFluentIconToggleLeft20RegularPath;
using fp::config_winui::kFluentIconToggleRight20RegularPath;
using fp::config_winui::kFluentReOrderDotsHorizontal20RegularPath;
using fp::config_winui::kFluentReOrderDotsVertical20RegularPath;
using fp::config_winui::kFluentTriangleLeft12FilledPath;
using fp::config_winui::kFluentTriangleRight12FilledPath;
using fp::config_winui::kFluentWeatherMoon20FilledPath;
using fp::config_winui::kFluentWeatherMoon24Path;
using fp::config_winui::kFluentWeatherSunny20RegularPath;
using fp::config_winui::PunctuationStatusIcon;
using fp::config_winui::ShapeStatusIcon;
using fp::config_winui::TriangleStatusIcon;
using fp::config_winui::UserLexiconStateIcon;

constexpr double kNavigationOpenPaneLength = 248.0;
constexpr double kTitleBarDragHeight = 40.0;
constexpr double kTitleBarCaptionButtonReservedWidth = 150.0;
constexpr double kContentFrameLeftInset = 12.0;
constexpr double kContentFrameTopInset = 48.0;
constexpr double kContentFrameRightInset = 24.0;
constexpr double kContentFrameBottomInset = 22.0;

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
    SetSettingsWindowHandle(GetWindowHandle(window_));
    ApplySettingsResources(Resources());
    window_.Title(kSettingsAppTitle);
    window_.ExtendsContentIntoTitleBar(true);
    window_.Content(BuildRoot(InitialPageTagFromProcess()));
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
        window_, SizeInt32{860, 480});
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
    SetSettingsWindowHandle(GetWindowHandle(window_));
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
        initial_page.empty() ? InitialPageTagFromProcess()
                             : NormalizeSettingsPageTag(initial_page);

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
      page = fp::config_winui::BuildGeneralPage(
          window_.Content().as<FrameworkElement>().XamlRoot(),
          [this]() { ResetToDefaultSettingsPage(); });
    } else if (tag == L"advanced") {
      page = BuildAdvancedPage();
    } else if (tag == L"appearance") {
      page = fp::config_winui::BuildAppearancePage(
          window_.Content().as<FrameworkElement>().XamlRoot(),
          [this]() { ShowThemePresetEditor(); },
          [this]() { ApplyThemeAndRefreshCurrentPage(); });
    } else if (tag == L"lexicon") {
      page = fp::config_winui::BuildLexiconPage(
          SettingsWindowHandle(), window_.Content().as<FrameworkElement>().XamlRoot());
    } else if (tag == L"hotkeys") {
      page = BuildHotkeysPage();
    } else if (tag == L"sync") {
      page = fp::config_winui::BuildSyncPage(
          SettingsWindowHandle(), window_.Content().as<FrameworkElement>().XamlRoot());
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

  void ResetToDefaultSettingsPage() {
    ResetDefaultSettings();
    RequestInputStateRefresh();
    RequestToolbarHostRefresh();
    RequestApplyInputConfigDeferred();
    ClearToolbarVisibleSwitches();
    page_cache_.clear();
    SelectNavItem(L"general");
    SetPage(hstring(L"general"));
  }

  void ShowThemePresetEditor() {
    ShowThemePresetDialog(window_.Content().as<FrameworkElement>().XamlRoot(),
                          [this]() { ApplyThemeAndRefreshCurrentPage(); });
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
