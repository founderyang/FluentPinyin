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
#include "config_winui/hotkey_recorder_controls.h"
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
using fp::config_winui::CurrentFuzzyPinyinCustomRuleCount;
using fp::config_winui::CurrentFuzzyPinyinCustomRulesText;
using fp::config_winui::CurrentFuzzyPinyinRules;
using fp::config_winui::FuzzyPinyinRuleIconText;
using fp::config_winui::FuzzyPinyinRuleSelected;
using fp::config_winui::PickRimeDictionaryFile;
using fp::config_winui::PickSyncBackupFile;
using fp::config_winui::PickSyncBackupSaveFile;
using fp::config_winui::FuzzyPinyinRuleDefinitions;
using fp::config_winui::JoinFuzzyPinyinRules;
using fp::config_winui::JoinFuzzyPinyinCustomRules;
using fp::config_winui::NormalizeFuzzyPinyinCustomRuleToken;
using fp::config_winui::ParseFuzzyPinyinCustomRuleSetting;
using fp::config_winui::ParseFuzzyPinyinRuleSetting;
using fp::config_winui::SplitFuzzyPinyinCustomRule;
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
using fp::config_winui::RequestApplyInputConfig;
using fp::config_winui::RequestApplyInputConfigDeferred;
using fp::config_winui::RequestCandidateWindowVisualRefresh;
using fp::config_winui::RequestCandidateWindowVisualRefreshDeferred;
using fp::config_winui::RequestInputStateRefresh;
using fp::config_winui::RequestInputStateRefreshDeferred;
using fp::config_winui::RequestToolbarHostRefresh;
using fp::config_winui::RequestToolbarHostShutdown;
using fp::config_winui::RunTool;
using fp::config_winui::CurrentCustomPhraseCount;
using fp::config_winui::CurrentManagedDictionaryCount;
using fp::config_winui::CurrentUserLexiconCount;
using fp::config_winui::FileStemDisplayName;
using fp::config_winui::ImportManagedDictionary;
using fp::config_winui::IsSafeRimeDictName;
using fp::config_winui::LexiconCountText;
using fp::config_winui::ManagedDictionaryEntry;
using fp::config_winui::NormalizePhraseWeight;
using fp::config_winui::PhraseEntry;
using fp::config_winui::ReadCustomPhraseEntries;
using fp::config_winui::ReadManagedDictionaryManifest;
using fp::config_winui::ReadUserLexiconEntries;
using fp::config_winui::RemoveManagedDictionaryFile;
using fp::config_winui::TrimLexiconText;
using fp::config_winui::WriteCustomPhraseEntries;
using fp::config_winui::WriteManagedDictionaryIntegrationFiles;
using fp::config_winui::WriteUserLexiconEntries;
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
using fp::config_winui::HotkeyRecorderButton;
using fp::config_winui::RimeConfigSwitch;
using fp::config_winui::SettingSwitch;
using fp::config_winui::SettingTextBox;
using fp::config_winui::SetHotkeyRecorderDisplay;
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

void SetFuzzyPinyinRuleChecks(
    const std::vector<std::pair<std::wstring, CheckBox>>& checks,
    const std::vector<std::wstring>& enabled_rules) {
  for (const auto& [id, check] : checks) {
    check.IsChecked(NullableBool(FuzzyPinyinRuleSelected(enabled_rules, id)));
  }
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
              ParseFuzzyPinyinRuleSetting(fp::kDefaultFuzzyPinyinRules, true));
        });
    actions.Children().Append(all);
    auto common = StableInlinePathActionButton(
        L"常用",
        kFluentIconSparkleCircle20RegularPath,
        0.64,
        [checks]() {
          SetFuzzyPinyinRuleChecks(
              *checks,
              ParseFuzzyPinyinRuleSetting(fp::kDefaultCommonFuzzyPinyinRules, false));
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
             ReadStringSetting(fp::kFuzzyPinyinCustomRulesSetting, L""))) {
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
          WriteBoolSetting(fp::kFuzzyPinyinSetting, false);
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
        WriteStringSetting(fp::kFuzzyPinyinRulesSetting, L"");
        WriteStringSetting(fp::kFuzzyPinyinCustomRulesSetting, L"");
        if (suppress_toggle_dialog) {
          *suppress_toggle_dialog = true;
        }
        fuzzy_switch.IsOn(false);
        SetIconChild(state_icon, FuzzyPinyinRulesStateIcon(false));
        if (suppress_toggle_dialog) {
          *suppress_toggle_dialog = false;
        }
        WriteBoolSetting(fp::kFuzzyPinyinSetting, false);
        RequestApplyInputConfigDeferred();
        return;
      }

      WriteStringSetting(fp::kFuzzyPinyinRulesSetting,
                         JoinFuzzyPinyinRules(selected));
      WriteStringSetting(fp::kFuzzyPinyinCustomRulesSetting,
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
        WriteBoolSetting(fp::kFuzzyPinyinSetting, true);
        RequestApplyInputConfigDeferred();
      }
    });
  }

  void ShowStatusTipBlacklistEditor(TextBlock const& count_icon) {
    ShowStatusTipBlacklistDialog(window_.Content().as<FrameworkElement>().XamlRoot(),
                                 count_icon);
  }

  void ShowSyncMessage(std::wstring_view title, std::wstring_view message, UINT flags = MB_OK) {
    MessageBoxW(SettingsWindowHandle(),
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
    WriteBoolSetting(fp::kSyncAutoEnabledSetting, enabled);
    WriteIntSetting(fp::kSyncAutoIntervalMinutesSetting, interval_minutes);
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
      const auto selected = PickRimeDictionaryFile(SettingsWindowHandle());
      if (!selected) {
        return;
      }

      std::wstring error;
      if (!ImportManagedDictionary(*selected, &error)) {
        MessageBoxW(SettingsWindowHandle(),
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
                   ImportedLexiconsStateIcon(
                       ReadBoolSetting(fp::kImportedLexiconsEnabledSetting, true)));
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
                   ImportedLexiconsStateIcon(
                       ReadBoolSetting(fp::kImportedLexiconsEnabledSetting, true)));
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
                     UserLexiconStateIcon(
                         ReadBoolSetting(fp::kUserLexiconEnabledSetting, true)));
      } else {
        WriteCustomPhraseEntries(entries);
        SetIconChild(state_icon,
                     CustomPhrasesStateIcon(
                         ReadBoolSetting(fp::kCustomPhrasesEnabledSetting, true)));
      }
      RequestApplyInputConfigDeferred();
    });
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
        L"中/英文模式",
        fp::kShortcutToolbarInputModeSetting,
        fp::kDefaultShortcutToolbarInputMode,
        TextIcon(L"中")));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"全/半角",
        fp::kShortcutToolbarShapeSetting,
        fp::kDefaultShortcutToolbarShape,
        ShapeStatusIcon(false)));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"中/英文标点",
        fp::kShortcutToolbarPunctuationSetting,
        fp::kDefaultShortcutToolbarPunctuation,
        PunctuationStatusIcon(true)));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"简体/繁体",
        fp::kShortcutToolbarCharsetSetting,
        fp::kDefaultShortcutToolbarCharset,
        TextIcon(L"简")));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"表情符号/符号",
        fp::kShortcutToolbarEmojiSetting,
        fp::kDefaultShortcutToolbarEmoji,
        EmojiStatusIcon()));
    page.Children().Append(SectionHeader(L"候选导航"));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"展开/收起候选框",
        fp::kShortcutCandidateExpandSetting,
        fp::kDefaultShortcutCandidateExpand,
        ChevronStatusIcon(kFluentChevronDown20Path)));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"上一页",
        fp::kShortcutCandidatePreviousPageSetting,
        fp::kDefaultShortcutCandidatePreviousPage,
        TriangleStatusIcon(false)));
    page.Children().Append(HotkeyBindingRowWithIcon(
        L"下一页",
        fp::kShortcutCandidateNextPageSetting,
        fp::kDefaultShortcutCandidateNextPage,
        TriangleStatusIcon(true)));
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
