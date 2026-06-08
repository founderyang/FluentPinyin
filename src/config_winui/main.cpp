#include "common/broadcast_messages.h"
#include "common/constants.h"
#include "common/candidate_font.h"
#include "common/encoding.h"
#include "common/theme.h"
#include "config_winui/app_paths.h"
#include "config_winui/candidate_layout_settings.h"
#include "config_winui/default_settings.h"
#include "config_winui/file_dialogs.h"
#include "config_winui/fuzzy_pinyin_custom_rules.h"
#include "config_winui/fuzzy_pinyin_rules.h"
#include "config_winui/fuzzy_pinyin_rules_dialog.h"
#include "config_winui/hotkey_recorder_controls.h"
#include "config_winui/hotkeys_page.h"
#include "config_winui/lexicon_dialogs.h"
#include "config_winui/lexicon_store.h"
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
#include "config_winui/status_tip_blacklist_dialog.h"
#include "config_winui/sync_settings_actions.h"
#include "config_winui/theme_helpers.h"
#include "config_winui/theme_preset_dialog.h"
#include "config_winui/toolbar_visibility_state.h"
#include "config_winui/wanxiang_modes.h"
#include "config_winui/window_helpers.h"
#include "sync/sync_service.h"

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
using fp::config_winui::CandidateLayoutChoices;
using fp::config_winui::ReadCandidateLayoutSetting;
using fp::config_winui::OpenPath;
using fp::config_winui::OpenUrl;
using fp::config_winui::ClearSettingsPaletteOverride;
using fp::config_winui::CurrentSettingsPalette;
using fp::config_winui::CurrentThemeModeSetting;
using fp::config_winui::DefaultCharsetChoiceIconText;
using fp::config_winui::DefaultCharsetChoices;
using fp::config_winui::DefaultCharsetIconText;
using fp::config_winui::DefaultInputModeChoiceIconText;
using fp::config_winui::DefaultInputModeChoices;
using fp::config_winui::DefaultInputModeIconText;
using fp::config_winui::DefaultPunctuationChoices;
using fp::config_winui::DefaultPresetForThemeMode;
using fp::config_winui::DefaultShapeChoices;
using fp::config_winui::DoublePinyinSchemeChoices;
using fp::config_winui::EffectiveThemePreset;
using fp::config_winui::BoolIconText;
using fp::config_winui::CandidateFontIconText;
using fp::config_winui::ChoiceIconText;
using fp::config_winui::PickRimeDictionaryFile;
using fp::config_winui::PickSyncBackupFile;
using fp::config_winui::PickSyncBackupSaveFile;
using fp::config_winui::InputSchemeChoices;
using fp::config_winui::InputSchemeIconText;
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
using fp::config_winui::CurrentStatusTipBlacklistCount;
using fp::config_winui::ShowStatusTipBlacklistDialog;
using fp::config_winui::SyncAutoIntervalChoices;
using fp::config_winui::SyncAutoIntervalMinutesFromValue;
using fp::config_winui::ApplyAutoSyncSchedule;
using fp::config_winui::CurrentSyncConfigFromSettings;
using fp::config_winui::RunSyncActionAsync;
using fp::config_winui::ShowSyncProviderDialog;
using fp::config_winui::RequestApplyInputConfig;
using fp::config_winui::RequestApplyInputConfigDeferred;
using fp::config_winui::RequestCandidateWindowVisualRefresh;
using fp::config_winui::RequestCandidateWindowVisualRefreshDeferred;
using fp::config_winui::RequestInputStateRefresh;
using fp::config_winui::RequestInputStateRefreshDeferred;
using fp::config_winui::RequestToolbarHostRefresh;
using fp::config_winui::RequestToolbarHostShutdown;
using fp::config_winui::RunTool;
using fp::config_winui::ReadManagedDictionaryManifest;
using fp::config_winui::ShowManagedDictionariesDialog;
using fp::config_winui::ShowPhraseLexiconDialog;
using fp::config_winui::WriteManagedDictionaryIntegrationFiles;
using fp::config_winui::SetSettingsPaletteOverride;
using fp::config_winui::SettingsThemePalette;
using fp::config_winui::ThemeModeDisplayText;
using fp::config_winui::ThemeModeIndex;
using fp::config_winui::ThemeModeValueForIndex;
using fp::config_winui::ShowThemePresetDialog;
using fp::config_winui::ThemePreview;
using fp::config_winui::ThemePreviewPalette;
using fp::config_winui::WanxiangModeDefinition;
using fp::config_winui::WanxiangModeDefinitions;
using fp::config_winui::ApplyDwmWindowFrame;
using fp::config_winui::ApplyTitleBarColors;
using fp::config_winui::ApplyWindowIcons;
using fp::config_winui::CenterWindowOnMonitor;
using fp::config_winui::DefaultWindowSize;
using fp::config_winui::GetWindowHandle;
using fp::config_winui::WriteBoolSetting;
using fp::config_winui::WriteCandidateLayoutSetting;
using fp::config_winui::WriteIntSetting;
using fp::config_winui::WriteStringSetting;
using fp::config_winui::EnsureUiFontsLoaded;
using fp::config_winui::ModuleDirectory;
using fp::config_winui::BoolChoiceCombo;
using fp::config_winui::CandidateCountCombo;
using fp::config_winui::CandidateFontFamilyCombo;
using fp::config_winui::CandidateFontSizeCombo;
using fp::config_winui::InputSchemeCombo;
using fp::config_winui::IntChoiceCombo;
using fp::config_winui::RimeConfigSwitch;
using fp::config_winui::SettingSwitch;
using fp::config_winui::SettingTextBox;
using fp::config_winui::StringChoiceCombo;
using fp::config_winui::StringChoiceComboWithIcon;
using fp::config_winui::WanxiangModeIcon;
using fp::config_winui::WanxiangModeSwitch;
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
using fp::config_winui::AutoPinyinCorrectionIcon;
using fp::config_winui::CandidateFontSizeIcon;
using fp::config_winui::CandidateLayoutIcon;
using fp::config_winui::ChevronStatusIcon;
using fp::config_winui::CustomPhrasesStateIcon;
using fp::config_winui::EmojiStatusIcon;
using fp::config_winui::FluentButtonPathIcon;
using fp::config_winui::FluentPathIcon;
using fp::config_winui::FuzzyPinyinRulesStateIcon;
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
using fp::config_winui::kGitHubMark24Path;
using fp::config_winui::PunctuationStatusIcon;
using fp::config_winui::ShapeStatusIcon;
using fp::config_winui::SmartFuzzyPinyinIcon;
using fp::config_winui::StatusTipBlacklistIcon;
using fp::config_winui::StatusTipEnabledIcon;
using fp::config_winui::SuperAbbrevIcon;
using fp::config_winui::RegisterToolbarVisibleSwitch;
using fp::config_winui::ThemeModeIcon;
using fp::config_winui::ToolbarVisibleSwitch;
using fp::config_winui::ToolbarVisibleIcon;
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

  void ShowFuzzyPinyinRulesEditor(ToggleSwitch const& fuzzy_switch,
                                  Grid const& state_icon,
                                  bool require_enabled,
                                  std::shared_ptr<bool> suppress_toggle_dialog) {
    ShowFuzzyPinyinRulesDialog(window_.Content().as<FrameworkElement>().XamlRoot(),
                               fuzzy_switch,
                               state_icon,
                               require_enabled,
                               suppress_toggle_dialog);
  }

  void ShowStatusTipBlacklistEditor(TextBlock const& count_icon) {
    ShowStatusTipBlacklistDialog(window_.Content().as<FrameworkElement>().XamlRoot(),
                                 count_icon);
  }

  void ShowManagedDictionariesEditor(Grid const& state_icon) {
    ShowManagedDictionariesDialog(window_.Content().as<FrameworkElement>().XamlRoot(),
                                  SettingsWindowHandle(),
                                  state_icon);
  }

  void ShowPhraseLexiconEditor(std::wstring_view title_text,
                               std::wstring_view description_text,
                               bool user_lexicon,
                               Grid const& state_icon) {
    ShowPhraseLexiconDialog(window_.Content().as<FrameworkElement>().XamlRoot(),
                            title_text,
                            description_text,
                            user_lexicon,
                            state_icon);
  }

  void ShowThemePresetEditor() {
    ShowThemePresetDialog(window_.Content().as<FrameworkElement>().XamlRoot(),
                          [this]() { ApplyThemeAndRefreshCurrentPage(); });
  }

  UIElement BuildGeneralPage() {
    auto page = PageShell(L"常规", L"输入、候选、拼音辅助和默认设置。");
    auto input_scheme_icon =
        TextIcon(InputSchemeIconText(
            ReadStringSetting(fp::kInputSchemeSetting, fp::kDefaultInputScheme)));
    auto double_pinyin_scheme_icon =
        TextIcon(ChoiceIconText(
            DoublePinyinSchemeChoices(),
            ReadStringSetting(fp::kDoublePinyinSchemeSetting,
                              fp::kDefaultDoublePinyinScheme)));
    auto double_pinyin_scheme = StringChoiceComboWithIcon(
        fp::kDoublePinyinSchemeSetting,
        DoublePinyinSchemeChoices(),
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
        ReadStringSetting(fp::kInputSchemeSetting,
                          fp::kDefaultInputScheme) ==
                std::wstring(fp::kInputSchemeDoublePinyin)
            ? Visibility::Visible
            : Visibility::Collapsed);
    page.Children().Append(SectionHeader(L"拼音设置", true));
    page.Children().Append(SettingRowWithIcon(
        L"输入方案",
        L"选择全拼或双拼。",
        InputSchemeCombo(double_pinyin_scheme,
                         [input_scheme_icon, double_pinyin_scheme_row](std::wstring_view value) {
                           input_scheme_icon.Text(InputSchemeIconText(value));
                           double_pinyin_scheme_row.Visibility(
                               value == fp::kInputSchemeDoublePinyin ? Visibility::Visible
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
        StringChoiceCombo(fp::kDefaultInputModeSetting,
                          DefaultInputModeChoices(),
                          fp::kDefaultInputMode,
                          L"",
                          false,
                          [input_mode_icon](std::wstring_view value) {
                            input_mode_icon.Text(DefaultInputModeChoiceIconText(value));
                            RequestInputStateRefresh();
                            RequestToolbarHostRefresh();
                          }),
        input_mode_icon,
        L"已联动"));
    auto charset_icon = TextIcon(DefaultCharsetIconText());
    page.Children().Append(SettingRowWithIcon(
        L"输入字符",
        L"设置默认输出简体或繁体。",
        StringChoiceCombo(fp::kDefaultCharsetSetting,
                          DefaultCharsetChoices(),
                          fp::kDefaultCharset,
                          L"",
                          false,
                          [charset_icon](std::wstring_view value) {
                            charset_icon.Text(DefaultCharsetChoiceIconText(value));
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
    SetIconChild(shape_icon,
                 ShapeStatusIcon(!ReadBoolSetting(fp::kDefaultShapeHalfSetting, true)));
    page.Children().Append(SettingRowWithIcon(
        L"全角/半角",
        L"设置默认全角或半角状态。",
        BoolChoiceCombo(fp::kDefaultShapeHalfSetting,
                        DefaultShapeChoices(),
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
                 PunctuationStatusIcon(
                     ReadBoolSetting(fp::kDefaultChinesePunctuationSetting, true)));
    page.Children().Append(SettingRowWithIcon(
        L"标点符号",
        L"设置默认中文或英文标点。",
        BoolChoiceCombo(fp::kDefaultChinesePunctuationSetting,
                        DefaultPunctuationChoices(),
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
        TextIcon(std::to_wstring(ReadIntSetting(fp::kCandidateCountSetting,
                                                fp::kDefaultCandidateCount,
                                                fp::kMinCandidateCount,
                                                fp::kMaxCandidateCount)));
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
    SetIconChild(candidate_layout_icon,
                 CandidateLayoutIcon(ReadCandidateLayoutSetting(fp::kDefaultCandidateLayout)));
    page.Children().Append(SettingRowWithIcon(
        L"候选排列",
        L"选择横排或竖排候选窗口。",
        StringChoiceCombo(fp::kCandidateLayoutSetting,
                          CandidateLayoutChoices(),
                          fp::kDefaultCandidateLayout,
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
                 AutoPinyinCorrectionIcon(
                     ReadBoolSetting(fp::kAutoPinyinCorrectionSetting, true)));
    page.Children().Append(SettingRowWithIcon(
        L"自动拼音纠错",
        L"修正常见拼写偏差。",
        SettingSwitch(fp::kAutoPinyinCorrectionSetting,
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
                 SuperAbbrevIcon(ReadBoolSetting(fp::kSuperAbbrevSetting, true)));
    page.Children().Append(SettingRowWithIcon(
        L"超级简拼",
        L"使用更短的声母组合快速出词。",
        SettingSwitch(fp::kSuperAbbrevSetting,
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
                 SmartFuzzyPinyinIcon(
                     ReadBoolSetting(fp::kSmartFuzzyPinyinSetting, true)));
    page.Children().Append(SettingRowWithIcon(
        L"智能模糊拼音",
        L"自动处理常见模糊音。",
        SettingSwitch(fp::kSmartFuzzyPinyinSetting,
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
                 FuzzyPinyinRulesStateIcon(
                     ReadBoolSetting(fp::kFuzzyPinyinSetting, false)));
    auto suppress_fuzzy_dialog = std::make_shared<bool>(false);
    ToggleSwitch fuzzy_rules_switch;
    ApplySettingsUIFont(fuzzy_rules_switch);
    fuzzy_rules_switch.MinWidth(0);
    fuzzy_rules_switch.HorizontalAlignment(HorizontalAlignment::Right);
    fuzzy_rules_switch.IsOn(ReadBoolSetting(fp::kFuzzyPinyinSetting, false));
    fuzzy_rules_switch.Toggled(
        [this, fuzzy_rules_switch, fuzzy_rules_icon, suppress_fuzzy_dialog](auto const&,
                                                                           auto const&) {
      WriteBoolSetting(fp::kFuzzyPinyinSetting, fuzzy_rules_switch.IsOn());
      SetIconChild(fuzzy_rules_icon, FuzzyPinyinRulesStateIcon(fuzzy_rules_switch.IsOn()));
      if (*suppress_fuzzy_dialog) {
        return;
      }
      if (fuzzy_rules_switch.IsOn()) {
        ShowFuzzyPinyinRulesEditor(fuzzy_rules_switch,
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
          ShowFuzzyPinyinRulesEditor(fuzzy_rules_switch,
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
      ClearToolbarVisibleSwitches();
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
                                              RimeConfigSwitch(fp::kNameInputSetting, true),
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
        CandidateFontIconText(ReadStringSetting(fp::kCandidateFontFamilySetting,
                                                fp::kDefaultCandidateFontFamily)));
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
                 ToolbarVisibleIcon(ReadBoolSettingMigrated(fp::kToolbarVisibleSetting,
                                                            false,
                                                            fp::kLegacyToolbarVisibleSetting)));
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
    RegisterToolbarVisibleSwitch(toolbar_switch);
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
                 StatusTipEnabledIcon(ReadBoolSetting(fp::kStatusTipEnabledSetting, true)));
    page.Children().Append(SettingRowWithIcon(
        L"输入状态提示",
        L"切换输入状态时，在鼠标附近显示紧凑提示。",
        SettingSwitch(fp::kStatusTipEnabledSetting,
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
          ShowStatusTipBlacklistEditor(unused_count_icon);
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
        ShowThemePresetEditor();
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
        [this]() { ShowThemePresetEditor(); });
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
                 UserLexiconStateIcon(
                     ReadBoolSetting(fp::kUserLexiconEnabledSetting, true)));
    ToggleSwitch user_switch;
    ApplySettingsUIFont(user_switch);
    user_switch.MinWidth(0);
    user_switch.HorizontalAlignment(HorizontalAlignment::Right);
    user_switch.IsOn(ReadBoolSetting(fp::kUserLexiconEnabledSetting, true));
    user_switch.Toggled([user_switch, user_icon](auto const&, auto const&) {
      WriteBoolSetting(fp::kUserLexiconEnabledSetting, user_switch.IsOn());
      WriteManagedDictionaryIntegrationFiles(ReadManagedDictionaryManifest());
      SetIconChild(user_icon, UserLexiconStateIcon(user_switch.IsOn()));
      RequestApplyInputConfigDeferred();
    });
    auto user_edit = StableIconToolButton(
        kFluentIconCalendarEdit20RegularPath,
        L"编辑用户词库",
        0.92,
        [this, user_icon]() {
          ShowPhraseLexiconEditor(L"用户词库",
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
                 CustomPhrasesStateIcon(
                     ReadBoolSetting(fp::kCustomPhrasesEnabledSetting, true)));
    ToggleSwitch phrase_switch;
    ApplySettingsUIFont(phrase_switch);
    phrase_switch.MinWidth(0);
    phrase_switch.HorizontalAlignment(HorizontalAlignment::Right);
    phrase_switch.IsOn(ReadBoolSetting(fp::kCustomPhrasesEnabledSetting, true));
    phrase_switch.Toggled([phrase_switch, phrase_icon](auto const&, auto const&) {
      WriteBoolSetting(fp::kCustomPhrasesEnabledSetting, phrase_switch.IsOn());
      SetIconChild(phrase_icon, CustomPhrasesStateIcon(phrase_switch.IsOn()));
      RequestApplyInputConfigDeferred();
    });
    auto phrase_edit = StableIconToolButton(
        kFluentIconDocumentEdit20RegularPath,
        L"编辑自定义短语",
        0.92,
        [this, phrase_icon]() {
          ShowPhraseLexiconEditor(L"自定义短语",
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
                 ImportedLexiconsStateIcon(
                     ReadBoolSetting(fp::kImportedLexiconsEnabledSetting, true)));
    ToggleSwitch import_switch;
    ApplySettingsUIFont(import_switch);
    import_switch.MinWidth(0);
    import_switch.HorizontalAlignment(HorizontalAlignment::Right);
    import_switch.IsOn(ReadBoolSetting(fp::kImportedLexiconsEnabledSetting, true));
    import_switch.Toggled([import_switch, import_icon](auto const&, auto const&) {
      WriteBoolSetting(fp::kImportedLexiconsEnabledSetting, import_switch.IsOn());
      WriteManagedDictionaryIntegrationFiles(ReadManagedDictionaryManifest());
      SetIconChild(import_icon, ImportedLexiconsStateIcon(import_switch.IsOn()));
      RequestApplyInputConfigDeferred();
    });
    auto import_edit = StableIconToolButton(
        kFluentIconEdit20RegularPath,
        L"管理导入词库",
        0.92,
        [this, import_icon]() {
          ShowManagedDictionariesEditor(import_icon);
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
  UIElement BuildSyncPage() {
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
        [this]() {
          ShowSyncProviderDialog(window_.Content().as<FrameworkElement>().XamlRoot());
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
    const int current_interval =
        ReadIntSetting(fp::kSyncAutoIntervalMinutesSetting,
                       fp::kDefaultSyncAutoIntervalMinutes,
                       fp::kMinSyncAutoIntervalMinutes,
                       fp::kMaxSyncAutoIntervalMinutes);
    auto auto_sync_switch = SettingSwitch(
        fp::kSyncAutoEnabledSetting,
        false,
        [this, current_interval](bool enabled) {
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
                          [this](std::wstring_view value) {
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
    backup.Click([this](auto const&, auto const&) {
      const auto selected = PickSyncBackupSaveFile(SettingsWindowHandle());
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
      const auto selected = PickSyncBackupFile(SettingsWindowHandle());
      if (!selected) {
        return;
      }
      const int confirm = MessageBoxW(SettingsWindowHandle(),
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
