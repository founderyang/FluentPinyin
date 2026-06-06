#include "tsf/tsf_text_service.h"

#include "common/constants.h"
#include "common/logging.h"
#include "common/path_utils.h"
#include "common/theme.h"
#include "tsf/guids.h"
#include "tsf/module.h"
#include "tsf/resource.h"

#include <ctffunc.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <oleauto.h>
#include <olectl.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <fstream>
#include <initializer_list>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <shlwapi.h>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fp::tsf {
namespace {

std::wstring AsciiToWide(std::string_view value);
bool IsUsableScreenPoint(POINT point);
bool PtInRectInclusive(const RECT& rect, int x, int y);
std::filesystem::path ModuleDirectory();
std::filesystem::path InstalledSiblingExecutable(std::wstring_view name);
bool IsToolbarHostProcess();
bool IsAlphabetVirtualKey(WPARAM wparam);
char AlphabetVirtualKeyToLowerAscii(WPARAM wparam);

enum LangBarMenuCommand : UINT {
  kMenuFullShape = 100,
  kMenuInputMode,
  kMenuFullShapeFull,
  kMenuFullShapeHalf,
  kMenuPunctuation,
  kMenuPunctuationChinese,
  kMenuPunctuationEnglish,
  kMenuCharset,
  kMenuCharsetSimplified,
  kMenuCharsetTraditional,
  kMenuCandidateLayout,
  kMenuCandidateLayoutHorizontal,
  kMenuCandidateLayoutVertical,
  kMenuEmoji,
  kMenuCustomPhrases,
  kMenuLexiconManagement,
  kMenuKeyConfig,
  kMenuToolbar,
  kMenuSettings,
  kMenuRedeploy,
  kMenuAbout,
};

enum class CandidateFontFamily {
  kMiSans,
  kSourceHanSans,
};

constexpr int kDefaultCompactCandidateCount = 7;
constexpr int kMinCompactCandidateCount = 3;
constexpr int kMaxCompactCandidateCount = 9;
constexpr int kExpandedCandidateMaxColumns = 9;
constexpr int kVerticalExpandedCandidateColumns = 4;
constexpr int kVerticalExpandedCandidateRows = 9;
[[maybe_unused]] constexpr int kExpandedCandidatePageSize =
    kDefaultCompactCandidateCount + kExpandedCandidateMaxColumns * 3;
constexpr int kCandidateFontPointSize = 11;
constexpr int kDefaultCandidateFontSizeLevel = 0;
constexpr int kMinCandidateFontSizeLevel = 0;
constexpr int kMaxCandidateFontSizeLevel = 3;
constexpr std::array<int, 4> kCandidateFontPointSizes{11, 12, 13, 14};
constexpr std::wstring_view kDefaultCandidateFontFamily = L"misans";
constexpr std::wstring_view kDefaultStatusTipBlacklist =
    L"explorer.exe,fluent-pinyin-settings.exe,ShellExperienceHost.exe,StartMenuExperienceHost.exe";
constexpr int kCandidateToolButtonSize = 18;
constexpr int kCandidateToolFeedbackSize = 20;
constexpr int kCandidateToolGap = 4;
constexpr int kCandidateToolOuterInset = 15;
constexpr int kCandidateToolSeparatorInset = 14;
constexpr int kCandidateToolCandidateGap = 6;
constexpr int kCandidateAnchorTopOffsetHalfDips = -1;
constexpr int kCandidateAnchorUnderlineClearanceHalfDips = 4;
// Keep compact horizontal sizing stable: it matches the native compact IME
// baseline. Expanded horizontal mode reuses the compact row width/tool geometry
// and only appends candidate rows below it.
constexpr int kCandidateWindowCompactMaxWidth = 1600;
constexpr int kCandidateWindowExpandedMaxWidth = 1600;
constexpr UINT_PTR kStatusTipHideTimer = 1;
constexpr UINT_PTR kCandidateWindowWatchTimer = 2;
constexpr UINT_PTR kCandidateToolTooltipTimer = 3;
constexpr UINT_PTR kControlInputCoreRestartTimer = 5;
constexpr UINT_PTR kToolbarWindowWatchTimer = 1;
constexpr UINT_PTR kToolbarTooltipTimer = 2;
constexpr UINT kStatusTipHideDelayMs = 1200;
constexpr UINT kCandidateToolTooltipDelayMs = 500;
constexpr UINT kToolbarTooltipDelayMs = 180;
constexpr DWORD kCandidateToolTooltipAnimationMs = 90;
constexpr DWORD kToolbarTooltipAnimationMs = 90;
constexpr DWORD kStatusTipSettingsRefreshMs = 1500;
constexpr int kCandidateWindowClassExtraLastWidth = 0;
constexpr int kCandidateWindowClassExtraLastHeight = sizeof(LONG_PTR);
constexpr int kCandidateToolTooltipFontPointSize = 11;
constexpr int kCandidateToolTooltipHeightDips = 33;
constexpr int kCandidateToolTooltipOverlapDips = 6;
constexpr int kCandidateToolTooltipPaddingDips = 10;
constexpr int kCandidateToolTooltipOverhangGuardDips = 2;
constexpr int kCandidateToolTooltipCornerRadiusHalfDips = 5;
constexpr int kStatusTipTextIconGapDips = 6;
constexpr int kStatusTipIconSizeDips = 24;
constexpr float kStatusTipDetailIconScale = 0.78f;
constexpr int kStatusTipMouseOffsetXDips = 4;
constexpr int kStatusTipMouseOffsetYDips = 7;
constexpr int kHorizontalExpandedTailRows = 3;
constexpr int kHorizontalExpandedFooterGapDips = 2;
constexpr int kHorizontalExpandedFooterHeightDips = 41;
constexpr int kHorizontalExpandedHeaderSeparatorInsetDips = 5;
constexpr int kHorizontalExpandedFooterSeparatorInsetDips = 4;
constexpr int kHorizontalExpandedSettingsBottomInsetDips = 11;
constexpr int kHorizontalExpandedBrandLeftInsetDips = 11;
constexpr int kHorizontalExpandedBrandVerticalInsetDips = 8;
constexpr int kHorizontalExpandedBrandIconSizeDips = 17;
constexpr int kHorizontalExpandedBrandTextGapDips = 6;
constexpr int kHorizontalExpandedBrandTextPointSize = 11;
constexpr int kHorizontalExpandedBrandRightPaddingDips = 0;
constexpr int kHorizontalExpandedBrandSettingsGapDips = 18;
constexpr int kVerticalExpandedBrandRailWidthDips = kHorizontalExpandedFooterHeightDips;
constexpr int kVerticalFooterGapDips = 2;
constexpr int kVerticalFooterHeightDips = 40;
constexpr int kVerticalFooterSeparatorInsetDips = 4;
constexpr int kVerticalCandidateTopDips = 2;
constexpr int kVerticalCandidateRowHeightDips = 33;
constexpr int kVerticalCandidateRowStepDips = 34;
constexpr int kVerticalCompactToolColumnInsetDips = 4;
constexpr int kVerticalCompactToolColumnLeftDips = 4;
constexpr int kVerticalCompactToolIconRightInsetDips = 13;
constexpr int kVerticalExpandedColumnGapDips = 7;
constexpr int kVerticalCompactMinItemWidthDips = 44;
constexpr int kVerticalExpandedMinColumnWidthDips = 82;
constexpr int kToolbarWidthDips = 216;
constexpr int kToolbarHeightDips = 38;
constexpr int kToolbarCornerRadiusDips = 8;
constexpr int kToolbarOuterInsetPixels = 2;
constexpr int kToolbarBorderPixels = 2;
constexpr int kToolbarDragWidthDips = 24;
constexpr int kToolbarItemSlotDips = 32;
constexpr int kToolbarTailPaddingHalfDips = 4;
constexpr int kToolbarSeparatorTopDips = 0;
constexpr int kToolbarSeparatorBottomDips = 0;
constexpr int kToolbarGripLeftDips = 11;
constexpr int kToolbarGripTopDips = 10;
constexpr int kToolbarGripWidthDips = 3;
constexpr int kToolbarGripHeightDips = 16;
constexpr int kToolbarTextPointSize = 16;
constexpr int kToolbarTextVerticalOffsetHalfDips = 0;
constexpr int kToolbarFeedbackWidthDips = 24;
constexpr int kToolbarFeedbackHeightDips = 28;
constexpr int kToolbarFeedbackEdgeInsetDips = 4;
constexpr int kToolbarTooltipMinWidthDips = 0;
constexpr int kToolbarTooltipFontPointSize = 9;
constexpr int kToolbarTooltipHeightDips = 30;
constexpr int kToolbarTooltipPaddingDips = 8;
constexpr int kToolbarTooltipOverhangGuardDips = 2;
constexpr int kToolbarTooltipCornerRadiusHalfDips = 7;
constexpr int kToolbarTooltipMouseOffsetYDips = 9;
constexpr int kToolbarDefaultRightInsetDips = 16;
constexpr int kToolbarDefaultBottomInsetDips = 16;
constexpr int kContextMenuWidthDips = 202;
constexpr int kContextMenuRowHeightHalfDips = 67;
constexpr int kContextMenuCornerRadiusDips = 7;
constexpr int kContextMenuTextPointSize = 11;
constexpr int kContextMenuTextLeftDips = 72;
constexpr int kContextMenuIconLeftDips = 42;
constexpr int kContextMenuIconSizeDips = 18;
constexpr int kContextMenuChevronRightDips = 17;
constexpr int kContextMenuChevronSizeDips = 16;
constexpr int kContextMenuTaskbarGapDips = 8;
constexpr int kContextMenuScreenMarginDips = 6;
constexpr int kContextMenuHoverInsetXDips = 3;
constexpr int kContextMenuHoverInsetYHalfDips = 3;
constexpr int kContextMenuHoverRadiusDips = 4;
constexpr int kContextSubmenuShapeWidthDips = 102;
constexpr int kContextSubmenuCharsetWidthDips = 118;
constexpr int kToolbarCustomSubmenuWidthDips = 222;
constexpr int kToolbarGearMenuWidthDips = 128;
constexpr int kToolbarGearMenuTextLeftDips = 15;
constexpr int kToolbarGearMenuTextRightDips = 12;
constexpr int kToolbarGearMenuChevronRightDips = 10;
constexpr int kToolbarCustomSubmenuIconLeftDips = 42;
constexpr int kToolbarCustomSubmenuIconSizeDips = 18;
constexpr int kToolbarCustomSubmenuTextLeftDips = 72;
constexpr int kToolbarCustomSubmenuTextRightDips = 12;
constexpr int kToolbarMenuTextIconPointSize = 11;
constexpr int kContextSubmenuTextLeftDips = 46;
constexpr int kContextSubmenuCheckLeftDips = 18;
constexpr int kContextSubmenuCheckSizeDips = 16;
constexpr int kContextSubmenuRadioDotSizeDips = 6;
constexpr int kContextSubmenuOverlapDips = 2;
constexpr std::wstring_view kFluentPinyinWebsiteUrl = L"";
constexpr std::wstring_view kToolbarVisibleSetting = L"toolbar_visible_v2";
constexpr std::wstring_view kToolbarPositionUserSetting = L"toolbar_position_user_v2";
constexpr std::wstring_view kToolbarPositionSetting = L"toolbar_position_v2";
constexpr std::wstring_view kToolbarLayoutSetting = L"toolbar_layout";
constexpr std::wstring_view kToolbarItemsSetting = L"toolbar_items";
constexpr wchar_t kToolbarHostControlWindowClassName[] = L"FluentPinyinToolbarHostControlWindowV3";
constexpr wchar_t kToolbarWindowClassName[] = L"FluentPinyinToolbarWindowV3";
constexpr wchar_t kToolbarTooltipWindowClassName[] = L"FluentPinyinToolbarTooltipWindowV3";
constexpr wchar_t kToolbarWindowOwnerMutexName[] = L"Local\\FluentPinyin.Toolbar.WindowOwner.V3";

UINT RestartInputCoreMessage() {
  static const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kRestartInputCoreMessageName).c_str());
  return message;
}

UINT ShutdownInputCoreMessage() {
  static const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kShutdownInputCoreMessageName).c_str());
  return message;
}

UINT ApplyInputConfigMessage() {
  static const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kApplyInputConfigMessageName).c_str());
  return message;
}

UINT RefreshInputStateMessage() {
  static const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kRefreshInputStateMessageName).c_str());
  return message;
}

UINT LegacyRefreshInputStateMessage() {
  static const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kLegacyRefreshInputStateMessageName).c_str());
  return message;
}

void RequestInputStateRefresh() {
  const UINT message = RefreshInputStateMessage();
  if (message != 0) {
    PostMessageW(HWND_BROADCAST, message, 0, 0);
  }
}

UINT ToolbarRefreshMessage() {
  static const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kToolbarRefreshMessageName).c_str());
  return message;
}

UINT LegacyToolbarRefreshMessage() {
  static const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kLegacyToolbarRefreshMessageName).c_str());
  return message;
}

UINT ToolbarHostShutdownMessage() {
  static const UINT message =
      RegisterWindowMessageW(std::wstring(fp::kToolbarHostShutdownMessageName).c_str());
  return message;
}

void RequestToolbarHostRefresh() {
  const UINT message = ToolbarRefreshMessage();
  if (message != 0) {
    PostMessageW(HWND_BROADCAST, message, 0, 0);
  }
  const UINT legacy_message = LegacyToolbarRefreshMessage();
  if (legacy_message != 0 && legacy_message != message) {
    PostMessageW(HWND_BROADCAST, legacy_message, 0, 0);
  }
}

void RequestToolbarHostShutdown() {
  const UINT message = ToolbarHostShutdownMessage();
  if (message != 0) {
    PostMessageW(HWND_BROADCAST, message, 0, 0);
  }
}

bool ToolbarHostControlWindowExists() {
  return FindWindowW(kToolbarHostControlWindowClassName, nullptr) != nullptr;
}

void LogTsfPerfIfSlow(std::wstring_view operation,
                      ULONGLONG elapsed_ms,
                      ULONGLONG threshold_ms = 20,
                      std::wstring_view detail = L"") {
  if (elapsed_ms < threshold_ms) {
    return;
  }
  std::wstring message(operation);
  message += L" took ";
  message += std::to_wstring(elapsed_ms);
  message += L" ms";
  if (!detail.empty()) {
    message += L" (";
    message += detail;
    message += L")";
  }
  message += L".";
  fp::LogInfo(L"tsf", message);
}

void StartToolbarHost() {
  RequestToolbarHostRefresh();
}

void RefreshToolbarHostIfVisible(bool visible) {
  (void)visible;
  RequestToolbarHostRefresh();
}

enum CandidateToolId : int {
  kCandidateToolNone = 0,
  kCandidateToolPrevious = 1,
  kCandidateToolNext = 2,
  kCandidateToolEmoji = 3,
  kCandidateToolExpand = 4,
  kCandidateToolSettings = 5,
  kCandidateToolBrand = 6,
};

enum ToolbarItemId : int {
  kToolbarItemNone = 0,
  kToolbarItemDrag = 1,
  kToolbarItemInputMode = 2,
  kToolbarItemShape = 3,
  kToolbarItemPunctuation = 4,
  kToolbarItemCharset = 5,
  kToolbarItemEmoji = 6,
  kToolbarItemSettings = 7,
};

struct ToolbarItemDefinition {
  int id;
  std::wstring_view setting_id;
  std::wstring_view custom_label;
};

constexpr std::array<ToolbarItemDefinition, 5> kToolbarCustomItemDefinitions{{
    {kToolbarItemInputMode, L"input_mode", L"\u4E2D/\u82F1\u6587"},
    {kToolbarItemShape, L"shape", L"\u5168/\u534A\u89D2"},
    {kToolbarItemPunctuation, L"punctuation", L"\u4E2D/\u82F1\u6587\u6807\u70B9"},
    {kToolbarItemCharset, L"charset", L"\u7B80\u4F53/\u7E41\u4F53\u4E2D\u6587\u5B57\u7B26"},
    {kToolbarItemEmoji, L"emoji", L"\u8868\u60C5\u7B26\u53F7/\u7B26\u53F7"},
}};

bool IsToolbarCustomItem(int item) {
  return std::any_of(kToolbarCustomItemDefinitions.begin(),
                     kToolbarCustomItemDefinitions.end(),
                     [item](const ToolbarItemDefinition& definition) {
                       return definition.id == item;
                     });
}

std::wstring_view ToolbarItemSettingId(int item) {
  for (const auto& definition : kToolbarCustomItemDefinitions) {
    if (definition.id == item) {
      return definition.setting_id;
    }
  }
  return {};
}

std::wstring ToolbarCustomItemLabel(int item) {
  for (const auto& definition : kToolbarCustomItemDefinitions) {
    if (definition.id == item) {
      return std::wstring(definition.custom_label);
    }
  }
  return {};
}

std::vector<int> DefaultToolbarVisibleItems() {
  std::vector<int> items;
  items.reserve(kToolbarCustomItemDefinitions.size());
  for (const auto& definition : kToolbarCustomItemDefinitions) {
    items.push_back(definition.id);
  }
  items.push_back(kToolbarItemSettings);
  return items;
}

enum ContextMenuRow : int {
  kContextMenuRowShape = 0,
  kContextMenuRowCharset = 1,
  kContextMenuRowEmoji = 2,
  kContextMenuRowCustomPhrases = 3,
  kContextMenuRowDictionaries = 4,
  kContextMenuRowKeyConfig = 5,
  kContextMenuRowToolbar = 6,
  kContextMenuRowSettings = 7,
  kContextMenuRowRestart = 8,
  kContextMenuRowCount = 9,
};

enum ToolbarGearMenuRow : int {
  kToolbarGearMenuRowCustom = 0,
  kToolbarGearMenuRowLayout = 1,
  kToolbarGearMenuRowHide = 2,
  kToolbarGearMenuRowSettings = 3,
  kToolbarGearMenuRowCount = 4,
};

bool ContextMenuHasSeparatorAfter(int row, bool toolbar_mode = false) {
  if (toolbar_mode) {
    return false;
  }
  return row == kContextMenuRowCharset || row == kContextMenuRowEmoji ||
         row == kContextMenuRowDictionaries || row == kContextMenuRowKeyConfig ||
         row == kContextMenuRowToolbar;
}

bool ContextMenuRowHasSubmenu(int row, bool toolbar_mode = false) {
  if (toolbar_mode) {
    return row == kToolbarGearMenuRowCustom;
  }
  return row == kContextMenuRowShape || row == kContextMenuRowCharset;
}

int ContextMenuRowCount(bool toolbar_mode) {
  return toolbar_mode ? kToolbarGearMenuRowCount : kContextMenuRowCount;
}

int ContextMenuWidthDips(bool toolbar_mode) {
  return toolbar_mode ? kToolbarGearMenuWidthDips : kContextMenuWidthDips;
}

int ContextMenuTextLeftDips(bool toolbar_mode) {
  return toolbar_mode ? kToolbarGearMenuTextLeftDips : kContextMenuTextLeftDips;
}

int ContextMenuTextRightDips(bool toolbar_mode) {
  return toolbar_mode ? kToolbarGearMenuTextRightDips : 16;
}

int ContextMenuChevronRightDips(bool toolbar_mode) {
  return toolbar_mode ? kToolbarGearMenuChevronRightDips : kContextMenuChevronRightDips;
}

int ContextSubmenuWidthDips(int parent_row, bool toolbar_mode = false) {
  if (toolbar_mode) {
    return kToolbarCustomSubmenuWidthDips;
  }
  return parent_row == kContextMenuRowCharset ? kContextSubmenuCharsetWidthDips
                                              : kContextSubmenuShapeWidthDips;
}

UINT ContextMenuCommandForRow(int row, bool toolbar_mode = false) {
  if (toolbar_mode) {
    switch (row) {
      case kToolbarGearMenuRowLayout:
      case kToolbarGearMenuRowHide:
      case kToolbarGearMenuRowSettings:
        return 1;
      default:
        return 0;
    }
  }
  switch (row) {
    case kContextMenuRowShape:
    case kContextMenuRowCharset:
      return 0;
    case kContextMenuRowEmoji:
      return kMenuEmoji;
    case kContextMenuRowCustomPhrases:
      return kMenuCustomPhrases;
    case kContextMenuRowDictionaries:
      return kMenuLexiconManagement;
    case kContextMenuRowKeyConfig:
      return kMenuKeyConfig;
    case kContextMenuRowToolbar:
      return kMenuToolbar;
    case kContextMenuRowSettings:
      return kMenuSettings;
    case kContextMenuRowRestart:
      return kMenuRedeploy;
    default:
      return 0;
  }
}

UINT ContextSubmenuCommandForRow(int parent_row, int row, bool toolbar_mode = false) {
  if (row < 0) {
    return 0;
  }
  if (toolbar_mode) {
    return parent_row == kToolbarGearMenuRowCustom ? 1 : 0;
  }
  if (row >= 2) {
    return 0;
  }
  if (parent_row == kContextMenuRowShape) {
    return row == 0 ? kMenuFullShapeHalf : kMenuFullShapeFull;
  }
  if (parent_row == kContextMenuRowCharset) {
    return row == 0 ? kMenuCharsetSimplified : kMenuCharsetTraditional;
  }
  return 0;
}

int CompactCandidateCount(int compact_count) {
  return std::clamp(compact_count, kMinCompactCandidateCount, kMaxCompactCandidateCount);
}

int ExpandedCandidateColumnCount(int compact_count) {
  return CompactCandidateCount(compact_count);
}

int ExpandedCandidateColumnCount(bool horizontal, int compact_count) {
  return horizontal ? ExpandedCandidateColumnCount(compact_count) : kVerticalExpandedCandidateColumns;
}

int ExpandedCandidateRowCount(bool horizontal) {
  return horizontal ? 1 + kHorizontalExpandedTailRows : kVerticalExpandedCandidateRows;
}

int ExpandedCandidatePageSize(bool horizontal, int compact_count) {
  (void)horizontal;
  return CompactCandidateCount(compact_count) +
         kMaxCompactCandidateCount * kHorizontalExpandedTailRows;
}

int ExpandedCandidatePageSize(int compact_count) {
  return ExpandedCandidatePageSize(true, compact_count);
}

int CandidateFontPointSizeForLevel(int level) {
  const int clamped = std::clamp(level, kMinCandidateFontSizeLevel, kMaxCandidateFontSizeLevel);
  return kCandidateFontPointSizes[static_cast<size_t>(clamped)];
}

int CandidateItemHeightDips(int base_dips, int candidate_font_point_size) {
  return base_dips + std::max(0, candidate_font_point_size - kCandidateFontPointSize) * 2;
}

int CandidateRowStepDips(int base_dips, int candidate_font_point_size) {
  return base_dips + std::max(0, candidate_font_point_size - kCandidateFontPointSize) * 2;
}

int ScaleCandidateSelectionMark(int base_pixels, int item_height, UINT dpi) {
  const int base_item_height =
      MulDiv(CandidateItemHeightDips(33, kCandidateFontPointSize), static_cast<int>(dpi), 96);
  return MulDiv(base_pixels, std::max(1, item_height), std::max(1, base_item_height));
}

const wchar_t* UiFontFamily(bool traditional = false) {
  return traditional ? L"MiSans TC" : L"MiSans";
}

const wchar_t* ToolbarTextFontFamily() {
  return UiFontFamily(false);
}

std::array<const wchar_t*, 4> UiFontFallbackFamilies(bool traditional) {
  return traditional
             ? std::array<const wchar_t*, 4>{L"MiSans TC", L"MiSans L3", L"MiSans", nullptr}
             : std::array<const wchar_t*, 4>{L"MiSans", L"MiSans TC", L"MiSans L3", nullptr};
}

CandidateFontFamily CandidateFontFamilyFromSetting(std::wstring_view value) {
  return value == L"source_han_sans" || value == L"plangothic"
             ? CandidateFontFamily::kSourceHanSans
             : CandidateFontFamily::kMiSans;
}

std::wstring NormalizeCandidateFontFamilySetting(std::wstring_view value) {
  return CandidateFontFamilyFromSetting(value) == CandidateFontFamily::kSourceHanSans
             ? std::wstring(L"source_han_sans")
             : std::wstring(kDefaultCandidateFontFamily);
}

const wchar_t* CandidateUiFontFamily(CandidateFontFamily family, bool traditional) {
  if (family == CandidateFontFamily::kSourceHanSans) {
    return traditional ? L"Source Han Sans TC" : L"Source Han Sans SC";
  }
  return UiFontFamily(traditional);
}

std::array<const wchar_t*, 5> CandidateUiFontFallbackFamilies(CandidateFontFamily family,
                                                              bool traditional) {
  if (family == CandidateFontFamily::kSourceHanSans) {
    return {CandidateUiFontFamily(family, traditional),
            L"Plangothic P1",
            L"Plangothic P2",
            L"MiSans L3",
            UiFontFamily(traditional)};
  }
  const auto ui_fallback = UiFontFallbackFamilies(traditional);
  return {ui_fallback[0], ui_fallback[1], ui_fallback[2], nullptr, nullptr};
}

enum class UiFontLoadSet {
  kBase,
  kCandidateFallback,
};

bool IsBaseUiFontFamily(const wchar_t* family) {
  if (family == nullptr) {
    return true;
  }
  const std::wstring_view name(family);
  return name == L"MiSans" || name == L"MiSans TC";
}

void AddPrivateFontIfExists(const std::filesystem::path& font_dir, const wchar_t* file) {
  const std::filesystem::path path = font_dir / file;
  std::error_code error;
  if (std::filesystem::exists(path, error)) {
    AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
  }
}

void EnsureUiFontsLoaded(UiFontLoadSet load_set = UiFontLoadSet::kBase) {
  static std::atomic_bool base_loaded = false;
  bool expected = false;
  const std::filesystem::path font_dir = ModuleDirectory() / L"fonts";
  if (base_loaded.compare_exchange_strong(expected, true)) {
    const wchar_t* base_font_files[] = {
        L"MiSans-Regular.ttf",
        L"MiSans-Medium.ttf",
        L"MiSans-Semibold.ttf",
        L"MiSansTC-Regular.ttf",
        L"MiSansTC-Medium.ttf",
        L"MiSansTC-Semibold.ttf",
    };
    for (const wchar_t* file : base_font_files) {
      AddPrivateFontIfExists(font_dir, file);
    }
  }

  if (load_set == UiFontLoadSet::kBase) {
    return;
  }

  static std::atomic_bool fallback_loaded = false;
  expected = false;
  if (!fallback_loaded.compare_exchange_strong(expected, true)) {
    return;
  }

  const wchar_t* fallback_font_files[] = {
      L"MiSansL3-Regular.ttf",
      L"SourceHanSansSC-Regular.otf",
      L"SourceHanSansTC-Regular.otf",
      L"PlangothicP1-Regular.ttf",
      L"PlangothicP2-Regular.ttf",
  };
  for (const wchar_t* file : fallback_font_files) {
    AddPrivateFontIfExists(font_dir, file);
  }
}

UINT DpiForWindow(HWND window) {
  if (window != nullptr && IsWindow(window)) {
    const UINT dpi = GetDpiForWindow(window);
    if (dpi != 0) {
      return dpi;
    }
  }

  HWND foreground = GetForegroundWindow();
  if (foreground != nullptr && IsWindow(foreground)) {
    const UINT dpi = GetDpiForWindow(foreground);
    if (dpi != 0) {
      return dpi;
    }
  }

  HWND desktop = GetDesktopWindow();
  if (desktop != nullptr) {
    const UINT dpi = GetDpiForWindow(desktop);
    if (dpi != 0) {
      return dpi;
    }
  }

  DWORD applied_dpi = 0;
  DWORD applied_dpi_size = sizeof(applied_dpi);
  if (RegGetValueW(HKEY_CURRENT_USER,
                   L"Control Panel\\Desktop\\WindowMetrics",
                   L"AppliedDPI",
                   RRF_RT_REG_DWORD,
                   nullptr,
                   &applied_dpi,
                   &applied_dpi_size) == ERROR_SUCCESS &&
      applied_dpi != 0) {
    return static_cast<UINT>(applied_dpi);
  }

  const UINT system_dpi = GetDpiForSystem();
  return system_dpi != 0 ? system_dpi : 96;
}

int ScaleForDpi(int value, UINT dpi) {
  return MulDiv(value, static_cast<int>(dpi), 96);
}

int ScaleHalfDipForDpi(int half_dips, UINT dpi) {
  return MulDiv(half_dips, static_cast<int>(dpi), 192);
}

int HairlineForDpi(UINT dpi) {
  (void)dpi;
  return 1;
}

void ApplySuggestedDpiRect(HWND window, LPARAM lparam, UINT flags = 0) {
  const auto* suggested = reinterpret_cast<const RECT*>(lparam);
  if (window == nullptr || suggested == nullptr) {
    return;
  }
  SetWindowPos(window,
               nullptr,
               suggested->left,
               suggested->top,
               suggested->right - suggested->left,
               suggested->bottom - suggested->top,
               SWP_NOACTIVATE | SWP_NOZORDER | flags);
}

int StatusTipDetailIconSize(UINT dpi) {
  const int base_size = ScaleForDpi(kStatusTipIconSizeDips, dpi);
  return std::max(
      1,
      static_cast<int>(std::lround(static_cast<float>(base_size) * kStatusTipDetailIconScale)));
}

UINT ReadableDpiForWindow(HWND window) {
  UINT dpi = DpiForWindow(window);
  if (dpi < 96) {
    dpi = 96;
  }
  if (dpi > 240) {
    dpi = 240;
  }
  return dpi;
}

UINT EffectiveDpiForMonitor(HMONITOR monitor) {
  if (monitor != nullptr) {
    using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore != nullptr) {
      auto get_dpi_for_monitor = reinterpret_cast<GetDpiForMonitorFn>(
          GetProcAddress(shcore, "GetDpiForMonitor"));
      if (get_dpi_for_monitor != nullptr) {
        UINT dpi_x = 0;
        UINT dpi_y = 0;
        constexpr int kMdtEffectiveDpi = 0;
        if (SUCCEEDED(get_dpi_for_monitor(monitor, kMdtEffectiveDpi, &dpi_x, &dpi_y)) &&
            dpi_x != 0) {
          FreeLibrary(shcore);
          return dpi_x;
        }
      }
      FreeLibrary(shcore);
    }
  }
  return DpiForWindow(nullptr);
}

UINT ReadableDpi(UINT dpi) {
  if (dpi < 96) {
    dpi = 96;
  }
  if (dpi > 240) {
    dpi = 240;
  }
  return dpi;
}

UINT ReadableDpiForPoint(POINT point) {
  const HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
  return ReadableDpi(EffectiveDpiForMonitor(monitor));
}

RECT WorkAreaForPoint(POINT point) {
  MONITORINFO monitor_info{};
  monitor_info.cbSize = sizeof(monitor_info);
  const HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
  if (GetMonitorInfoW(monitor, &monitor_info)) {
    return monitor_info.rcWork;
  }
  RECT work_area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  return work_area;
}

HFONT CreateUiFontForDpi(int point_size,
                         UINT dpi,
                         int weight = FW_NORMAL,
                         const wchar_t* family = nullptr,
                         DWORD quality = ANTIALIASED_QUALITY) {
  const wchar_t* font_family = family != nullptr ? family : UiFontFamily(false);
  EnsureUiFontsLoaded(IsBaseUiFontFamily(font_family) ? UiFontLoadSet::kBase
                                                      : UiFontLoadSet::kCandidateFallback);
  return CreateFontW(-MulDiv(point_size, static_cast<int>(dpi), 72),
                     0,
                     0,
                     0,
                     weight,
                     FALSE,
                     FALSE,
                     FALSE,
                     DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS,
                     quality,
                     DEFAULT_PITCH | FF_DONTCARE,
                     font_family);
}

HFONT CreateLayeredUiFontForDpi(int point_size,
                                UINT dpi,
                                int weight = FW_NORMAL,
                                const wchar_t* family = nullptr) {
  return CreateUiFontForDpi(point_size, dpi, weight, family, ANTIALIASED_QUALITY);
}

std::filesystem::path SettingsPath() {
  return fp::GetFpRoamingDataPath() / L"settings.ini";
}

std::wstring NarrowPath(const std::filesystem::path& path) {
  return path.wstring();
}

std::wstring NormalizeSettingLine(std::wstring line) {
  if (!line.empty() && line.front() == L'\ufeff') {
    line.erase(line.begin());
  }
  if (line.size() >= 3 && line[0] == L'\u00EF' && line[1] == L'\u00BB' &&
      line[2] == L'\u00BF') {
    line.erase(0, 3);
  }
  if (!line.empty() && line.back() == L'\r') {
    line.pop_back();
  }
  return line;
}

std::wstring DecodeMultiByteSetting(UINT code_page,
                                    DWORD flags,
                                    const char* data,
                                    size_t length) {
  if (data == nullptr || length == 0 ||
      length > static_cast<size_t>((std::numeric_limits<int>::max)())) {
    return {};
  }
  const int byte_count = static_cast<int>(length);
  const int wide_count = MultiByteToWideChar(code_page, flags, data, byte_count, nullptr, 0);
  if (wide_count <= 0) {
    return {};
  }
  std::wstring text(static_cast<size_t>(wide_count), L'\0');
  const int written =
      MultiByteToWideChar(code_page, flags, data, byte_count, text.data(), wide_count);
  if (written <= 0) {
    return {};
  }
  text.resize(static_cast<size_t>(written));
  return text;
}

std::wstring DecodeSettingsBytes(const std::vector<char>& bytes) {
  if (bytes.empty()) {
    return {};
  }

  auto byte_at = [&bytes](size_t index) {
    return static_cast<unsigned char>(bytes[index]);
  };

  if (bytes.size() >= 2 && byte_at(0) == 0xFF && byte_at(1) == 0xFE) {
    std::wstring text;
    text.reserve((bytes.size() - 2) / 2);
    for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
      text.push_back(static_cast<wchar_t>(byte_at(i) | (byte_at(i + 1) << 8)));
    }
    return text;
  }

  if (bytes.size() >= 2 && byte_at(0) == 0xFE && byte_at(1) == 0xFF) {
    std::wstring text;
    text.reserve((bytes.size() - 2) / 2);
    for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
      text.push_back(static_cast<wchar_t>((byte_at(i) << 8) | byte_at(i + 1)));
    }
    return text;
  }

  size_t offset = 0;
  if (bytes.size() >= 3 && byte_at(0) == 0xEF && byte_at(1) == 0xBB && byte_at(2) == 0xBF) {
    offset = 3;
  }

  const char* data = bytes.data() + offset;
  const size_t length = bytes.size() - offset;
  std::wstring text = DecodeMultiByteSetting(CP_UTF8, MB_ERR_INVALID_CHARS, data, length);
  if (!text.empty() || length == 0) {
    return text;
  }
  return DecodeMultiByteSetting(CP_ACP, 0, data, length);
}

std::string EncodeSettingsUtf8(std::wstring_view text) {
  if (text.empty() ||
      text.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
    return {};
  }
  const int wide_count = static_cast<int>(text.size());
  const int byte_count =
      WideCharToMultiByte(CP_UTF8, 0, text.data(), wide_count, nullptr, 0, nullptr, nullptr);
  if (byte_count <= 0) {
    return {};
  }
  std::string bytes(static_cast<size_t>(byte_count), '\0');
  const int written = WideCharToMultiByte(
      CP_UTF8, 0, text.data(), wide_count, bytes.data(), byte_count, nullptr, nullptr);
  if (written <= 0) {
    return {};
  }
  bytes.resize(static_cast<size_t>(written));
  return bytes;
}

std::vector<std::wstring> ReadSettingLines(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }

  input.seekg(0, std::ios::end);
  const std::streamoff size = input.tellg();
  if (size <= 0) {
    return {};
  }
  input.seekg(0, std::ios::beg);

  std::vector<char> bytes(static_cast<size_t>(size));
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  bytes.resize(static_cast<size_t>(std::max<std::streamsize>(0, input.gcount())));

  std::vector<std::wstring> lines;
  const std::wstring text = DecodeSettingsBytes(bytes);
  std::wistringstream stream(text);
  std::wstring line;
  while (std::getline(stream, line)) {
    lines.push_back(NormalizeSettingLine(std::move(line)));
  }
  return lines;
}

std::filesystem::file_time_type SettingsFileWriteTime(const std::filesystem::path& path) {
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return std::filesystem::file_time_type{};
  }
  const auto write_time = std::filesystem::last_write_time(path, error);
  return error ? std::filesystem::file_time_type{} : write_time;
}

struct SettingsCache {
  std::filesystem::path path;
  std::filesystem::file_time_type write_time{};
  bool loaded = false;
  std::vector<std::wstring> lines;
  std::unordered_map<std::wstring, std::vector<size_t>> line_indices;
};

std::mutex& SettingsCacheMutex() {
  static std::mutex mutex;
  return mutex;
}

SettingsCache& MutableSettingsCache() {
  static SettingsCache cache;
  return cache;
}

void RebuildSettingsIndex(SettingsCache& cache) {
  cache.line_indices.clear();
  for (size_t index = 0; index < cache.lines.size(); ++index) {
    const std::wstring& line = cache.lines[index];
    const size_t equals = line.find(L'=');
    if (equals == std::wstring::npos) {
      continue;
    }
    cache.line_indices[line.substr(0, equals)].push_back(index);
  }
}

void EnsureSettingsCacheLoadedLocked(SettingsCache& cache) {
  const auto path = SettingsPath();
  const auto write_time = SettingsFileWriteTime(path);
  if (cache.loaded && cache.path == path && cache.write_time == write_time) {
    return;
  }

  cache.path = path;
  cache.write_time = write_time;
  cache.loaded = true;
  cache.lines = ReadSettingLines(path);
  RebuildSettingsIndex(cache);
}

void FlushSettingLines(const std::filesystem::path& path,
                       const std::vector<std::wstring>& lines) {
  fp::EnsureDirectory(path.parent_path());
  std::wstring content;
  for (const auto& line : lines) {
    content += line;
    content += L"\n";
  }
  const std::string bytes = EncodeSettingsUtf8(content);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!bytes.empty()) {
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }
}

std::optional<std::wstring> FindSettingValue(std::wstring_view key) {
  std::lock_guard lock(SettingsCacheMutex());
  auto& cache = MutableSettingsCache();
  EnsureSettingsCacheLoadedLocked(cache);
  const auto found = cache.line_indices.find(std::wstring(key));
  if (found != cache.line_indices.end() && !found->second.empty()) {
    const std::wstring& line = cache.lines[found->second.front()];
    const size_t equals = line.find(L'=');
    if (equals != std::wstring::npos) {
      return line.substr(equals + 1);
    }
  }
  return std::nullopt;
}

void UpsertSettingLine(std::vector<std::wstring>* lines,
                       std::wstring_view key,
                       std::wstring_view value) {
  if (lines == nullptr) {
    return;
  }
  const std::wstring prefix = std::wstring(key) + L"=";
  const std::wstring line_value = prefix + std::wstring(value);
  bool replaced = false;
  for (auto& line : *lines) {
    if (line.starts_with(prefix)) {
      line = line_value;
      replaced = true;
    }
  }
  if (!replaced) {
    lines->push_back(line_value);
  }
}

void WriteSettingLine(std::wstring_view key, std::wstring_view value) {
  const auto path = SettingsPath();
  std::lock_guard lock(SettingsCacheMutex());
  auto& cache = MutableSettingsCache();
  EnsureSettingsCacheLoadedLocked(cache);
  UpsertSettingLine(&cache.lines, key, value);
  FlushSettingLines(path, cache.lines);
  cache.path = path;
  cache.write_time = SettingsFileWriteTime(path);
  cache.loaded = true;
  RebuildSettingsIndex(cache);
}

void WriteStringSetting(std::wstring_view key, std::wstring_view value) {
  std::wstring sanitized(value);
  std::replace(sanitized.begin(), sanitized.end(), L'\r', L',');
  std::replace(sanitized.begin(), sanitized.end(), L'\n', L',');
  WriteSettingLine(key, sanitized);
}

void WriteBoolSetting(std::wstring_view key, bool value) {
  WriteSettingLine(key, value ? L"1" : L"0");
}

void WritePointSetting(std::wstring_view key, POINT point) {
  WriteSettingLine(key, std::to_wstring(point.x) + L"," + std::to_wstring(point.y));
}

std::wstring ReadStringSetting(std::wstring_view key, std::wstring_view default_value = L"") {
  const std::optional<std::wstring> value = FindSettingValue(key);
  return value.has_value() ? *value : std::wstring(default_value);
}

bool IsTruthySettingValue(std::wstring_view value) {
  return value == L"1" || value == L"true" || value == L"True";
}

std::optional<bool> ReadOptionalBoolSetting(std::wstring_view key) {
  const std::optional<std::wstring> value = FindSettingValue(key);
  if (!value.has_value()) {
    return std::nullopt;
  }
  return IsTruthySettingValue(*value);
}

bool ReadBoolSetting(std::wstring_view key, bool default_value) {
  const std::optional<bool> value = ReadOptionalBoolSetting(key);
  return value.has_value() ? *value : default_value;
}

std::wstring TrimShortcutDisplay(std::wstring value) {
  size_t first = 0;
  while (first < value.size() && std::iswspace(value[first])) {
    ++first;
  }
  size_t last = value.size();
  while (last > first && std::iswspace(value[last - 1])) {
    --last;
  }
  return value.substr(first, last - first);
}

std::wstring ShortcutDisplay(std::wstring_view setting_key, std::wstring_view fallback) {
  return TrimShortcutDisplay(ReadStringSetting(setting_key, fallback));
}

std::wstring TextWithShortcut(std::wstring_view label,
                              std::wstring_view setting_key,
                              std::wstring_view fallback) {
  std::wstring text(label);
  const std::wstring shortcut = ShortcutDisplay(setting_key, fallback);
  if (!shortcut.empty()) {
    text += L" (";
    text += shortcut;
    text += L")";
  }
  return text;
}

bool IsReasonableStoredScreenPoint(POINT point) {
  const int virtual_left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int virtual_top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int virtual_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int virtual_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  const int virtual_right = virtual_left + virtual_width;
  const int virtual_bottom = virtual_top + virtual_height;
  const int padding = std::max({4096, virtual_width, virtual_height});
  return point.x >= virtual_left - padding && point.x <= virtual_right + padding &&
         point.y >= virtual_top - padding && point.y <= virtual_bottom + padding;
}

std::optional<POINT> ReadPointSetting(std::wstring_view key) {
  const std::wstring value = ReadStringSetting(key);
  const size_t separator = value.find(L',');
  if (separator == std::wstring::npos) {
    return std::nullopt;
  }
  try {
    POINT point{std::stoi(value.substr(0, separator)),
                std::stoi(value.substr(separator + 1))};
    return IsReasonableStoredScreenPoint(point) ? std::optional<POINT>(point) : std::nullopt;
  } catch (...) {
    return std::nullopt;
  }
}

int ReadIntSetting(std::wstring_view key, int default_value, int min_value, int max_value) {
  const std::optional<std::wstring> value = FindSettingValue(key);
  if (!value.has_value()) {
    return default_value;
  }
  try {
    return std::clamp(std::stoi(*value), min_value, max_value);
  } catch (...) {
    return default_value;
  }
}

bool ReadBoolSettingMigrated(std::wstring_view key,
                             bool default_value,
                             std::wstring_view legacy_key) {
  if (const std::optional<bool> value = ReadOptionalBoolSetting(key)) {
    return *value;
  }
  return ReadBoolSetting(legacy_key, default_value);
}

bool ReadToolbarVisibleSetting(bool default_value) {
  if (const std::optional<bool> value = ReadOptionalBoolSetting(kToolbarVisibleSetting)) {
    return *value;
  }
  const std::optional<bool> legacy_value = ReadOptionalBoolSetting(L"toolbar_visible");
  return legacy_value.has_value() && *legacy_value ? true : default_value;
}

void WriteIntSetting(std::wstring_view key, int value) {
  WriteSettingLine(key, std::to_wstring(value));
}

SIZE MeasureText(HDC dc, const std::wstring& text) {
  SIZE size{};
  if (!text.empty()) {
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
  }
  return size;
}

bool FontHasText(HDC dc, const std::wstring& text) {
  if (text.empty()) {
    return true;
  }

  std::vector<WORD> glyphs(text.size());
  const DWORD result = GetGlyphIndicesW(dc,
                                        text.c_str(),
                                        static_cast<int>(text.size()),
                                        glyphs.data(),
                                        GGI_MARK_NONEXISTING_GLYPHS);
  if (result == GDI_ERROR) {
    return true;
  }
  return std::none_of(glyphs.begin(), glyphs.end(), [](WORD glyph) {
    return glyph == 0xFFFF;
  });
}

struct TextMeasureCacheEntry {
  std::wstring text;
  std::wstring face;
  int point_size = 0;
  UINT dpi = 0;
  int weight = FW_NORMAL;
  bool traditional = false;
  CandidateFontFamily candidate_font_family = CandidateFontFamily::kMiSans;
  SIZE size{};
};

std::mutex& TextMeasureCacheMutex() {
  static std::mutex mutex;
  return mutex;
}

std::vector<TextMeasureCacheEntry>& TextMeasureCache() {
  static std::vector<TextMeasureCacheEntry> cache;
  return cache;
}

bool TryGetCachedTextMeasure(const std::wstring& text,
                             const std::wstring& face,
                             int point_size,
                             UINT dpi,
                             int weight,
                             bool traditional,
                             CandidateFontFamily candidate_font_family,
                             SIZE* size) {
  if (size == nullptr || text.size() > 64) {
    return false;
  }
  std::lock_guard<std::mutex> lock(TextMeasureCacheMutex());
  for (const auto& entry : TextMeasureCache()) {
    if (entry.point_size == point_size && entry.dpi == dpi && entry.weight == weight &&
        entry.traditional == traditional &&
        entry.candidate_font_family == candidate_font_family && entry.text == text &&
        entry.face == face) {
      *size = entry.size;
      return true;
    }
  }
  return false;
}

void StoreCachedTextMeasure(const std::wstring& text,
                            const std::wstring& face,
                            int point_size,
                            UINT dpi,
                            int weight,
                            bool traditional,
                            CandidateFontFamily candidate_font_family,
                            SIZE size) {
  if (text.empty() || text.size() > 64) {
    return;
  }
  constexpr size_t kMaxTextMeasureCacheEntries = 768;
  std::lock_guard<std::mutex> lock(TextMeasureCacheMutex());
  auto& cache = TextMeasureCache();
  for (auto& entry : cache) {
    if (entry.point_size == point_size && entry.dpi == dpi && entry.weight == weight &&
        entry.traditional == traditional &&
        entry.candidate_font_family == candidate_font_family && entry.text == text &&
        entry.face == face) {
      entry.size = size;
      return;
    }
  }
  if (cache.size() >= kMaxTextMeasureCacheEntries) {
    cache.erase(cache.begin());
  }
  cache.push_back({text, face, point_size, dpi, weight, traditional, candidate_font_family, size});
}

std::wstring CurrentTextFace(HDC dc) {
  wchar_t face[LF_FACESIZE]{};
  if (dc == nullptr || GetTextFaceW(dc, LF_FACESIZE, face) <= 0) {
    return {};
  }
  return face;
}

bool TextFaceMatchesFamilyName(std::wstring_view face, std::wstring_view family) {
  if (face == family) {
    return true;
  }
  return face.size() > family.size() && face.substr(0, family.size()) == family &&
         std::iswspace(face[family.size()]) != 0;
}

bool TextFaceMatchesFamily(std::wstring_view face, std::wstring_view family) {
  if (TextFaceMatchesFamilyName(face, family)) {
    return true;
  }
  if (family == L"Source Han Sans SC") {
    return TextFaceMatchesFamilyName(face, L"思源黑体");
  }
  if (family == L"Source Han Sans TC") {
    return TextFaceMatchesFamilyName(face, L"思源黑體");
  }
  if (family == L"Plangothic P1") {
    return TextFaceMatchesFamilyName(face, L"遍黑体P1") ||
           TextFaceMatchesFamilyName(face, L"遍黑體P1");
  }
  if (family == L"Plangothic P2") {
    return TextFaceMatchesFamilyName(face, L"遍黑体P2") ||
           TextFaceMatchesFamilyName(face, L"遍黑體P2");
  }
  return false;
}

CandidateFontFamily CandidateFontFamilyForFace(std::wstring_view face) {
  return TextFaceMatchesFamily(face, L"Source Han Sans SC") ||
                 TextFaceMatchesFamily(face, L"Source Han Sans TC") ||
                 TextFaceMatchesFamily(face, L"思源黑体") ||
                 TextFaceMatchesFamily(face, L"思源黑體") ||
                 TextFaceMatchesFamily(face, L"Plangothic P1") ||
                 TextFaceMatchesFamily(face, L"Plangothic P2") ||
                 TextFaceMatchesFamily(face, L"遍黑体P1") ||
                 TextFaceMatchesFamily(face, L"遍黑体P2") ||
                 TextFaceMatchesFamily(face, L"遍黑體P1") ||
                 TextFaceMatchesFamily(face, L"遍黑體P2")
             ? CandidateFontFamily::kSourceHanSans
             : CandidateFontFamily::kMiSans;
}

bool MeasureTextWithFontFamily(HDC dc,
                               const std::wstring& text,
                               int point_size,
                               UINT dpi,
                               int weight,
                               const wchar_t* family,
                               const std::wstring& current_face,
                               SIZE* size) {
  if (family == nullptr || size == nullptr) {
    return false;
  }

  if (TextFaceMatchesFamily(current_face, family)) {
    if (!FontHasText(dc, text)) {
      return false;
    }
    *size = MeasureText(dc, text);
    return true;
  }

  HFONT font = CreateUiFontForDpi(point_size, dpi, weight, family);
  if (font == nullptr) {
    return false;
  }
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  const bool has_text = TextFaceMatchesFamily(CurrentTextFace(dc), family) && FontHasText(dc, text);
  if (has_text) {
    *size = MeasureText(dc, text);
  }
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
  return has_text;
}

bool DrawTextWithFontFamily(HDC dc,
                            const std::wstring& text,
                            RECT* rect,
                            UINT format,
                            int point_size,
                            UINT dpi,
                            int weight,
                            const wchar_t* family,
                            const std::wstring& current_face) {
  if (family == nullptr) {
    return false;
  }

  if (TextFaceMatchesFamily(current_face, family)) {
    if (!FontHasText(dc, text)) {
      return false;
    }
    DrawTextW(dc, text.c_str(), -1, rect, format);
    return true;
  }

  HFONT font = CreateUiFontForDpi(point_size, dpi, weight, family);
  if (font == nullptr) {
    return false;
  }
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  const bool has_text = TextFaceMatchesFamily(CurrentTextFace(dc), family) && FontHasText(dc, text);
  if (has_text) {
    DrawTextW(dc, text.c_str(), -1, rect, format);
  }
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
  return has_text;
}

size_t TextClusterLength(const std::wstring& text, size_t index) {
  if (index + 1 < text.size()) {
    const wchar_t lead = text[index];
    const wchar_t trail = text[index + 1];
    if (lead >= 0xD800 && lead <= 0xDBFF && trail >= 0xDC00 && trail <= 0xDFFF) {
      return 2;
    }
  }
  return 1;
}

struct FallbackTextRun {
  std::wstring text;
  const wchar_t* family = nullptr;
  SIZE size{};
};

void AppendFallbackTextRun(std::vector<FallbackTextRun>* runs,
                           const std::wstring& text,
                           const wchar_t* family,
                           SIZE size) {
  if (runs == nullptr || text.empty()) {
    return;
  }
  if (!runs->empty() && runs->back().family == family) {
    runs->back().text += text;
    runs->back().size.cx += size.cx;
    runs->back().size.cy = std::max(runs->back().size.cy, size.cy);
    return;
  }
  runs->push_back({text, family, size});
}

bool BuildFallbackTextRuns(HDC dc,
                           const std::wstring& text,
                           int point_size,
                           UINT dpi,
                           int weight,
                           bool traditional,
                           CandidateFontFamily candidate_font_family,
                           std::vector<FallbackTextRun>* runs,
                           SIZE* total_size) {
  if (dc == nullptr || text.empty() || runs == nullptr || total_size == nullptr) {
    return false;
  }

  runs->clear();
  *total_size = {};
  const std::wstring current_face = CurrentTextFace(dc);
  if (candidate_font_family == CandidateFontFamily::kMiSans) {
    candidate_font_family = CandidateFontFamilyForFace(current_face);
  }
  const auto fallback_families =
      CandidateUiFontFallbackFamilies(candidate_font_family, traditional);
  for (size_t index = 0; index < text.size();) {
    const size_t cluster_length = TextClusterLength(text, index);
    const std::wstring cluster = text.substr(index, cluster_length);
    index += cluster_length;

    bool measured = false;
    SIZE cluster_size{};
    const wchar_t* selected_family = nullptr;
    for (const wchar_t* family : fallback_families) {
      if (family == nullptr) {
        continue;
      }
      if (MeasureTextWithFontFamily(
              dc, cluster, point_size, dpi, weight, family, current_face, &cluster_size)) {
        selected_family = family;
        measured = true;
        break;
      }
    }

    if (!measured) {
      cluster_size = MeasureText(dc, cluster);
      measured = true;
    }

    if (measured) {
      total_size->cx += cluster_size.cx;
      total_size->cy = std::max(total_size->cy, cluster_size.cy);
      AppendFallbackTextRun(runs, cluster, selected_family, cluster_size);
    }
  }

  return !runs->empty();
}

void DrawFallbackTextRuns(HDC dc,
                          const std::vector<FallbackTextRun>& runs,
                          SIZE total_size,
                          RECT* rect,
                          UINT format,
                          int point_size,
                          UINT dpi,
                          int weight) {
  if (dc == nullptr || rect == nullptr || runs.empty()) {
    return;
  }

  const int available_width = std::max(0L, rect->right - rect->left);
  int x = rect->left;
  if ((format & DT_RIGHT) == DT_RIGHT) {
    x = rect->right - total_size.cx;
  } else if ((format & DT_CENTER) == DT_CENTER) {
    x = rect->left + (available_width - total_size.cx) / 2;
  }

  const int saved_dc = SaveDC(dc);
  if (saved_dc != 0) {
    IntersectClipRect(dc, rect->left, rect->top, rect->right, rect->bottom);
  }

  const std::wstring current_face = CurrentTextFace(dc);
  UINT run_format = format;
  run_format &= ~(DT_CENTER | DT_RIGHT | DT_END_ELLIPSIS);
  run_format |= DT_LEFT;
  for (const auto& run : runs) {
    RECT run_rect{x, rect->top, x + run.size.cx + 2, rect->bottom};
    if (run.family != nullptr) {
      DrawTextWithFontFamily(
          dc, run.text, &run_rect, run_format, point_size, dpi, weight, run.family, current_face);
    } else {
      DrawTextW(dc, run.text.c_str(), -1, &run_rect, run_format);
    }
    x += run.size.cx;
    if (x >= rect->right) {
      break;
    }
  }

  if (saved_dc != 0) {
    RestoreDC(dc, saved_dc);
  }
}

SIZE MeasureTextWithFallback(HDC dc,
                             const std::wstring& text,
                             int point_size,
                             UINT dpi,
                             int weight,
                             bool traditional = false,
                             CandidateFontFamily candidate_font_family = CandidateFontFamily::kMiSans) {
  SIZE size{};
  if (text.empty()) {
    return size;
  }

  const std::wstring current_face = CurrentTextFace(dc);
  if (candidate_font_family == CandidateFontFamily::kMiSans) {
    candidate_font_family = CandidateFontFamilyForFace(current_face);
  }
  if (TryGetCachedTextMeasure(text,
                              current_face,
                              point_size,
                              dpi,
                              weight,
                              traditional,
                              candidate_font_family,
                              &size)) {
    return size;
  }
  const auto fallback_families =
      CandidateUiFontFallbackFamilies(candidate_font_family, traditional);
  for (const wchar_t* family : fallback_families) {
    if (family != nullptr && TextFaceMatchesFamily(current_face, family) && FontHasText(dc, text)) {
      size = MeasureText(dc, text);
      StoreCachedTextMeasure(text,
                             current_face,
                             point_size,
                             dpi,
                             weight,
                             traditional,
                             candidate_font_family,
                             size);
      return size;
    }
  }
  for (const wchar_t* family : fallback_families) {
    if (MeasureTextWithFontFamily(dc, text, point_size, dpi, weight, family, current_face, &size)) {
      StoreCachedTextMeasure(text,
                             current_face,
                             point_size,
                             dpi,
                             weight,
                             traditional,
                             candidate_font_family,
                             size);
      return size;
    }
  }

  std::vector<FallbackTextRun> runs;
  if (BuildFallbackTextRuns(
          dc, text, point_size, dpi, weight, traditional, candidate_font_family, &runs, &size)) {
    StoreCachedTextMeasure(text,
                           current_face,
                           point_size,
                           dpi,
                           weight,
                           traditional,
                           candidate_font_family,
                           size);
    return size;
  }

  size = FontHasText(dc, text) ? MeasureText(dc, text) : size;
  StoreCachedTextMeasure(text,
                         current_face,
                         point_size,
                         dpi,
                         weight,
                         traditional,
                         candidate_font_family,
                         size);
  return size;
}

void DrawTextWithFallback(HDC dc,
                          const std::wstring& text,
                          RECT* rect,
                          UINT format,
                          int point_size,
                          UINT dpi,
                          int weight,
                          bool traditional = false,
                          CandidateFontFamily candidate_font_family = CandidateFontFamily::kMiSans) {
  if (text.empty()) {
    return;
  }

  const std::wstring current_face = CurrentTextFace(dc);
  if (candidate_font_family == CandidateFontFamily::kMiSans) {
    candidate_font_family = CandidateFontFamilyForFace(current_face);
  }
  const auto fallback_families =
      CandidateUiFontFallbackFamilies(candidate_font_family, traditional);
  for (const wchar_t* family : fallback_families) {
    if (family != nullptr && TextFaceMatchesFamily(current_face, family) && FontHasText(dc, text)) {
      DrawTextW(dc, text.c_str(), -1, rect, format);
      return;
    }
  }
  for (const wchar_t* family : fallback_families) {
    if (DrawTextWithFontFamily(dc, text, rect, format, point_size, dpi, weight, family, current_face)) {
      return;
    }
  }

  std::vector<FallbackTextRun> runs;
  SIZE total_size{};
  if (BuildFallbackTextRuns(
          dc, text, point_size, dpi, weight, traditional, candidate_font_family, &runs, &total_size)) {
    DrawFallbackTextRuns(dc, runs, total_size, rect, format, point_size, dpi, weight);
    return;
  }

  if (FontHasText(dc, text)) {
    DrawTextW(dc, text.c_str(), -1, rect, format);
  }
}

bool SystemUsesLightTheme() {
  DWORD value = 1;
  DWORD size = sizeof(value);
  const LSTATUS status =
      RegGetValueW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"SystemUsesLightTheme",
                   RRF_RT_REG_DWORD,
                   nullptr,
                   &value,
                   &size);
  return status != ERROR_SUCCESS || value != 0;
}

bool AppsUseLightTheme() {
  DWORD value = 1;
  DWORD size = sizeof(value);
  const LSTATUS status =
      RegGetValueW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"AppsUseLightTheme",
                   RRF_RT_REG_DWORD,
                   nullptr,
                   &value,
                   &size);
  return status != ERROR_SUCCESS || value != 0;
}

struct CachedIconEntry {
  bool light = false;
  int width = 0;
  int height = 0;
  HICON icon = nullptr;
};

HICON CachedFluentPinyinIcon(bool light, int width, int height) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  static std::mutex mutex;
  static std::vector<CachedIconEntry> cache;
  std::lock_guard<std::mutex> lock(mutex);
  for (const auto& entry : cache) {
    if (entry.light == light && entry.width == width && entry.height == height) {
      return entry.icon;
    }
  }

  HICON icon =
      static_cast<HICON>(LoadImageW(g_module_instance,
                                    MAKEINTRESOURCEW(light ? IDI_FLUENT_PINYIN_LIGHT
                                                           : IDI_FLUENT_PINYIN_DARK),
                                    IMAGE_ICON,
                                    width,
                                    height,
                                    LR_DEFAULTCOLOR));
  if (icon != nullptr) {
    cache.push_back({light, width, height, icon});
  }
  return icon;
}

struct CandidateWindowPalette {
  COLORREF background;
  COLORREF edge;
  COLORREF border;
  COLORREF text;
  COLORREF candidate_number;
  COLORREF muted;
  COLORREF disabled;
  COLORREF highlight;
  COLORREF accent;
  COLORREF separator;
  COLORREF expanded_separator;
  COLORREF page_active;
  COLORREF page_disabled;
  COLORREF tool_icon;
  COLORREF tool_hover;
  COLORREF tool_pressed;
  COLORREF brand_icon_background;
};

bool ThemeUsesLightMode(std::wstring_view mode, std::wstring_view preset) {
  const std::wstring effective = fp::EffectiveThemePreset(mode, preset, AppsUseLightTheme());
  return effective == fp::kThemePresetDefaultLight || effective == fp::kThemePresetWhite;
}

CandidateWindowPalette CandidatePalette(std::wstring_view mode, std::wstring_view preset) {
  auto color = [](fp::ThemeColor value) {
    return RGB(value.red, value.green, value.blue);
  };
  const auto palette =
      fp::ThemePaletteForPreset(fp::EffectiveThemePreset(mode, preset, AppsUseLightTheme()));
  return {
      color(palette.window_background),
      color(palette.border),
      color(palette.border),
      color(palette.text),
      color(palette.light ? fp::ThemeColor{24, 24, 24} : palette.secondary_text),
      color(palette.light ? palette.secondary_text : palette.text),
      color(palette.disabled_text),
      color(palette.highlight),
      color(palette.accent),
      color(palette.separator),
      color(palette.separator),
      color(palette.text),
      color(palette.disabled_text),
      color(palette.text),
      color(palette.button_hover),
      color(palette.button_pressed),
      color(palette.window_background),
  };
}

COLORREF FloatingWindowEdgeColor(std::wstring_view mode, std::wstring_view preset) {
  return CandidatePalette(mode, preset).edge;
}

COLORREF ToolbarTextColorForTheme(std::wstring_view mode, std::wstring_view preset) {
  const std::wstring effective = fp::EffectiveThemePreset(mode, preset, AppsUseLightTheme());
  if (effective == fp::kThemePresetDefaultLight || effective == fp::kThemePresetWhite) {
    return RGB(32, 32, 32);
  }
  return RGB(226, 226, 226);
}

int FloatingWindowBorderWidth(UINT dpi) {
  return HairlineForDpi(dpi);
}

void ApplyCandidateDwmFrame(HWND window, std::wstring_view mode, std::wstring_view preset) {
  if (window == nullptr) {
    return;
  }
  constexpr DWORD kDwmBorderColor = 34;
  const COLORREF border_color = FloatingWindowEdgeColor(mode, preset);
  DwmSetWindowAttribute(window, kDwmBorderColor, &border_color, sizeof(border_color));
}

COLORREF TrayTextColor() {
  HIGHCONTRASTW high_contrast{};
  high_contrast.cbSize = sizeof(high_contrast);
  if (SystemParametersInfoW(SPI_GETHIGHCONTRAST,
                            sizeof(high_contrast),
                            &high_contrast,
                            0) &&
      (high_contrast.dwFlags & HCF_HIGHCONTRASTON) != 0) {
    return GetSysColor(COLOR_WINDOWTEXT);
  }
  return SystemUsesLightTheme() ? RGB(32, 32, 32) : RGB(245, 245, 245);
}

COLORREF TrayInverseTextColor() {
  return SystemUsesLightTheme() ? RGB(245, 245, 245) : RGB(32, 32, 32);
}

std::wstring GuidToString(REFGUID guid) {
  wchar_t buffer[64]{};
  StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
  return buffer;
}

std::wstring TipLanguageProfileKey() {
  return L"Software\\Microsoft\\CTF\\TIP\\" + GuidToString(kTextServiceClsid) +
         L"\\LanguageProfile\\0x00000804\\" + GuidToString(kProfileGuid);
}

std::filesystem::path ProfileIconPathForSystemTheme() {
  const auto icon = ModuleDirectory() / L"fluent-pinyin.ico";
  if (GetFileAttributesW(icon.c_str()) != INVALID_FILE_ATTRIBUTES) {
    return icon;
  }
  return {};
}

bool RegistryStringEquals(HKEY root,
                          const std::wstring& subkey,
                          const wchar_t* name,
                          const std::wstring& value) {
  std::wstring existing(32768, L'\0');
  DWORD size = static_cast<DWORD>(existing.size() * sizeof(wchar_t));
  DWORD type = 0;
  const LSTATUS status = RegGetValueW(root,
                                      subkey.c_str(),
                                      name,
                                      RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                                      &type,
                                      existing.data(),
                                      &size);
  if (status != ERROR_SUCCESS || size == 0) {
    return false;
  }
  existing.resize((size / sizeof(wchar_t)) - 1);
  return _wcsicmp(existing.c_str(), value.c_str()) == 0;
}

bool SetRegistryExpandableStringIfChanged(HKEY root,
                                          const std::wstring& subkey,
                                          const wchar_t* name,
                                          const std::wstring& value) {
  if (RegistryStringEquals(root, subkey, name, value)) {
    return false;
  }

  HKEY key = nullptr;
  const LSTATUS open_status = RegCreateKeyExW(root,
                                              subkey.c_str(),
                                              0,
                                              nullptr,
                                              REG_OPTION_NON_VOLATILE,
                                              KEY_WRITE,
                                              nullptr,
                                              &key,
                                              nullptr);
  if (open_status != ERROR_SUCCESS) {
    return false;
  }
  const LSTATUS write_status =
      RegSetValueExW(key,
                     name,
                     0,
                     REG_EXPAND_SZ,
                     reinterpret_cast<const BYTE*>(value.c_str()),
                     static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
  return write_status == ERROR_SUCCESS;
}

bool SetRegistryDwordIfChanged(HKEY root,
                               const std::wstring& subkey,
                               const wchar_t* name,
                               DWORD value) {
  DWORD existing = 0;
  DWORD size = sizeof(existing);
  const LSTATUS read_status = RegGetValueW(root,
                                           subkey.c_str(),
                                           name,
                                           RRF_RT_REG_DWORD,
                                           nullptr,
                                           &existing,
                                           &size);
  if (read_status == ERROR_SUCCESS && existing == value) {
    return false;
  }

  HKEY key = nullptr;
  const LSTATUS open_status = RegCreateKeyExW(root,
                                              subkey.c_str(),
                                              0,
                                              nullptr,
                                              REG_OPTION_NON_VOLATILE,
                                              KEY_WRITE,
                                              nullptr,
                                              &key,
                                              nullptr);
  if (open_status != ERROR_SUCCESS) {
    return false;
  }
  const LSTATUS write_status =
      RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
  RegCloseKey(key);
  return write_status == ERROR_SUCCESS;
}

void RefreshProfileIconForSystemTheme() {
  const auto icon_path = ProfileIconPathForSystemTheme();
  if (icon_path.empty()) {
    return;
  }

  const std::wstring icon_value = icon_path.wstring();
  const std::wstring profile_key = TipLanguageProfileKey();
  bool changed = false;
  changed |= SetRegistryExpandableStringIfChanged(
      HKEY_CURRENT_USER, profile_key, L"IconFile", icon_value);
  changed |= SetRegistryDwordIfChanged(HKEY_CURRENT_USER, profile_key, L"IconIndex", 0);
  changed |= SetRegistryExpandableStringIfChanged(
      HKEY_LOCAL_MACHINE, profile_key, L"IconFile", icon_value);
  changed |= SetRegistryDwordIfChanged(HKEY_LOCAL_MACHINE, profile_key, L"IconIndex", 0);
  if (changed) {
    SendNotifyMessageW(HWND_BROADCAST,
                       WM_SETTINGCHANGE,
                       0,
                       reinterpret_cast<LPARAM>(L"Software\\Microsoft\\CTF\\TIP"));
  }
}

struct ToolbarPalette {
  COLORREF background;
  COLORREF drag_background;
  COLORREF border;
  COLORREF separator;
  COLORREF drag_separator;
  COLORREF text;
  COLORREF muted;
  COLORREF hover;
  COLORREF pressed;
  COLORREF tooltip_background;
  COLORREF tooltip_text;
  COLORREF context_background;
  COLORREF context_separator;
  COLORREF context_text;
  COLORREF context_hover;
};

ToolbarPalette ToolbarPaletteForTheme(std::wstring_view mode, std::wstring_view preset) {
  auto color = [](fp::ThemeColor value) {
    return RGB(value.red, value.green, value.blue);
  };
  const auto theme =
      fp::ThemePaletteForPreset(fp::EffectiveThemePreset(mode, preset, AppsUseLightTheme()));
  const CandidateWindowPalette candidate = CandidatePalette(mode, preset);
  return {
      color(theme.toolbar_background),
      color(theme.toolbar_drag),
      color(theme.border),
      color(theme.separator),
      color(theme.border),
      ToolbarTextColorForTheme(mode, preset),
      candidate.disabled,
      candidate.tool_hover,
      candidate.tool_pressed,
      color(theme.toolbar_background),
      candidate.text,
      color(theme.window_background),
      candidate.separator,
      candidate.text,
      candidate.highlight,
  };
}

ToolbarPalette ToolbarWindowPaletteForTheme(std::wstring_view mode, std::wstring_view preset) {
  ToolbarPalette palette = ToolbarPaletteForTheme(mode, preset);
  const std::wstring effective =
      fp::EffectiveThemePreset(mode, preset, AppsUseLightTheme());
  if (effective == fp::kThemePresetDefaultDark) {
    palette.background = RGB(38, 38, 38);
    palette.drag_background = RGB(30, 30, 30);
    palette.border = RGB(62, 62, 62);
    palette.separator = RGB(48, 48, 48);
    palette.drag_separator = RGB(48, 48, 48);
    palette.text = RGB(255, 255, 255);
    palette.muted = RGB(152, 152, 152);
    palette.hover = RGB(62, 62, 62);
    palette.pressed = RGB(78, 78, 78);
  }
  return palette;
}

Gdiplus::REAL IconStrokeWidth(RECT rect,
                              int requested_width,
                              Gdiplus::REAL ratio = 0.045f) {
  const Gdiplus::REAL w =
      static_cast<Gdiplus::REAL>(std::max(1, static_cast<int>(rect.right - rect.left)));
  const Gdiplus::REAL h =
      static_cast<Gdiplus::REAL>(std::max(1, static_cast<int>(rect.bottom - rect.top)));
  const Gdiplus::REAL scaled = std::min(w, h) * ratio;
  const Gdiplus::REAL requested = static_cast<Gdiplus::REAL>(std::max(1, requested_width));
  return std::clamp(std::min(requested, scaled), 1.0f, 3.0f);
}

void FillRoundedRect(HDC dc, RECT rect, int radius, COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  HGDIOBJ old_brush = SelectObject(dc, brush);
  HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
  RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
  SelectObject(dc, old_pen);
  SelectObject(dc, old_brush);
  DeleteObject(brush);
}

void FillSolidRect(HDC dc, const RECT& rect, COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
}

void ApplyRoundedWindowRegion(HWND window, int width, int height, int radius, bool redraw = true) {
  if (window == nullptr || width <= 0 || height <= 0 || radius <= 0) {
    return;
  }
  HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius * 2, radius * 2);
  if (region == nullptr) {
    return;
  }
  if (SetWindowRgn(window, region, redraw ? TRUE : FALSE) == 0) {
    DeleteObject(region);
  }
}

void ApplyRoundedWindowRegionIfNeeded(HWND window, int width, int height, int radius) {
  if (window == nullptr || width <= 0 || height <= 0) {
    return;
  }
  const LONG_PTR last_width = GetWindowLongPtrW(window, kCandidateWindowClassExtraLastWidth);
  const LONG_PTR last_height = GetWindowLongPtrW(window, kCandidateWindowClassExtraLastHeight);
  if (last_width == width && last_height == height) {
    return;
  }
  SetWindowLongPtrW(window, kCandidateWindowClassExtraLastWidth, width);
  SetWindowLongPtrW(window, kCandidateWindowClassExtraLastHeight, height);
  ApplyRoundedWindowRegion(window, width, height, radius, false);
}

void FillRoundedRectAntialias(HDC dc, RECT rect, int radius, COLORREF color) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    FillRoundedRect(dc, rect, radius, color);
    return;
  }
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  const Gdiplus::REAL left = static_cast<Gdiplus::REAL>(rect.left);
  const Gdiplus::REAL top = static_cast<Gdiplus::REAL>(rect.top);
  const Gdiplus::REAL width = static_cast<Gdiplus::REAL>(rect.right - rect.left);
  const Gdiplus::REAL height = static_cast<Gdiplus::REAL>(rect.bottom - rect.top);
  const Gdiplus::REAL diameter =
      std::min<Gdiplus::REAL>(std::min(width, height),
                              static_cast<Gdiplus::REAL>(std::max(1, radius * 2)));
  Gdiplus::GraphicsPath path;
  path.AddArc(left, top, diameter, diameter, 180.0f, 90.0f);
  path.AddArc(left + width - diameter, top, diameter, diameter, 270.0f, 90.0f);
  path.AddArc(left + width - diameter, top + height - diameter, diameter, diameter, 0.0f, 90.0f);
  path.AddArc(left, top + height - diameter, diameter, diameter, 90.0f, 90.0f);
  path.CloseFigure();
  Gdiplus::SolidBrush brush(
      Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
  graphics.FillPath(&brush, &path);
}

void FillRoundedRectSidesAntialias(HDC dc,
                                   RECT rect,
                                   int radius,
                                   bool round_left,
                                   bool round_right,
                                   COLORREF color) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    FillSolidRect(dc, rect, color);
    return;
  }
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  const Gdiplus::REAL left = static_cast<Gdiplus::REAL>(rect.left);
  const Gdiplus::REAL top = static_cast<Gdiplus::REAL>(rect.top);
  const Gdiplus::REAL width = static_cast<Gdiplus::REAL>(rect.right - rect.left);
  const Gdiplus::REAL height = static_cast<Gdiplus::REAL>(rect.bottom - rect.top);
  const Gdiplus::REAL diameter =
      std::min<Gdiplus::REAL>(std::min(width, height),
                              static_cast<Gdiplus::REAL>(std::max(1, radius * 2)));
  const Gdiplus::REAL r = diameter / 2.0f;
  Gdiplus::GraphicsPath path;
  path.StartFigure();
  path.AddLine(left + (round_left ? r : 0.0f), top, left + width - (round_right ? r : 0.0f), top);
  if (round_right) {
    path.AddArc(left + width - diameter, top, diameter, diameter, 270.0f, 90.0f);
  }
  path.AddLine(left + width, top + (round_right ? r : 0.0f),
               left + width, top + height - (round_right ? r : 0.0f));
  if (round_right) {
    path.AddArc(left + width - diameter, top + height - diameter, diameter, diameter, 0.0f, 90.0f);
  }
  path.AddLine(left + width - (round_right ? r : 0.0f), top + height,
               left + (round_left ? r : 0.0f), top + height);
  if (round_left) {
    path.AddArc(left, top + height - diameter, diameter, diameter, 90.0f, 90.0f);
  }
  path.AddLine(left, top + height - (round_left ? r : 0.0f), left, top + (round_left ? r : 0.0f));
  if (round_left) {
    path.AddArc(left, top, diameter, diameter, 180.0f, 90.0f);
  }
  path.CloseFigure();
  Gdiplus::SolidBrush brush(
      Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
  graphics.FillPath(&brush, &path);
}

void DrawRoundedRectBorderAntialias(HDC dc, RECT rect, int radius, COLORREF color) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    HPEN border = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_pen = SelectObject(dc, border);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(border);
    return;
  }
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  const Gdiplus::REAL left = static_cast<Gdiplus::REAL>(rect.left) + 0.5f;
  const Gdiplus::REAL top = static_cast<Gdiplus::REAL>(rect.top) + 0.5f;
  const Gdiplus::REAL width = static_cast<Gdiplus::REAL>(rect.right - rect.left - 1);
  const Gdiplus::REAL height = static_cast<Gdiplus::REAL>(rect.bottom - rect.top - 1);
  const Gdiplus::REAL diameter =
      std::min<Gdiplus::REAL>(std::min(width, height),
                              static_cast<Gdiplus::REAL>(std::max(1, radius * 2)));
  Gdiplus::GraphicsPath path;
  path.AddArc(left, top, diameter, diameter, 180.0f, 90.0f);
  path.AddArc(left + width - diameter, top, diameter, diameter, 270.0f, 90.0f);
  path.AddArc(left + width - diameter, top + height - diameter, diameter, diameter, 0.0f, 90.0f);
  path.AddArc(left, top + height - diameter, diameter, diameter, 90.0f, 90.0f);
  path.CloseFigure();
  Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)),
                   1.0f);
  graphics.DrawPath(&pen, &path);
}

void DrawToolbarOuterEdge(HDC dc, RECT rect, int radius, int stroke_width, COLORREF color) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    DrawRoundedRectBorderAntialias(dc, rect, radius, color);
    return;
  }
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  const Gdiplus::REAL stroke =
      static_cast<Gdiplus::REAL>(std::max(1, stroke_width));
  const Gdiplus::REAL inset = stroke / 2.0f;
  const Gdiplus::REAL left = static_cast<Gdiplus::REAL>(rect.left) + inset;
  const Gdiplus::REAL top = static_cast<Gdiplus::REAL>(rect.top) + inset;
  const Gdiplus::REAL width =
      static_cast<Gdiplus::REAL>(rect.right - rect.left) - stroke;
  const Gdiplus::REAL height =
      static_cast<Gdiplus::REAL>(rect.bottom - rect.top) - stroke;
  const Gdiplus::REAL diameter =
      std::min<Gdiplus::REAL>(std::min(width, height),
                              static_cast<Gdiplus::REAL>(std::max(1, radius * 2)));
  Gdiplus::GraphicsPath path;
  path.AddArc(left, top, diameter, diameter, 180.0f, 90.0f);
  path.AddArc(left + width - diameter, top, diameter, diameter, 270.0f, 90.0f);
  path.AddArc(left + width - diameter, top + height - diameter, diameter, diameter, 0.0f, 90.0f);
  path.AddArc(left, top + height - diameter, diameter, diameter, 90.0f, 90.0f);
  path.CloseFigure();
  Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)),
                   stroke);
  graphics.DrawPath(&pen, &path);
}

RECT DrawRoundedSurface(HDC dc,
                        RECT rect,
                        int radius,
                        int border_width,
                        COLORREF edge_color,
                        COLORREF surface_color) {
  border_width = std::max(1, border_width);
  FillRoundedRectAntialias(dc, rect, radius, surface_color);
  RECT surface{rect.left + border_width,
               rect.top + border_width,
               rect.right - border_width,
               rect.bottom - border_width};
  if (g_gdiplus_ready && border_width == 1) {
    DrawRoundedRectBorderAntialias(dc, rect, radius, edge_color);
  } else {
    for (int inset = 0; inset < border_width; ++inset) {
      RECT border_rect{rect.left + inset,
                       rect.top + inset,
                       rect.right - inset,
                       rect.bottom - inset};
      DrawRoundedRectBorderAntialias(dc, border_rect, std::max(1, radius - inset), edge_color);
    }
  }
  return surface;
}

RECT DrawFloatingWindowSurface(HDC dc,
                               RECT rect,
                               int radius,
                               UINT dpi,
                               COLORREF edge_color,
                               COLORREF surface_color) {
  return DrawRoundedSurface(dc,
                            rect,
                            radius,
                            FloatingWindowBorderWidth(dpi),
                            edge_color,
                            surface_color);
}

BYTE RoundedRectCoverageAlpha(int x, int y, int width, int height, int radius) {
  if (width <= 0 || height <= 0 || radius <= 0) {
    return 255;
  }
  constexpr int kSamples = 8;
  const float r = static_cast<float>(std::min(radius, std::min(width, height) / 2));
  int covered = 0;
  for (int sample_y = 0; sample_y < kSamples; ++sample_y) {
    for (int sample_x = 0; sample_x < kSamples; ++sample_x) {
      const float px = static_cast<float>(x) +
                       (static_cast<float>(sample_x) + 0.5f) / static_cast<float>(kSamples);
      const float py = static_cast<float>(y) +
                       (static_cast<float>(sample_y) + 0.5f) / static_cast<float>(kSamples);
      bool inside = false;
      if (px >= r && px <= static_cast<float>(width) - r) {
        inside = py >= 0.0f && py <= static_cast<float>(height);
      } else if (py >= r && py <= static_cast<float>(height) - r) {
        inside = px >= 0.0f && px <= static_cast<float>(width);
      } else {
        const float cx = px < r ? r : static_cast<float>(width) - r;
        const float cy = py < r ? r : static_cast<float>(height) - r;
        const float dx = px - cx;
        const float dy = py - cy;
        inside = dx * dx + dy * dy <= r * r;
      }
      if (inside) {
        ++covered;
      }
    }
  }
  return static_cast<BYTE>((covered * 255 + (kSamples * kSamples / 2)) /
                           (kSamples * kSamples));
}

template <typename DrawFn>
bool RenderRoundedLayeredWindow(HWND window, int radius, COLORREF matte_color, DrawFn draw_fn) {
  if (window == nullptr) {
    return false;
  }
  RECT client{};
  GetClientRect(window, &client);
  const int width = client.right - client.left;
  const int height = client.bottom - client.top;
  if (width <= 0 || height <= 0) {
    return false;
  }

  HDC screen_dc = GetDC(nullptr);
  HDC memory_dc = screen_dc != nullptr ? CreateCompatibleDC(screen_dc) : nullptr;
  if (screen_dc == nullptr || memory_dc == nullptr) {
    if (memory_dc != nullptr) {
      DeleteDC(memory_dc);
    }
    if (screen_dc != nullptr) {
      ReleaseDC(nullptr, screen_dc);
    }
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE);
    return false;
  }

  BITMAPINFO bitmap_info{};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = width;
  bitmap_info.bmiHeader.biHeight = -height;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP bitmap = CreateDIBSection(screen_dc, &bitmap_info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bitmap == nullptr || bits == nullptr) {
    if (bitmap != nullptr) {
      DeleteObject(bitmap);
    }
    DeleteDC(memory_dc);
    ReleaseDC(nullptr, screen_dc);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE);
    return false;
  }

  HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap);
  RECT draw_rect{0, 0, width, height};
  FillSolidRect(memory_dc, draw_rect, matte_color);
  draw_fn(memory_dc);

  radius = std::clamp(radius, 0, std::min(width, height) / 2);
  auto* pixels = static_cast<BYTE*>(bits);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const BYTE alpha = RoundedRectCoverageAlpha(x, y, width, height, radius);
      BYTE* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) +
                              static_cast<size_t>(x)) *
                                 4;
      pixel[0] = static_cast<BYTE>((static_cast<int>(pixel[0]) * alpha + 127) / 255);
      pixel[1] = static_cast<BYTE>((static_cast<int>(pixel[1]) * alpha + 127) / 255);
      pixel[2] = static_cast<BYTE>((static_cast<int>(pixel[2]) * alpha + 127) / 255);
      pixel[3] = alpha;
    }
  }

  RECT window_rect{};
  GetWindowRect(window, &window_rect);
  POINT dst{window_rect.left, window_rect.top};
  POINT src{0, 0};
  SIZE size{width, height};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  const bool updated =
      UpdateLayeredWindow(window, screen_dc, &dst, &size, memory_dc, &src, 0, &blend, ULW_ALPHA) !=
      FALSE;

  if (old_bitmap != nullptr) {
    SelectObject(memory_dc, old_bitmap);
  }
  DeleteObject(bitmap);
  DeleteDC(memory_dc);
  ReleaseDC(nullptr, screen_dc);
  return updated;
}

template <typename DrawFn>
bool RenderToolbarLayeredWindowWithVisibleRect(HWND window,
                                               RECT visible_rect,
                                               int radius,
                                               COLORREF matte_color,
                                               DrawFn draw_fn) {
  if (window == nullptr) {
    return false;
  }
  RECT client{};
  GetClientRect(window, &client);
  const int width = client.right - client.left;
  const int height = client.bottom - client.top;
  if (width <= 0 || height <= 0) {
    return false;
  }

  visible_rect.left = std::clamp<LONG>(visible_rect.left, 0, width);
  visible_rect.top = std::clamp<LONG>(visible_rect.top, 0, height);
  visible_rect.right = std::clamp<LONG>(visible_rect.right, visible_rect.left, width);
  visible_rect.bottom = std::clamp<LONG>(visible_rect.bottom, visible_rect.top, height);
  const int visible_width = visible_rect.right - visible_rect.left;
  const int visible_height = visible_rect.bottom - visible_rect.top;
  if (visible_width <= 0 || visible_height <= 0) {
    return false;
  }

  HDC screen_dc = GetDC(nullptr);
  HDC memory_dc = screen_dc != nullptr ? CreateCompatibleDC(screen_dc) : nullptr;
  if (screen_dc == nullptr || memory_dc == nullptr) {
    if (memory_dc != nullptr) {
      DeleteDC(memory_dc);
    }
    if (screen_dc != nullptr) {
      ReleaseDC(nullptr, screen_dc);
    }
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE);
    return false;
  }

  BITMAPINFO bitmap_info{};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = width;
  bitmap_info.bmiHeader.biHeight = -height;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP bitmap = CreateDIBSection(screen_dc, &bitmap_info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bitmap == nullptr || bits == nullptr) {
    if (bitmap != nullptr) {
      DeleteObject(bitmap);
    }
    DeleteDC(memory_dc);
    ReleaseDC(nullptr, screen_dc);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE);
    return false;
  }

  HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap);
  RECT draw_rect{0, 0, width, height};
  FillSolidRect(memory_dc, draw_rect, matte_color);
  draw_fn(memory_dc);

  radius = std::clamp(radius, 0, std::min(visible_width, visible_height) / 2);
  auto* pixels = static_cast<BYTE*>(bits);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      BYTE alpha = 0;
      if (x >= visible_rect.left && x < visible_rect.right && y >= visible_rect.top &&
          y < visible_rect.bottom) {
        alpha = RoundedRectCoverageAlpha(x - visible_rect.left,
                                         y - visible_rect.top,
                                         visible_width,
                                         visible_height,
                                         radius);
      }
      BYTE* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) +
                              static_cast<size_t>(x)) *
                                 4;
      pixel[0] = static_cast<BYTE>((static_cast<int>(pixel[0]) * alpha + 127) / 255);
      pixel[1] = static_cast<BYTE>((static_cast<int>(pixel[1]) * alpha + 127) / 255);
      pixel[2] = static_cast<BYTE>((static_cast<int>(pixel[2]) * alpha + 127) / 255);
      pixel[3] = alpha;
    }
  }

  RECT window_rect{};
  GetWindowRect(window, &window_rect);
  POINT dst{window_rect.left, window_rect.top};
  POINT src{0, 0};
  SIZE size{width, height};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  const bool updated =
      UpdateLayeredWindow(window, screen_dc, &dst, &size, memory_dc, &src, 0, &blend, ULW_ALPHA) !=
      FALSE;

  if (old_bitmap != nullptr) {
    SelectObject(memory_dc, old_bitmap);
  }
  DeleteObject(bitmap);
  DeleteDC(memory_dc);
  ReleaseDC(nullptr, screen_dc);
  return updated;
}

void DrawMicrosoftWindowShadow(HDC dc, RECT rect, int radius, COLORREF color) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    return;
  }

  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  const int inset_count = 5;
  for (int inset = inset_count; inset >= 1; --inset) {
    const BYTE alpha = static_cast<BYTE>(8 + (inset_count - inset) * 5);
    const Gdiplus::REAL left = static_cast<Gdiplus::REAL>(rect.left + inset) + 0.5f;
    const Gdiplus::REAL top = static_cast<Gdiplus::REAL>(rect.top + inset + 1) + 0.5f;
    const Gdiplus::REAL width = static_cast<Gdiplus::REAL>(rect.right - rect.left - inset * 2 - 1);
    const Gdiplus::REAL height = static_cast<Gdiplus::REAL>(rect.bottom - rect.top - inset * 2 - 1);
    if (width <= 0.0f || height <= 0.0f) {
      continue;
    }
    const Gdiplus::REAL diameter = static_cast<Gdiplus::REAL>(std::max(1, radius * 2));
    Gdiplus::GraphicsPath path;
    path.AddArc(left, top, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(left + width - diameter, top, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(left + width - diameter, top + height - diameter, diameter, diameter, 0.0f, 90.0f);
    path.AddArc(left, top + height - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
    Gdiplus::Pen pen(Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color)),
                     1.0f);
    graphics.DrawPath(&pen, &path);
  }
}

std::vector<double> ParseSvgNumbers(std::wstring_view path, size_t* index) {
  std::vector<double> numbers;
  while (*index < path.size()) {
    while (*index < path.size() &&
           (std::iswspace(path[*index]) || path[*index] == L',')) {
      ++(*index);
    }
    if (*index >= path.size() || std::iswalpha(path[*index])) {
      break;
    }
    const wchar_t* start = path.data() + *index;
    wchar_t* end = nullptr;
    const double value = wcstod(start, &end);
    if (end == start) {
      break;
    }
    numbers.push_back(value);
    *index = static_cast<size_t>(end - path.data());
  }
  return numbers;
}

void DrawFluentPathIcon(HDC dc,
                        RECT rect,
                        std::wstring_view path_data,
                        float view_box_size,
                        COLORREF color,
                        float scale = 1.0f,
                        float dx = 0.0f,
                        float dy = 0.0f,
                        float scale_y = 0.0f) {
  EnsureGdiplus();
  if (!g_gdiplus_ready || path_data.empty() || view_box_size <= 0.0f) {
    return;
  }

  Gdiplus::GraphicsPath icon;
  wchar_t command = 0;
  Gdiplus::PointF current{};
  Gdiplus::PointF subpath_start{};
  size_t index = 0;
  while (index < path_data.size()) {
    while (index < path_data.size() &&
           (std::iswspace(path_data[index]) || path_data[index] == L',')) {
      ++index;
    }
    if (index >= path_data.size()) {
      break;
    }
    if (std::iswalpha(path_data[index])) {
      command = path_data[index++];
    }
    if (command == 0) {
      break;
    }
    std::vector<double> numbers = ParseSvgNumbers(path_data, &index);
    size_t offset = 0;
    switch (command) {
      case L'M':
      case L'm': {
        bool first = true;
        while (offset + 1 < numbers.size()) {
          Gdiplus::PointF point{
              static_cast<Gdiplus::REAL>(numbers[offset]),
              static_cast<Gdiplus::REAL>(numbers[offset + 1])};
          if (command == L'm') {
            point.X += current.X;
            point.Y += current.Y;
          }
          if (first) {
            icon.StartFigure();
            subpath_start = point;
            first = false;
          } else {
            icon.AddLine(current, point);
          }
          current = point;
          offset += 2;
        }
        break;
      }
      case L'L':
      case L'l':
        while (offset + 1 < numbers.size()) {
          Gdiplus::PointF point{
              static_cast<Gdiplus::REAL>(numbers[offset]),
              static_cast<Gdiplus::REAL>(numbers[offset + 1])};
          if (command == L'l') {
            point.X += current.X;
            point.Y += current.Y;
          }
          icon.AddLine(current, point);
          current = point;
          offset += 2;
        }
        break;
      case L'H':
      case L'h':
        while (offset < numbers.size()) {
          Gdiplus::PointF point{
              command == L'h' ? current.X + static_cast<Gdiplus::REAL>(numbers[offset])
                              : static_cast<Gdiplus::REAL>(numbers[offset]),
              current.Y};
          icon.AddLine(current, point);
          current = point;
          ++offset;
        }
        break;
      case L'V':
      case L'v':
        while (offset < numbers.size()) {
          Gdiplus::PointF point{
              current.X,
              command == L'v' ? current.Y + static_cast<Gdiplus::REAL>(numbers[offset])
                              : static_cast<Gdiplus::REAL>(numbers[offset])};
          icon.AddLine(current, point);
          current = point;
          ++offset;
        }
        break;
      case L'C':
      case L'c':
        while (offset + 5 < numbers.size()) {
          Gdiplus::PointF control1{
              static_cast<Gdiplus::REAL>(numbers[offset]),
              static_cast<Gdiplus::REAL>(numbers[offset + 1])};
          Gdiplus::PointF control2{
              static_cast<Gdiplus::REAL>(numbers[offset + 2]),
              static_cast<Gdiplus::REAL>(numbers[offset + 3])};
          Gdiplus::PointF point{
              static_cast<Gdiplus::REAL>(numbers[offset + 4]),
              static_cast<Gdiplus::REAL>(numbers[offset + 5])};
          if (command == L'c') {
            control1.X += current.X;
            control1.Y += current.Y;
            control2.X += current.X;
            control2.Y += current.Y;
            point.X += current.X;
            point.Y += current.Y;
          }
          icon.AddBezier(current, control1, control2, point);
          current = point;
          offset += 6;
        }
        break;
      case L'Z':
      case L'z':
        icon.CloseFigure();
        current = subpath_start;
        break;
      default:
        break;
    }
  }

  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  const Gdiplus::REAL rect_width = static_cast<Gdiplus::REAL>(rect.right - rect.left);
  const Gdiplus::REAL rect_height = static_cast<Gdiplus::REAL>(rect.bottom - rect.top);
  const Gdiplus::REAL base_side = std::min(rect_width, rect_height);
  const Gdiplus::REAL side_x = base_side * scale;
  const Gdiplus::REAL side_y = base_side * (scale_y > 0.0f ? scale_y : scale);
  Gdiplus::Matrix transform;
  transform.Translate(static_cast<Gdiplus::REAL>(rect.left) + (rect_width - side_x) / 2.0f +
                          rect_width * dx,
                      static_cast<Gdiplus::REAL>(rect.top) + (rect_height - side_y) / 2.0f +
                          rect_height * dy);
  transform.Scale(side_x / view_box_size, side_y / view_box_size);
  icon.Transform(&transform);
  Gdiplus::SolidBrush brush(
      Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
  graphics.FillPath(&brush, &icon);
}

constexpr std::wstring_view kFluentSettings24 =
    L"M12.0122 2.25C12.7462 2.25846 13.4773 2.34326 14.1937 2.50304C14.5064 2.57279 14.7403 2.83351 14.7758 3.15196L14.946 4.67881C15.0231 5.37986 15.615 5.91084 16.3206 5.91158C16.5103 5.91188 16.6979 5.87238 16.8732 5.79483L18.2738 5.17956C18.5651 5.05159 18.9055 5.12136 19.1229 5.35362C20.1351 6.43464 20.8889 7.73115 21.3277 9.14558C21.4223 9.45058 21.3134 9.78203 21.0564 9.9715L19.8149 10.8866C19.4607 11.1468 19.2516 11.56 19.2516 11.9995C19.2516 12.4389 19.4607 12.8521 19.8157 13.1129L21.0582 14.0283C21.3153 14.2177 21.4243 14.5492 21.3297 14.8543C20.8911 16.2685 20.1377 17.5649 19.1261 18.6461C18.9089 18.8783 18.5688 18.9483 18.2775 18.8206L16.8712 18.2045C16.4688 18.0284 16.0068 18.0542 15.6265 18.274C15.2463 18.4937 14.9933 18.8812 14.945 19.3177L14.7759 20.8444C14.741 21.1592 14.5122 21.4182 14.204 21.4915C12.7556 21.8361 11.2465 21.8361 9.79803 21.4915C9.48991 21.4182 9.26105 21.1592 9.22618 20.8444L9.05736 19.32C9.00777 18.8843 8.75434 18.498 8.37442 18.279C7.99451 18.06 7.5332 18.0343 7.1322 18.2094L5.72557 18.8256C5.43422 18.9533 5.09403 18.8833 4.87678 18.6509C3.86462 17.5685 3.11119 16.2705 2.6732 14.8548C2.57886 14.5499 2.68786 14.2186 2.94485 14.0293L4.18818 13.1133C4.54232 12.8531 4.75147 12.4399 4.75147 12.0005C4.75147 11.561 4.54232 11.1478 4.18771 10.8873L2.94516 9.97285C2.6878 9.78345 2.5787 9.45178 2.67337 9.14658C3.11212 7.73215 3.86594 6.43564 4.87813 5.35462C5.09559 5.12236 5.43594 5.05259 5.72724 5.18056L7.12762 5.79572C7.53056 5.97256 7.9938 5.94585 8.37577 5.72269C8.75609 5.50209 9.00929 5.11422 9.05817 4.67764L9.22824 3.15196C9.26376 2.83335 9.49786 2.57254 9.8108 2.50294C10.5281 2.34342 11.26 2.25865 12.0122 2.25ZM12.0124 3.7499C11.5583 3.75524 11.1056 3.79443 10.6578 3.86702L10.5489 4.84418C10.4471 5.75368 9.92003 6.56102 9.13042 7.01903C8.33597 7.48317 7.36736 7.53903 6.52458 7.16917L5.62629 6.77456C5.05436 7.46873 4.59914 8.25135 4.27852 9.09168L5.07632 9.67879C5.81513 10.2216 6.25147 11.0837 6.25147 12.0005C6.25147 12.9172 5.81513 13.7793 5.0771 14.3215L4.27805 14.9102C4.59839 15.752 5.05368 16.5361 5.626 17.2316L6.53113 16.8351C7.36923 16.4692 8.33124 16.5227 9.12353 16.9794C9.91581 17.4361 10.4443 18.2417 10.548 19.1526L10.657 20.1365C11.5466 20.2878 12.4555 20.2878 13.3451 20.1365L13.4541 19.1527C13.5549 18.2421 14.0828 17.4337 14.876 16.9753C15.6692 16.5168 16.6332 16.463 17.4728 16.8305L18.3772 17.2267C18.949 16.5323 19.4041 15.7495 19.7247 14.909L18.9267 14.3211C18.1879 13.7783 17.7516 12.9162 17.7516 11.9995C17.7516 11.0827 18.1879 10.2206 18.9258 9.67847L19.7227 9.09109C19.4021 8.25061 18.9468 7.46784 18.3748 6.77356L17.4783 7.16737C17.113 7.32901 16.7178 7.4122 16.3187 7.41158C14.849 7.41004 13.6155 6.30355 13.4551 4.84383L13.3462 3.8667C12.9007 3.7942 12.4526 3.75512 12.0124 3.7499ZM11.9997 8.24995C14.0708 8.24995 15.7497 9.92888 15.7497 12C15.7497 14.071 14.0708 15.75 11.9997 15.75C9.92863 15.75 8.2497 14.071 8.2497 12C8.2497 9.92888 9.92863 8.24995 11.9997 8.24995ZM11.9997 9.74995C10.7571 9.74995 9.7497 10.7573 9.7497 12C9.7497 13.2426 10.7571 14.25 11.9997 14.25C13.2423 14.25 14.2497 13.2426 14.2497 12C14.2497 10.7573 13.2423 9.74995 11.9997 9.74995Z";
constexpr std::wstring_view kFluentEmoji24 =
    L"M12 1.99805C17.5237 1.99805 22.0015 6.47589 22.0015 11.9996C22.0015 17.5233 17.5237 22.0011 12 22.0011C6.47626 22.0011 1.99841 17.5233 1.99841 11.9996C1.99841 6.47589 6.47626 1.99805 12 1.99805ZM12 3.49805C7.30469 3.49805 3.49841 7.30432 3.49841 11.9996C3.49841 16.6949 7.30469 20.5011 12 20.5011C16.6952 20.5011 20.5015 16.6949 20.5015 11.9996C20.5015 7.30432 16.6952 3.49805 12 3.49805ZM8.4617 14.7829C9.31084 15.8606 10.6019 16.5012 11.9999 16.5012C13.3962 16.5012 14.6856 15.8624 15.5349 14.7871C15.7916 14.462 16.2633 14.4066 16.5883 14.6634C16.9134 14.9201 16.9688 15.3917 16.712 15.7168C15.5813 17.1485 13.8601 18.0012 11.9999 18.0012C10.1373 18.0012 8.41408 17.1462 7.28348 15.7112C7.02713 15.3859 7.08307 14.9143 7.40843 14.658C7.73379 14.4016 8.20535 14.4576 8.4617 14.7829ZM9.00041 8.75024C9.69037 8.75024 10.2497 9.30956 10.2497 9.99953C10.2497 10.6895 9.69037 11.2488 9.00041 11.2488C8.31045 11.2488 7.75112 10.6895 7.75112 9.99953C7.75112 9.30956 8.31045 8.75024 9.00041 8.75024ZM15.0004 8.75024C15.6904 8.75024 16.2497 9.30956 16.2497 9.99953C16.2497 10.6895 15.6904 11.2488 15.0004 11.2488C14.3104 11.2488 13.7511 10.6895 13.7511 9.99953C13.7511 9.30956 14.3104 8.75024 15.0004 8.75024Z";
// Fluent UI System Icons: arrow_clockwise_24_regular.
constexpr std::wstring_view kFluentArrowClockwise24 =
    L"M12 4.5C7.85786 4.5 4.5 7.85786 4.5 12C4.5 16.1421 7.85786 19.5 12 19.5C16.1421 19.5 19.5 16.1421 19.5 12C19.5 11.6236 19.4723 11.2538 19.4188 10.8923C19.3515 10.4382 19.6839 10 20.1429 10C20.5138 10 20.839 10.2562 20.8953 10.6228C20.9642 11.0718 21 11.5317 21 12C21 16.9706 16.9706 21 12 21C7.02944 21 3 16.9706 3 12C3 7.02944 7.02944 3 12 3C14.3051 3 16.4077 3.86656 18 5.29168V4.25C18 3.83579 18.3358 3.5 18.75 3.5C19.1642 3.5 19.5 3.83579 19.5 4.25V7.25C19.5 7.66421 19.1642 8 18.75 8H15.75C15.3358 8 15 7.66421 15 7.25C15 6.83579 15.3358 6.5 15.75 6.5H17.0991C15.7609 5.25883 13.9691 4.5 12 4.5Z";
// Fluent UI System Icons: arrow_repeat_all_24_regular.
constexpr std::wstring_view kFluentArrowRepeatAll24 =
    L"M14.6102 2.47047L14.5334 2.4031C14.2394 2.17855 13.818 2.20101 13.5495 2.47047L13.4824 2.54755C13.2587 2.84259 13.281 3.26552 13.5495 3.53498L15.521 5.5118H8.5L8.26687 5.51592C4.785 5.63911 2 8.51085 2 12.0354C2 13.7259 2.6407 15.2663 3.6917 16.4252L3.76407 16.4947C3.89496 16.6065 4.06463 16.674 4.25 16.674C4.66421 16.674 5 16.337 5 15.9213C5 15.7481 4.9417 15.5885 4.84373 15.4613L4.64439 15.2306C3.92953 14.3627 3.5 13.2494 3.5 12.0354C3.5 9.26396 5.73858 7.01725 8.5 7.01725H15.381L13.5495 8.85754L13.4824 8.93463C13.2587 9.22967 13.281 9.6526 13.5495 9.92206C13.8424 10.216 14.3173 10.216 14.6102 9.92206L17.7922 6.72852L17.8593 6.65144C18.083 6.3564 18.0606 5.93347 17.7922 5.66401L14.6102 2.47047ZM20.23 7.57108C20.0999 7.46224 19.9326 7.39677 19.75 7.39677C19.3358 7.39677 19 7.73378 19 8.14949C19 8.33618 19.0677 8.507 19.1791 8.63722C19.9992 9.53109 20.5 10.7246 20.5 12.0354C20.5 14.8069 18.2614 17.0536 15.5 17.0536H8.558L10.4634 15.1425L10.5365 15.0573C10.7339 14.7897 10.7319 14.4206 10.5305 14.155L10.4634 14.0779L10.3785 14.0045C10.1119 13.8065 9.74409 13.8085 9.47951 14.0106L9.40271 14.0779L6.22073 17.2715L6.14756 17.3566C5.95023 17.6242 5.95224 17.9934 6.15361 18.2589L6.22073 18.336L9.40271 21.5295L9.48683 21.6024C9.78044 21.8211 10.1971 21.7968 10.4634 21.5295C10.7319 21.2601 10.7542 20.8371 10.5305 20.5421L10.4634 20.465L8.564 18.559H15.5L15.7331 18.5549C19.215 18.4317 22 15.56 22 12.0354C22 10.342 21.3571 8.79923 20.3029 7.63965L20.23 7.57108Z";
// Fluent UI System Icons: checkmark_20_regular.
constexpr std::wstring_view kFluentCheckmark20 =
    L"M3.37371 10.1678C3.19025 9.96143 2.87421 9.94284 2.66782 10.1263C2.46143 10.3098 2.44284 10.6258 2.6263 10.8322L6.6263 15.3322C6.81743 15.5472 7.15013 15.557 7.35356 15.3536L17.8536 4.85355C18.0488 4.65829 18.0488 4.34171 17.8536 4.14645C17.6583 3.95118 17.3417 3.95118 17.1465 4.14645L7.02141 14.2715L3.37371 10.1678Z";
constexpr std::wstring_view kFluentBoardHeart24 =
    L"M21.4963 5.56352C21.4007 3.85437 19.9844 2.49805 18.2514 2.49805H6.25065C4.45582 2.49805 3.00081 3.95313 3.0006 5.74796L3 17.7518L3.00514 17.9362C3.10076 19.6454 4.51701 21.0017 6.25006 21.0017H13.3659L11.915 19.4998H11.5V19.501L6.25 19.5017C5.33765 19.5017 4.58839 18.8036 4.50728 17.9124L4.5001 17.7517L4.5 9.49905L11.5 9.49805V12.1729C11.9493 11.7599 12.4601 11.4559 13 11.2611V3.99705L18.2514 3.99805C19.1638 3.99805 19.913 4.69609 19.9941 5.58729L20.0013 5.74802L20.0005 11.0276C20.517 11.086 21.0256 11.2361 21.5011 11.478L21.5014 5.74794L21.4963 5.56352ZM6.25073 3.99805L11.5 3.99705V7.99805L4.5 7.99905L4.5006 5.74808C4.50071 4.82994 5.20785 4.07693 6.10723 4.00386L6.25073 3.99805ZM21.9768 13.0589C23.3411 14.4711 23.3411 16.7605 21.9768 18.1727L17.5345 22.7707C17.3869 22.9235 17.1935 22.9999 17 22.9998C16.8065 22.9999 16.6131 22.9235 16.4655 22.7707L12.0232 18.1727C11.8179 17.9602 11.6435 17.7278 11.5 17.4816C10.8333 16.3376 10.8333 14.894 11.5 13.75C11.6435 13.5038 11.8179 13.2714 12.0232 13.0589C12.3155 12.7564 12.6466 12.5187 13 12.3458C14.2963 11.7116 15.8917 11.9493 16.9637 13.0589L17 13.0965L17.0363 13.0589C17.8443 12.2225 18.9498 11.8815 20.0003 12.0359C20.5295 12.1137 21.0448 12.3172 21.5011 12.6464C21.6683 12.767 21.8276 12.9045 21.9768 13.0589Z";
constexpr std::wstring_view kFluentTriangleLeft12Filled =
    L"M1.45866 5.21367C0.847113 5.56267 0.847113 6.43734 1.45866 6.78633L8.62781 10.8776C9.23809 11.2259 10 10.7893 10 10.0913V10.0635L10 10.0586V1.94235L10 1.93748V1.9087C10 1.2107 9.23809 0.774094 8.62781 1.12237L1.45866 5.21367Z";
constexpr std::wstring_view kFluentTriangleRight12Filled =
    L"M10.5414 6.78633C11.1529 6.43734 11.1529 5.56267 10.5414 5.21367L3.3722 1.12237C2.76192 0.774094 2.00001 1.2107 2.00001 1.9087L2.00001 1.9365L2 1.94138L2 10.0576L2.00001 10.0625L2.00001 10.0913C2.00001 10.7893 2.76192 11.2259 3.3722 10.8776L10.5414 6.78633Z";
constexpr std::wstring_view kFluentHeart16Filled =
    L"M7.54112 3.94779C6.26943 2.6761 4.21207 2.66992 2.94588 3.93611C1.67969 5.20231 1.68587 7.25966 2.95756 8.53136L7.66505 13.2389C7.86031 13.4341 8.1769 13.4341 8.37216 13.2389L13.0552 8.55858C14.3184 7.28827 14.3144 5.23668 13.0425 3.96476C11.7686 2.69079 9.71024 2.68461 8.44178 3.95306L7.99452 4.40119L7.54112 3.94779Z";
constexpr std::wstring_view kFluentCircle20Filled =
    L"M10 2C5.58172 2 2 5.58172 2 10C2 14.4183 5.58172 18 10 18C14.4183 18 18 14.4183 18 10C18 5.58172 14.4183 2 10 2Z";
constexpr std::wstring_view kFluentWeatherMoon24 =
    L"M20.0258 17.0014C17.2639 21.7851 11.1471 23.4241 6.3634 20.6622C5.06068 19.9101 3.964 18.8926 3.12872 17.6797C2.84945 17.2741 3.0301 16.7141 3.49369 16.5482C7.26112 15.1997 9.27892 13.6372 10.4498 11.4021C11.6825 9.04908 12.001 6.47162 11.1387 2.93862C11.0195 2.45008 11.4053 1.98492 11.9075 2.01186C13.4645 2.09539 14.9856 2.54263 16.3649 3.33903C21.1486 6.10088 22.7876 12.2177 20.0258 17.0014ZM11.7785 12.0981C10.5272 14.4867 8.46706 16.1972 4.96104 17.597C5.5693 18.2929 6.29275 18.8894 7.1134 19.3632C11.1796 21.7108 16.3791 20.3176 18.7267 16.2514C21.0744 12.1852 19.6812 6.98571 15.6149 4.63807C14.7379 4.1317 13.7951 3.79168 12.8228 3.62253C13.4699 7.00652 13.0525 9.66622 11.7785 12.0981Z";
constexpr std::wstring_view kFluentChevronLeft20 =
    L"M12.3534 15.8537C12.1585 16.0493 11.8419 16.0499 11.6463 15.855L6.16178 10.39C5.94607 10.1751 5.94607 9.82574 6.16178 9.6108L11.6463 4.14582C11.8419 3.9509 12.1585 3.95147 12.3534 4.14708C12.5483 4.34269 12.5477 4.65927 12.3521 4.85418L7.18753 10.0004L12.3521 15.1466C12.5477 15.3415 12.5483 15.6581 12.3534 15.8537Z";
constexpr std::wstring_view kFluentChevronRight20 =
    L"M7.64582 4.14708C7.84073 3.95147 8.15731 3.9509 8.35292 4.14582L13.8374 9.6108C14.0531 9.82574 14.0531 10.1751 13.8374 10.39L8.35292 15.855C8.15731 16.0499 7.84073 16.0493 7.64582 15.8537C7.4509 15.6581 7.45147 15.3415 7.64708 15.1466L12.8117 10.0004L7.64708 4.85418C7.45147 4.65927 7.4509 4.34269 7.64582 4.14708Z";
constexpr std::wstring_view kFluentChevronDown20 =
    L"M15.8537 7.64582C16.0493 7.84073 16.0499 8.15731 15.855 8.35292L10.39 13.8374C10.1751 14.0531 9.82574 14.0531 9.6108 13.8374L4.14582 8.35292C3.9509 8.15731 3.95147 7.84073 4.14708 7.64582C4.34269 7.4509 4.65927 7.45147 4.85418 7.64708L10.0004 12.8117L15.1466 7.64708C15.3415 7.45147 15.6581 7.4509 15.8537 7.64582Z";
constexpr std::wstring_view kFluentChevronUp20 =
    L"M4.14708 12.3534C3.95147 12.1585 3.9509 11.8419 4.14582 11.6463L9.6108 6.16178C9.82574 5.94607 10.1751 5.94607 10.39 6.16178L15.855 11.6463C16.0499 11.8419 16.0493 12.1585 15.8537 12.3534C15.6581 12.5483 15.3415 12.5477 15.1466 12.3521L10.0004 7.18753L4.85418 12.3521C4.65927 12.5477 4.34269 12.5483 4.14708 12.3534Z";
constexpr float kMicrosoftShapeModeIconScale = 0.78f;
constexpr float kToolbarShapeModeIconScale = 1.30f;

void DrawToolbarMenuCheckmark(HDC dc, RECT rect, COLORREF color, UINT dpi) {
  EnsureGdiplus();
  if (g_gdiplus_ready) {
    DrawFluentPathIcon(dc, rect, kFluentCheckmark20, 20.0f, color, 0.92f);
    return;
  }

  HFONT font = CreateUiFontForDpi(11, dpi, FW_NORMAL, UiFontFamily(false));
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  const int old_bk = SetBkMode(dc, TRANSPARENT);
  const COLORREF old_text = SetTextColor(dc, color);
  DrawTextW(dc,
            L"\u2713",
            -1,
            &rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  SetTextColor(dc, old_text);
  SetBkMode(dc, old_bk);
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
}

void DrawChevron(HDC dc, RECT rect, bool right, COLORREF color, int stroke_width) {
  (void)stroke_width;
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc,
              right ? L"\u25B6" : L"\u25C0",
              -1,
              &rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }

  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  Gdiplus::SolidBrush brush(
      Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));

  const Gdiplus::REAL width = static_cast<Gdiplus::REAL>(rect.right - rect.left);
  const Gdiplus::REAL height = static_cast<Gdiplus::REAL>(rect.bottom - rect.top);
  const Gdiplus::REAL cx =
      static_cast<Gdiplus::REAL>(rect.left) + width * (right ? 0.235f : 0.335f);
  const Gdiplus::REAL cy = static_cast<Gdiplus::REAL>(rect.top) + height * 0.50f;
  const Gdiplus::REAL half_width = width * 0.155f;
  const Gdiplus::REAL half_height = height * 0.210f;
  Gdiplus::GraphicsPath path;
  if (right) {
    path.AddLine(cx - half_width, cy - half_height, cx + half_width, cy);
    path.AddLine(cx + half_width, cy, cx - half_width, cy + half_height);
  } else {
    path.AddLine(cx + half_width, cy - half_height, cx - half_width, cy);
    path.AddLine(cx - half_width, cy, cx + half_width, cy + half_height);
  }
  path.CloseFigure();
  graphics.FillPath(&brush, &path);
}

void DrawCandidatePageTriangle(HDC dc,
                               RECT rect,
                               bool right,
                               COLORREF color,
                               bool centered,
                               bool compact_horizontal,
                               bool vertical_direction = false) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc,
              vertical_direction ? (right ? L"\u25BC" : L"\u25B2")
                                 : (right ? L"\u25B6" : L"\u25C0"),
              -1,
              &rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }

  if (vertical_direction) {
    DrawFluentPathIcon(dc,
                       rect,
                       right ? kFluentChevronDown20 : kFluentChevronUp20,
                       20.0f,
                       color,
                       0.98f,
                       0.0f,
                       0.0f,
                       0.98f);
    return;
  }

  const float centered_dx = compact_horizontal && !right ? -0.095f : 0.0f;
  const float centered_dy = compact_horizontal ? 0.035f : 0.0f;
  DrawFluentPathIcon(dc,
                     rect,
                     right ? kFluentTriangleRight12Filled : kFluentTriangleLeft12Filled,
                     12.0f,
                     color,
                     0.46f,
                     centered ? centered_dx : (right ? -0.295f : -0.170f),
                     centered ? centered_dy : 0.035f,
                     0.55f);
}

void DrawEmojiIcon(HDC dc, RECT rect, COLORREF color, int stroke_width) {
  (void)stroke_width;
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, L"\u2661", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }
  DrawFluentPathIcon(dc, rect, kFluentBoardHeart24, 24.0f, color, 0.90f, 0.01f, 0.00f);
}

void DrawCandidateBoardHeartIcon(HDC dc, RECT rect, COLORREF color, int stroke_width) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    DrawEmojiIcon(dc, rect, color, stroke_width);
    return;
  }

  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)),
                   IconStrokeWidth(rect, stroke_width, 0.062f));
  pen.SetStartCap(Gdiplus::LineCapRound);
  pen.SetEndCap(Gdiplus::LineCapRound);
  pen.SetLineJoin(Gdiplus::LineJoinRound);

  const Gdiplus::REAL width = static_cast<Gdiplus::REAL>(rect.right - rect.left);
  const Gdiplus::REAL height = static_cast<Gdiplus::REAL>(rect.bottom - rect.top);
  const Gdiplus::REAL x_offset = width * 0.065f;
  const Gdiplus::REAL board_left =
      static_cast<Gdiplus::REAL>(rect.left) + x_offset + width * 0.010f;
  const Gdiplus::REAL board_top = static_cast<Gdiplus::REAL>(rect.top) + height * 0.180f;
  const Gdiplus::REAL board_right =
      static_cast<Gdiplus::REAL>(rect.left) + x_offset + width * 0.735f;
  const Gdiplus::REAL board_bottom = static_cast<Gdiplus::REAL>(rect.top) + height * 0.890f;
  const Gdiplus::REAL radius = width * 0.155f;
  const Gdiplus::REAL top_end =
      static_cast<Gdiplus::REAL>(rect.left) + x_offset + width * 0.130f;
  const Gdiplus::REAL right_start_y =
      static_cast<Gdiplus::REAL>(rect.top) + height * 0.500f;
  Gdiplus::GraphicsPath box;
  box.StartFigure();
  box.AddLine(top_end, board_top, board_left + radius, board_top);
  box.AddBezier(board_left + radius,
                board_top,
                board_left + radius * 0.45f,
                board_top,
                board_left,
                board_top + radius * 0.45f,
                board_left,
                board_top + radius);
  box.AddLine(board_left, board_top + radius, board_left, board_bottom - radius);
  box.AddBezier(board_left,
                board_bottom - radius,
                board_left,
                board_bottom - radius * 0.45f,
                board_left + radius * 0.45f,
                board_bottom,
                board_left + radius,
                board_bottom);
  box.AddLine(board_left + radius, board_bottom, board_right - radius, board_bottom);
  box.AddBezier(board_right - radius,
                board_bottom,
                board_right - radius * 0.45f,
                board_bottom,
                board_right,
                board_bottom - radius * 0.45f,
                board_right,
                board_bottom - radius);
  box.AddLine(board_right, board_bottom - radius, board_right, right_start_y);
  graphics.DrawPath(&pen, &box);

  Gdiplus::SolidBrush brush(
      Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
  const Gdiplus::REAL heart_left =
      static_cast<Gdiplus::REAL>(rect.left) + x_offset + width * 0.190f;
  const Gdiplus::REAL heart_top =
      static_cast<Gdiplus::REAL>(rect.top) + height * 0.060f;
  const Gdiplus::REAL heart_w = width * 0.650f;
  const Gdiplus::REAL heart_h = height * 0.540f;
  Gdiplus::GraphicsPath heart;
  heart.StartFigure();
  heart.AddBezier(heart_left + heart_w * 0.500f,
                  heart_top + heart_h * 0.940f,
                  heart_left + heart_w * 0.080f,
                  heart_top + heart_h * 0.610f,
                  heart_left + heart_w * 0.000f,
                  heart_top + heart_h * 0.300f,
                  heart_left + heart_w * 0.205f,
                  heart_top + heart_h * 0.110f);
  heart.AddBezier(heart_left + heart_w * 0.205f,
                  heart_top + heart_h * 0.110f,
                  heart_left + heart_w * 0.340f,
                  heart_top - heart_h * 0.020f,
                  heart_left + heart_w * 0.485f,
                  heart_top + heart_h * 0.035f,
                  heart_left + heart_w * 0.500f,
                  heart_top + heart_h * 0.225f);
  heart.AddBezier(heart_left + heart_w * 0.500f,
                  heart_top + heart_h * 0.225f,
                  heart_left + heart_w * 0.545f,
                  heart_top + heart_h * 0.035f,
                  heart_left + heart_w * 0.705f,
                  heart_top - heart_h * 0.020f,
                  heart_left + heart_w * 0.835f,
                  heart_top + heart_h * 0.110f);
  heart.AddBezier(heart_left + heart_w * 0.835f,
                  heart_top + heart_h * 0.110f,
                  heart_left + heart_w * 1.055f,
                  heart_top + heart_h * 0.310f,
                  heart_left + heart_w * 0.940f,
                  heart_top + heart_h * 0.620f,
                  heart_left + heart_w * 0.500f,
                  heart_top + heart_h * 0.940f);
  heart.CloseFigure();
  graphics.FillPath(&brush, &heart);
}

void DrawToolbarBoardHeartIcon(HDC dc, RECT rect, COLORREF color, int stroke_width) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    DrawCandidateBoardHeartIcon(dc, rect, color, stroke_width);
    return;
  }

  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)),
                   IconStrokeWidth(rect, stroke_width, 0.055f));
  pen.SetStartCap(Gdiplus::LineCapRound);
  pen.SetEndCap(Gdiplus::LineCapRound);
  pen.SetLineJoin(Gdiplus::LineJoinRound);

  const Gdiplus::REAL width = static_cast<Gdiplus::REAL>(rect.right - rect.left);
  const Gdiplus::REAL height = static_cast<Gdiplus::REAL>(rect.bottom - rect.top);
  const Gdiplus::REAL side = std::min(width, height) * 0.705f;
  const Gdiplus::REAL left =
      static_cast<Gdiplus::REAL>(rect.left) + (width - side) * 0.36f;
  const Gdiplus::REAL top =
      static_cast<Gdiplus::REAL>(rect.top) + (height - side) * 0.54f;
  const Gdiplus::REAL radius = side * 0.150f;

  Gdiplus::GraphicsPath box;
  box.AddArc(left, top, radius * 2.0f, radius * 2.0f, 180.0f, 90.0f);
  box.AddArc(left + side - radius * 2.0f, top, radius * 2.0f, radius * 2.0f, 270.0f, 90.0f);
  box.AddArc(left + side - radius * 2.0f,
             top + side - radius * 2.0f,
             radius * 2.0f,
             radius * 2.0f,
             0.0f,
             90.0f);
  box.AddArc(left,
             top + side - radius * 2.0f,
             radius * 2.0f,
             radius * 2.0f,
             90.0f,
             90.0f);
  box.CloseFigure();
  graphics.DrawPath(&pen, &box);

  Gdiplus::SolidBrush brush(
      Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
  const Gdiplus::REAL heart_w = side * 0.310f;
  const Gdiplus::REAL heart_h = side * 0.260f;
  const Gdiplus::REAL heart_left = left + side * 0.500f - heart_w * 0.500f;
  const Gdiplus::REAL heart_top = top + side * 0.210f;
  Gdiplus::GraphicsPath heart;
  heart.StartFigure();
  heart.AddBezier(heart_left + heart_w * 0.500f,
                  heart_top + heart_h * 0.940f,
                  heart_left + heart_w * 0.090f,
                  heart_top + heart_h * 0.630f,
                  heart_left + heart_w * 0.000f,
                  heart_top + heart_h * 0.315f,
                  heart_left + heart_w * 0.205f,
                  heart_top + heart_h * 0.120f);
  heart.AddBezier(heart_left + heart_w * 0.205f,
                  heart_top + heart_h * 0.120f,
                  heart_left + heart_w * 0.345f,
                  heart_top - heart_h * 0.030f,
                  heart_left + heart_w * 0.490f,
                  heart_top + heart_h * 0.050f,
                  heart_left + heart_w * 0.500f,
                  heart_top + heart_h * 0.245f);
  heart.AddBezier(heart_left + heart_w * 0.500f,
                  heart_top + heart_h * 0.245f,
                  heart_left + heart_w * 0.545f,
                  heart_top + heart_h * 0.045f,
                  heart_left + heart_w * 0.705f,
                  heart_top - heart_h * 0.030f,
                  heart_left + heart_w * 0.835f,
                  heart_top + heart_h * 0.115f);
  heart.AddBezier(heart_left + heart_w * 0.835f,
                  heart_top + heart_h * 0.115f,
                  heart_left + heart_w * 1.060f,
                  heart_top + heart_h * 0.315f,
                  heart_left + heart_w * 0.935f,
                  heart_top + heart_h * 0.640f,
                  heart_left + heart_w * 0.500f,
                  heart_top + heart_h * 0.940f);
  heart.CloseFigure();
  graphics.FillPath(&brush, &heart);
}

void DrawSmileIcon(HDC dc, RECT rect, COLORREF color, int stroke_width) {
  (void)stroke_width;
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, L"\u263A", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }
  DrawFluentPathIcon(dc, rect, kFluentEmoji24, 24.0f, color, 0.94f);
}

void DrawMoonIcon(HDC dc, RECT rect, COLORREF color, int stroke_width) {
  (void)stroke_width;
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, L"\u25D4", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }
  DrawFluentPathIcon(dc, rect, kFluentWeatherMoon24, 24.0f, color, kMicrosoftShapeModeIconScale);
}

void DrawPunctuationIcon(HDC dc, RECT rect, COLORREF color, int stroke_width) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, L"\uFF0C\u3002", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  Gdiplus::SolidBrush brush(
      Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
  Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)),
                   IconStrokeWidth(rect, stroke_width, 0.050f));
  pen.SetStartCap(Gdiplus::LineCapRound);
  pen.SetEndCap(Gdiplus::LineCapRound);
  const Gdiplus::REAL w = static_cast<Gdiplus::REAL>(rect.right - rect.left);
  const Gdiplus::REAL h = static_cast<Gdiplus::REAL>(rect.bottom - rect.top);
  const Gdiplus::REAL dot = std::max<Gdiplus::REAL>(2.0f, std::min(w, h) * 0.12f);
  const Gdiplus::REAL cx1 = static_cast<Gdiplus::REAL>(rect.left) + w * 0.33f;
  const Gdiplus::REAL cy = static_cast<Gdiplus::REAL>(rect.top) + h * 0.42f;
  graphics.FillEllipse(&brush, cx1 - dot, cy - dot, dot * 2.0f, dot * 2.0f);
  const Gdiplus::REAL cx2 = static_cast<Gdiplus::REAL>(rect.left) + w * 0.62f;
  graphics.DrawLine(&pen, cx2, cy + dot * 0.8f, cx2 - dot * 0.7f, cy + dot * 3.8f);
}

void DrawMicrosoftPunctuationModeIcon(HDC dc,
                                      RECT rect,
                                      bool chinese_punctuation,
                                      COLORREF color,
                                      int stroke_width,
                                      bool snap_to_device_pixels = true) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc,
              chinese_punctuation ? L"\u3002" : L".",
              -1,
              &rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  const Gdiplus::REAL w = static_cast<Gdiplus::REAL>(rect.right - rect.left);
  const Gdiplus::REAL h = static_cast<Gdiplus::REAL>(rect.bottom - rect.top);
  const Gdiplus::REAL size = std::min(w, h);
  const Gdiplus::REAL left = static_cast<Gdiplus::REAL>(rect.left) + (w - size) / 2.0f;
  const Gdiplus::REAL top = static_cast<Gdiplus::REAL>(rect.top) + (h - size) / 2.0f;
  struct ReferenceRun {
    float y;
    float x1;
    float x2;
    int shade;
  };
  auto fill_reference_runs = [&](const ReferenceRun* runs, size_t count) {
    constexpr Gdiplus::REAL kReferenceLeft = 8.0f;
    constexpr Gdiplus::REAL kReferenceTop = 5.0f;
    constexpr Gdiplus::REAL kReferenceSize = 52.0f;
    constexpr float kReferenceBackground = 36.0f;
    constexpr float kReferenceForeground = 255.0f;
    if (!snap_to_device_pixels) {
      Gdiplus::GraphicsPath silhouette(Gdiplus::FillModeWinding);
      for (size_t index = 0; index < count; ++index) {
        const Gdiplus::REAL x1 =
            left + size * ((runs[index].x1 - kReferenceLeft) / kReferenceSize);
        const Gdiplus::REAL y1 =
            top + size * ((runs[index].y - kReferenceTop) / kReferenceSize);
        const Gdiplus::REAL x2 =
            left + size * ((runs[index].x2 + 1.0f - kReferenceLeft) / kReferenceSize);
        const Gdiplus::REAL y2 =
            top + size * ((runs[index].y + 1.0f - kReferenceTop) / kReferenceSize);
        silhouette.AddRectangle(Gdiplus::RectF(
            x1, y1, std::max<Gdiplus::REAL>(0.1f, x2 - x1),
            std::max<Gdiplus::REAL>(0.1f, y2 - y1)));
      }
      Gdiplus::SolidBrush brush(
          Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
      graphics.FillPath(&brush, &silhouette);
      return;
    }
    for (size_t index = 0; index < count; ++index) {
      Gdiplus::REAL x1 =
          left + size * ((runs[index].x1 - kReferenceLeft) / kReferenceSize);
      Gdiplus::REAL y1 =
          top + size * ((runs[index].y - kReferenceTop) / kReferenceSize);
      Gdiplus::REAL x2 =
          left + size * ((runs[index].x2 + 1.0f - kReferenceLeft) / kReferenceSize);
      Gdiplus::REAL y2 =
          top + size * ((runs[index].y + 1.0f - kReferenceTop) / kReferenceSize);
      if (snap_to_device_pixels) {
        x1 = std::round(x1);
        y1 = std::round(y1);
        x2 = std::round(x2);
        y2 = std::round(y2);
      }
      const float coverage = std::clamp(
          (static_cast<float>(runs[index].shade) - kReferenceBackground) /
              (kReferenceForeground - kReferenceBackground),
          0.0f,
          1.0f);
      const BYTE alpha = static_cast<BYTE>(std::lround(coverage * 255.0f));
      Gdiplus::SolidBrush brush(
          Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color)));
      graphics.FillRectangle(
          &brush, x1, y1, std::max<Gdiplus::REAL>(1.0f, x2 - x1),
          std::max<Gdiplus::REAL>(1.0f, y2 - y1));
    }
  };
  if (chinese_punctuation) {
    const ReferenceRun runs[] = {
        {10.0f, 25.0f, 28.0f, 130}, {11.0f, 23.0f, 23.0f, 130},
        {11.0f, 24.0f, 29.0f, 255}, {11.0f, 30.0f, 30.0f, 188},
        {12.0f, 22.0f, 22.0f, 223}, {12.0f, 23.0f, 30.0f, 255},
        {12.0f, 31.0f, 31.0f, 223}, {13.0f, 21.0f, 21.0f, 188},
        {13.0f, 22.0f, 25.0f, 255}, {13.0f, 26.0f, 27.0f, 188},
        {13.0f, 28.0f, 28.0f, 223}, {13.0f, 29.0f, 31.0f, 255},
        {13.0f, 32.0f, 32.0f, 223}, {14.0f, 20.0f, 20.0f, 130},
        {14.0f, 21.0f, 23.0f, 255}, {14.0f, 24.0f, 24.0f, 130},
        {14.0f, 29.0f, 29.0f, 130}, {14.0f, 30.0f, 32.0f, 255},
        {14.0f, 33.0f, 33.0f, 188}, {15.0f, 20.0f, 20.0f, 188},
        {15.0f, 21.0f, 22.0f, 255}, {15.0f, 23.0f, 23.0f, 188},
        {15.0f, 30.0f, 30.0f, 130}, {15.0f, 31.0f, 32.0f, 255},
        {15.0f, 33.0f, 33.0f, 223}, {16.0f, 20.0f, 20.0f, 223},
        {16.0f, 21.0f, 22.0f, 255}, {16.0f, 23.0f, 23.0f, 130},
        {16.0f, 31.0f, 31.0f, 223}, {16.0f, 32.0f, 33.0f, 255},
        {17.0f, 20.0f, 20.0f, 223}, {17.0f, 21.0f, 22.0f, 255},
        {17.0f, 31.0f, 31.0f, 223}, {17.0f, 32.0f, 33.0f, 255},
        {17.0f, 34.0f, 34.0f, 130}, {18.0f, 20.0f, 20.0f, 223},
        {18.0f, 21.0f, 22.0f, 255}, {18.0f, 31.0f, 31.0f, 223},
        {18.0f, 32.0f, 33.0f, 255}, {18.0f, 34.0f, 34.0f, 130},
        {19.0f, 20.0f, 20.0f, 223}, {19.0f, 21.0f, 22.0f, 255},
        {19.0f, 23.0f, 23.0f, 188}, {19.0f, 31.0f, 33.0f, 255},
        {20.0f, 20.0f, 20.0f, 130}, {20.0f, 21.0f, 23.0f, 255},
        {20.0f, 24.0f, 24.0f, 130}, {20.0f, 30.0f, 30.0f, 223},
        {20.0f, 31.0f, 32.0f, 255}, {20.0f, 33.0f, 33.0f, 188},
        {21.0f, 21.0f, 21.0f, 223}, {21.0f, 22.0f, 24.0f, 255},
        {21.0f, 25.0f, 25.0f, 188}, {21.0f, 26.0f, 28.0f, 130},
        {21.0f, 29.0f, 29.0f, 223}, {21.0f, 30.0f, 32.0f, 255},
        {22.0f, 22.0f, 22.0f, 223}, {22.0f, 23.0f, 31.0f, 255},
        {22.0f, 32.0f, 32.0f, 130}, {23.0f, 23.0f, 23.0f, 223},
        {23.0f, 24.0f, 29.0f, 255}, {23.0f, 30.0f, 30.0f, 223},
        {23.0f, 31.0f, 31.0f, 130}, {24.0f, 25.0f, 25.0f, 188},
        {24.0f, 26.0f, 27.0f, 223}, {24.0f, 28.0f, 28.0f, 188},
        {24.0f, 29.0f, 29.0f, 130}, {24.0f, 46.0f, 48.0f, 130},
        {25.0f, 44.0f, 44.0f, 188}, {25.0f, 45.0f, 49.0f, 255},
        {25.0f, 50.0f, 50.0f, 223}, {26.0f, 42.0f, 42.0f, 130},
        {26.0f, 43.0f, 51.0f, 255}, {26.0f, 52.0f, 52.0f, 130},
        {27.0f, 42.0f, 52.0f, 255}, {28.0f, 41.0f, 41.0f, 188},
        {28.0f, 42.0f, 52.0f, 255}, {28.0f, 53.0f, 53.0f, 188},
        {29.0f, 41.0f, 41.0f, 223}, {29.0f, 42.0f, 52.0f, 255},
        {29.0f, 53.0f, 53.0f, 223}, {30.0f, 41.0f, 41.0f, 223},
        {30.0f, 42.0f, 52.0f, 255}, {30.0f, 53.0f, 53.0f, 223},
        {31.0f, 41.0f, 41.0f, 223}, {31.0f, 42.0f, 52.0f, 255},
        {31.0f, 53.0f, 53.0f, 223}, {32.0f, 41.0f, 41.0f, 130},
        {32.0f, 42.0f, 52.0f, 255}, {32.0f, 53.0f, 53.0f, 223},
        {33.0f, 42.0f, 42.0f, 223}, {33.0f, 43.0f, 52.0f, 255},
        {33.0f, 53.0f, 53.0f, 188}, {34.0f, 43.0f, 43.0f, 223},
        {34.0f, 44.0f, 52.0f, 255}, {34.0f, 53.0f, 53.0f, 130},
        {35.0f, 44.0f, 44.0f, 130}, {35.0f, 45.0f, 46.0f, 223},
        {35.0f, 47.0f, 52.0f, 255}, {35.0f, 53.0f, 53.0f, 130},
        {36.0f, 48.0f, 52.0f, 255}, {37.0f, 47.0f, 47.0f, 130},
        {37.0f, 48.0f, 51.0f, 255}, {37.0f, 52.0f, 52.0f, 188},
        {38.0f, 47.0f, 47.0f, 188}, {38.0f, 48.0f, 51.0f, 255},
        {38.0f, 52.0f, 52.0f, 130}, {39.0f, 47.0f, 50.0f, 255},
        {39.0f, 51.0f, 51.0f, 188}, {40.0f, 46.0f, 46.0f, 130},
        {40.0f, 47.0f, 50.0f, 255}, {41.0f, 46.0f, 49.0f, 255},
        {41.0f, 50.0f, 50.0f, 130}, {42.0f, 45.0f, 45.0f, 223},
        {42.0f, 46.0f, 48.0f, 255}, {42.0f, 49.0f, 49.0f, 130},
        {43.0f, 44.0f, 44.0f, 130}, {43.0f, 45.0f, 47.0f, 255},
        {43.0f, 48.0f, 48.0f, 188}, {44.0f, 43.0f, 43.0f, 130},
        {44.0f, 44.0f, 46.0f, 255}, {44.0f, 47.0f, 47.0f, 223},
        {45.0f, 42.0f, 42.0f, 130}, {45.0f, 43.0f, 45.0f, 255},
        {45.0f, 46.0f, 46.0f, 223}, {46.0f, 41.0f, 41.0f, 130},
        {46.0f, 42.0f, 44.0f, 255}, {46.0f, 45.0f, 45.0f, 188},
        {47.0f, 40.0f, 40.0f, 130}, {47.0f, 41.0f, 43.0f, 255},
        {47.0f, 44.0f, 44.0f, 130}, {48.0f, 40.0f, 41.0f, 188},
        {48.0f, 42.0f, 42.0f, 130},
    };
    fill_reference_runs(runs, std::size(runs));
  } else {
    const ReferenceRun runs[] = {
        {11.0f, 24.0f, 24.0f, 130}, {11.0f, 25.0f, 25.0f, 188},
        {11.0f, 26.0f, 27.0f, 223}, {11.0f, 28.0f, 28.0f, 188},
        {12.0f, 23.0f, 23.0f, 223}, {12.0f, 24.0f, 29.0f, 255},
        {12.0f, 30.0f, 30.0f, 223}, {13.0f, 22.0f, 22.0f, 223},
        {13.0f, 23.0f, 30.0f, 255}, {13.0f, 31.0f, 31.0f, 223},
        {14.0f, 21.0f, 21.0f, 188}, {14.0f, 22.0f, 31.0f, 255},
        {14.0f, 32.0f, 32.0f, 188}, {15.0f, 21.0f, 31.0f, 255},
        {15.0f, 32.0f, 32.0f, 223}, {16.0f, 20.0f, 20.0f, 130},
        {16.0f, 21.0f, 32.0f, 255}, {17.0f, 20.0f, 20.0f, 188},
        {17.0f, 21.0f, 32.0f, 255}, {17.0f, 33.0f, 33.0f, 130},
        {18.0f, 20.0f, 20.0f, 188}, {18.0f, 21.0f, 32.0f, 255},
        {18.0f, 33.0f, 33.0f, 130}, {19.0f, 21.0f, 32.0f, 255},
        {20.0f, 21.0f, 21.0f, 223}, {20.0f, 22.0f, 31.0f, 255},
        {20.0f, 32.0f, 32.0f, 188}, {21.0f, 21.0f, 21.0f, 130},
        {21.0f, 22.0f, 31.0f, 255}, {22.0f, 22.0f, 22.0f, 130},
        {22.0f, 23.0f, 29.0f, 255}, {22.0f, 30.0f, 30.0f, 223},
        {22.0f, 31.0f, 31.0f, 130}, {23.0f, 24.0f, 24.0f, 188},
        {23.0f, 25.0f, 25.0f, 223}, {23.0f, 26.0f, 27.0f, 255},
        {23.0f, 28.0f, 28.0f, 223}, {23.0f, 29.0f, 29.0f, 188},
        {27.0f, 43.0f, 43.0f, 130}, {27.0f, 44.0f, 50.0f, 188},
        {28.0f, 43.0f, 49.0f, 255}, {28.0f, 50.0f, 50.0f, 188},
        {29.0f, 43.0f, 49.0f, 255}, {29.0f, 50.0f, 50.0f, 130},
        {30.0f, 42.0f, 42.0f, 130}, {30.0f, 43.0f, 49.0f, 255},
        {31.0f, 42.0f, 42.0f, 188}, {31.0f, 43.0f, 48.0f, 255},
        {31.0f, 49.0f, 49.0f, 188}, {32.0f, 42.0f, 42.0f, 188},
        {32.0f, 43.0f, 48.0f, 255}, {32.0f, 49.0f, 49.0f, 130},
        {33.0f, 42.0f, 48.0f, 255}, {34.0f, 41.0f, 41.0f, 130},
        {34.0f, 42.0f, 47.0f, 255}, {34.0f, 48.0f, 48.0f, 188},
        {35.0f, 41.0f, 41.0f, 130}, {35.0f, 42.0f, 47.0f, 255},
        {35.0f, 48.0f, 48.0f, 130}, {36.0f, 41.0f, 41.0f, 188},
        {36.0f, 42.0f, 47.0f, 255}, {37.0f, 41.0f, 46.0f, 255},
        {37.0f, 47.0f, 47.0f, 188}, {38.0f, 41.0f, 46.0f, 255},
        {38.0f, 47.0f, 47.0f, 130}, {39.0f, 40.0f, 40.0f, 130},
        {39.0f, 41.0f, 46.0f, 255}, {40.0f, 40.0f, 40.0f, 188},
        {40.0f, 41.0f, 46.0f, 255}, {41.0f, 40.0f, 40.0f, 188},
        {41.0f, 41.0f, 45.0f, 255}, {41.0f, 46.0f, 46.0f, 188},
        {42.0f, 40.0f, 45.0f, 255}, {42.0f, 46.0f, 46.0f, 130},
        {43.0f, 39.0f, 39.0f, 130}, {43.0f, 40.0f, 45.0f, 255},
        {44.0f, 40.0f, 44.0f, 188}, {44.0f, 45.0f, 45.0f, 130},
    };
    fill_reference_runs(runs, std::size(runs));
  }
}

void DrawPunctuationModeIcon(HDC dc,
                             RECT rect,
                             bool chinese_punctuation,
                             COLORREF color,
                             int stroke_width) {
  DrawMicrosoftPunctuationModeIcon(dc, rect, chinese_punctuation, color, stroke_width);
}

void DrawStatusPunctuationModeIcon(HDC dc,
                                   RECT rect,
                                   bool chinese_punctuation,
                                   COLORREF color,
                                   int stroke_width) {
  (void)stroke_width;
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    DrawMicrosoftPunctuationModeIcon(dc, rect, chinese_punctuation, color, 1);
    return;
  }

  Gdiplus::Graphics graphics(dc);
  graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

  const Gdiplus::REAL w = static_cast<Gdiplus::REAL>(rect.right - rect.left);
  const Gdiplus::REAL h = static_cast<Gdiplus::REAL>(rect.bottom - rect.top);
  const Gdiplus::REAL size = std::min(w, h);
  const Gdiplus::REAL left = static_cast<Gdiplus::REAL>(rect.left) + (w - size) / 2.0f;
  const Gdiplus::REAL top = static_cast<Gdiplus::REAL>(rect.top) + (h - size) / 2.0f;
  constexpr Gdiplus::REAL kReferenceLeft = 8.0f;
  constexpr Gdiplus::REAL kReferenceTop = 5.0f;
  constexpr Gdiplus::REAL kReferenceSize = 52.0f;
  const Gdiplus::REAL scale = size / kReferenceSize;
  auto x = [&](float value) {
    return left + (static_cast<Gdiplus::REAL>(value) - kReferenceLeft) * scale;
  };
  auto y = [&](float value) {
    return top + (static_cast<Gdiplus::REAL>(value) - kReferenceTop) * scale;
  };
  auto width = [&](float value) { return static_cast<Gdiplus::REAL>(value) * scale; };
  auto point = [&](float px, float py) { return Gdiplus::PointF{x(px), y(py)}; };

  Gdiplus::SolidBrush brush(
      Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));

  if (chinese_punctuation) {
    Gdiplus::GraphicsPath ring(Gdiplus::FillModeAlternate);
    ring.AddEllipse(x(20.2f), y(10.2f), width(14.2f), width(14.2f));
    ring.AddEllipse(x(23.5f), y(13.6f), width(7.7f), width(7.7f));
    graphics.FillPath(&brush, &ring);

    Gdiplus::GraphicsPath comma;
    comma.StartFigure();
    comma.AddBezier(point(46.3f, 23.9f),
                    point(42.5f, 23.9f),
                    point(40.9f, 26.4f),
                    point(40.9f, 30.6f));
    comma.AddBezier(point(40.9f, 30.6f),
                    point(40.9f, 34.7f),
                    point(44.5f, 36.8f),
                    point(48.2f, 36.0f));
    comma.AddBezier(point(48.2f, 36.0f),
                    point(47.5f, 39.0f),
                    point(44.6f, 44.0f),
                    point(39.6f, 49.0f));
    comma.AddLine(point(39.6f, 49.0f), point(42.0f, 49.0f));
    comma.AddBezier(point(42.0f, 49.0f),
                    point(49.6f, 43.5f),
                    point(54.0f, 37.7f),
                    point(54.0f, 30.9f));
    comma.AddBezier(point(54.0f, 30.9f),
                    point(54.0f, 26.4f),
                    point(50.4f, 24.0f),
                    point(46.3f, 23.9f));
    comma.CloseFigure();
    graphics.FillPath(&brush, &comma);
  } else {
    graphics.FillEllipse(&brush, x(20.0f), y(10.5f), width(14.0f), width(13.5f));

    Gdiplus::GraphicsPath stroke;
    stroke.StartFigure();
    stroke.AddLine(point(43.0f, 26.8f), point(50.9f, 26.8f));
    stroke.AddLine(point(48.6f, 34.0f), point(45.2f, 45.6f));
    stroke.AddLine(point(39.2f, 45.6f), point(42.4f, 26.8f));
    stroke.CloseFigure();
    graphics.FillPath(&brush, &stroke);
  }
}

void DrawSettingsIcon(HDC dc, RECT rect, COLORREF color, int stroke_width) {
  (void)stroke_width;
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, L"\u2699", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }
  DrawFluentPathIcon(dc, rect, kFluentSettings24, 24.0f, color, 0.90f);
}

void DrawRestartIcon(HDC dc, RECT rect, COLORREF color, int stroke_width) {
  (void)stroke_width;
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, L"\u21BB", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }
  DrawFluentPathIcon(dc, rect, kFluentArrowClockwise24, 24.0f, color, 0.90f);
}

void DrawShapeModeIcon(HDC dc, RECT rect, bool full_shape, COLORREF color, int stroke_width) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc,
              full_shape ? L"\u25CF" : L"\u25D4",
              -1,
              &rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }
  if (full_shape) {
    (void)stroke_width;
    DrawFluentPathIcon(dc, rect, kFluentCircle20Filled, 20.0f, color, kMicrosoftShapeModeIconScale);
  } else {
    DrawMoonIcon(dc, rect, color, stroke_width);
  }
}

void DrawToolbarShapeModeIcon(HDC dc, RECT rect, bool full_shape, COLORREF color, int stroke_width) {
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    DrawShapeModeIcon(dc, rect, full_shape, color, stroke_width);
    return;
  }
  if (full_shape) {
    (void)stroke_width;
    DrawFluentPathIcon(dc, rect, kFluentCircle20Filled, 20.0f, color, kToolbarShapeModeIconScale);
  } else {
    DrawFluentPathIcon(
        dc, rect, kFluentWeatherMoon24, 24.0f, color, 1.20f, -0.040f, 0.005f, 1.30f);
  }
}

void DrawToolbarSmileIcon(HDC dc, RECT rect, COLORREF color, int stroke_width) {
  (void)stroke_width;
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    DrawSmileIcon(dc, rect, color, stroke_width);
    return;
  }
  DrawFluentPathIcon(dc, rect, kFluentEmoji24, 24.0f, color, 1.18f);
}

void DrawToolbarSettingsIcon(HDC dc, RECT rect, COLORREF color, int stroke_width) {
  (void)stroke_width;
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    DrawSettingsIcon(dc, rect, color, stroke_width);
    return;
  }
  DrawFluentPathIcon(dc, rect, kFluentSettings24, 24.0f, color, 1.12f);
}

void DrawToolbarPunctuationTextIcon(HDC dc,
                                    RECT rect,
                                    bool chinese_punctuation,
                                    COLORREF color,
                                    UINT dpi) {
  const int left = rect.left + ScaleForDpi(4, dpi);
  const int top = rect.top + ScaleForDpi(5, dpi);
  const int size = ScaleForDpi(22, dpi);
  RECT icon_rect{left, top, left + size, top + size};
  DrawStatusPunctuationModeIcon(dc, icon_rect, chinese_punctuation, color, 1);
}

void DrawExpandChevron(HDC dc,
                       RECT rect,
                       bool expanded,
                       int direction,
                       COLORREF color,
                       int stroke_width) {
  (void)stroke_width;
  EnsureGdiplus();
  if (!g_gdiplus_ready) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    const wchar_t* fallback = expanded ? L"\u2303" : L"\u2304";
    if (direction > 0) {
      fallback = L"\u203A";
    } else if (direction < 0) {
      fallback = L"\u2039";
    }
    DrawTextW(dc, fallback, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }

  std::wstring_view icon = expanded ? kFluentChevronUp20 : kFluentChevronDown20;
  if (direction > 0) {
    icon = kFluentChevronRight20;
  } else if (direction < 0) {
    icon = kFluentChevronLeft20;
  }
  DrawFluentPathIcon(dc,
                     rect,
                     icon,
                     20.0f,
                     color,
                     1.13f,
                     direction == 0 ? -0.150f : 0.0f,
                     0.005f,
                     1.12f);
}

bool PtInRectInclusive(const RECT& rect, int x, int y) {
  if (rect.right <= rect.left || rect.bottom <= rect.top) {
    return false;
  }
  return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

struct ToolbarItemMetrics {
  int id;
  RECT hit_rect;
  RECT visual_rect;
  RECT feedback_rect;
};

std::vector<int> VisibleToolbarItemsWithSettings(const std::vector<int>& items);
int ToolbarWindowWidthPixels(bool vertical, size_t item_count, UINT dpi);
int ToolbarWindowHeightPixels(bool vertical, size_t item_count, UINT dpi);

struct ToolbarItemVisualDips {
  int left_offset;
  int top;
  int width;
  int height;
};

ToolbarItemVisualDips ToolbarItemVisualMetricsDips(int item) {
  switch (item) {
    case kToolbarItemInputMode:
      return {6, 3, 20, 30};
    case kToolbarItemShape:
      return {7, 2, 19, 32};
    case kToolbarItemPunctuation:
      return {-1, 3, 29, 30};
    case kToolbarItemCharset:
      return {3, 3, 24, 30};
    case kToolbarItemEmoji:
      return {6, 3, 21, 30};
    case kToolbarItemSettings:
      return {4, 3, 22, 30};
    default:
      return {4, 3, 24, 30};
  }
}

std::vector<ToolbarItemMetrics> ToolbarItemsForDpi(UINT dpi,
                                                   bool vertical,
                                                   const std::vector<int>& visible_items) {
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const std::vector<int> items = VisibleToolbarItemsWithSettings(visible_items);
  const auto item = [&](int id, size_t index) {
    const ToolbarItemVisualDips visual = ToolbarItemVisualMetricsDips(id);
    RECT visual_rect{};
    RECT slot_rect{};
    if (vertical) {
      const int slot_top = kToolbarDragWidthDips + static_cast<int>(index) * kToolbarItemSlotDips;
      const int visual_left = (kToolbarHeightDips - visual.height) / 2;
      const int visual_top = slot_top + (kToolbarItemSlotDips - visual.width) / 2;
      slot_rect = RECT{s(0), s(slot_top), s(kToolbarHeightDips), s(slot_top + kToolbarItemSlotDips)};
      visual_rect = RECT{s(visual_left),
                         s(visual_top),
                         s(visual_left + visual.height),
                         s(visual_top + visual.width)};
    } else {
      const int slot_left = kToolbarDragWidthDips + static_cast<int>(index) * kToolbarItemSlotDips;
      slot_rect = RECT{s(slot_left), s(0), s(slot_left + kToolbarItemSlotDips), s(kToolbarHeightDips)};
      visual_rect = RECT{s(slot_left + visual.left_offset),
                         s(visual.top),
                         s(slot_left + visual.left_offset + visual.width),
                         s(visual.top + visual.height)};
    }
    const int feedback_width =
        vertical ? s(kToolbarFeedbackHeightDips) : s(kToolbarFeedbackWidthDips);
    const int feedback_height =
        vertical ? s(kToolbarFeedbackWidthDips) : s(kToolbarFeedbackHeightDips);
    const int toolbar_width = ToolbarWindowWidthPixels(vertical, items.size(), dpi);
    const int toolbar_height = ToolbarWindowHeightPixels(vertical, items.size(), dpi);
    const int edge_inset = s(kToolbarFeedbackEdgeInsetDips);
    const int slot_width = std::max(1, static_cast<int>(slot_rect.right - slot_rect.left));
    const int slot_height = std::max(1, static_cast<int>(slot_rect.bottom - slot_rect.top));
    const int centered_feedback_left =
        slot_rect.left + (slot_width - feedback_width) / 2;
    const int feedback_left =
        std::clamp(centered_feedback_left,
                   edge_inset,
                   std::max(edge_inset, toolbar_width - edge_inset - feedback_width));
    const int feedback_top = slot_rect.top + (slot_height - feedback_height) / 2;
    const int clamped_feedback_top =
        std::clamp(feedback_top,
                   edge_inset,
                   std::max(edge_inset, toolbar_height - edge_inset - feedback_height));
    const RECT feedback_rect{feedback_left,
                             clamped_feedback_top,
                             feedback_left + feedback_width,
                             clamped_feedback_top + feedback_height};
    if (id == kToolbarItemCharset || id == kToolbarItemSettings) {
      const int visual_width =
          std::max(1, static_cast<int>(visual_rect.right - visual_rect.left));
      const int visual_height =
          std::max(1, static_cast<int>(visual_rect.bottom - visual_rect.top));
      const int visual_left = (feedback_rect.left + feedback_rect.right - visual_width) / 2;
      const int visual_top = (feedback_rect.top + feedback_rect.bottom - visual_height) / 2;
      visual_rect = RECT{visual_left,
                         visual_top,
                         visual_left + visual_width,
                         visual_top + visual_height};
    }
    return ToolbarItemMetrics{
        id,
        feedback_rect,
        visual_rect,
        feedback_rect,
    };
  };

  std::vector<ToolbarItemMetrics> metrics;
  metrics.reserve(items.size());
  for (size_t index = 0; index < items.size(); ++index) {
    metrics.push_back(item(items[index], index));
  }
  return metrics;
}

RECT ToolbarItemIconRect(const ToolbarItemMetrics& item, UINT dpi, int inset_dips = 1) {
  const int inset = ScaleForDpi(inset_dips, dpi);
  RECT rect{item.visual_rect.left + inset,
            item.visual_rect.top + inset,
            item.visual_rect.right - inset,
            item.visual_rect.bottom - inset};
  if (rect.right <= rect.left || rect.bottom <= rect.top) {
    return item.visual_rect;
  }
  return rect;
}

int ToolbarItemAtPoint(UINT dpi, bool vertical, const std::vector<int>& visible_items, int x, int y) {
  if ((!vertical && x < ScaleForDpi(kToolbarDragWidthDips, dpi)) ||
      (vertical && y < ScaleForDpi(kToolbarDragWidthDips, dpi))) {
    return kToolbarItemDrag;
  }
  const std::vector<ToolbarItemMetrics> items =
      ToolbarItemsForDpi(dpi, vertical, visible_items);
  for (const auto& item : items) {
    if (PtInRectInclusive(item.hit_rect, x, y)) {
      return item.id;
    }
  }
  return kToolbarItemNone;
}

std::wstring ToolbarTooltipText(int item,
                                bool ascii_mode,
                                bool full_shape_mode,
                                bool chinese_punctuation_mode,
                                bool simplified_charset) {
  (void)ascii_mode;
  (void)full_shape_mode;
  (void)chinese_punctuation_mode;
  (void)simplified_charset;
  switch (item) {
    case kToolbarItemInputMode:
      return TextWithShortcut(L"\u4E2D/\u82F1\u6587", L"shortcut_toolbar_input_mode", L"Shift");
    case kToolbarItemShape:
      return TextWithShortcut(L"\u5168/\u534A\u89D2", L"shortcut_toolbar_shape", L"Shift+.");
    case kToolbarItemPunctuation:
      return TextWithShortcut(L"\u4E2D/\u82F1\u6587\u6807\u70B9",
                              L"shortcut_toolbar_punctuation",
                              L"Ctrl+.");
    case kToolbarItemCharset:
      return TextWithShortcut(L"\u7B80\u4F53/\u7E41\u4F53\u4E2D\u6587\u5B57\u7B26",
                              L"shortcut_toolbar_charset",
                              L"Ctrl+Shift+F");
    case kToolbarItemEmoji:
      return L"\u8868\u60C5\u7B26\u53F7/\u7B26\u53F7";
    case kToolbarItemSettings:
      return L"\u8BBE\u7F6E";
    default:
      return L"";
  }
}

int ToolbarTooltipCornerRadius(UINT dpi) {
  return ScaleHalfDipForDpi(kToolbarTooltipCornerRadiusHalfDips, dpi);
}

std::wstring Trim(std::wstring_view value) {
  size_t first = 0;
  while (first < value.size() && std::iswspace(value[first])) {
    ++first;
  }
  size_t last = value.size();
  while (last > first && std::iswspace(value[last - 1])) {
    --last;
  }
  return std::wstring(value.substr(first, last - first));
}

std::wstring ToLower(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  return value;
}

bool ContainsToolbarItem(const std::vector<int>& items, int item) {
  return std::find(items.begin(), items.end(), item) != items.end();
}

std::vector<int> ParseToolbarVisibleItems(std::wstring_view value) {
  if (Trim(value).empty()) {
    return DefaultToolbarVisibleItems();
  }
  std::vector<int> items;
  size_t start = 0;
  while (start <= value.size()) {
    const size_t separator = value.find(L',', start);
    const size_t end = separator == std::wstring_view::npos ? value.size() : separator;
    const std::wstring token = ToLower(Trim(value.substr(start, end - start)));
    for (const auto& definition : kToolbarCustomItemDefinitions) {
      if (token == definition.setting_id && !ContainsToolbarItem(items, definition.id)) {
        items.push_back(definition.id);
        break;
      }
    }
    if (separator == std::wstring_view::npos) {
      break;
    }
    start = separator + 1;
  }
  if (!ContainsToolbarItem(items, kToolbarItemSettings)) {
    items.push_back(kToolbarItemSettings);
  }
  return items.empty() ? DefaultToolbarVisibleItems() : items;
}

std::wstring SerializeToolbarVisibleItems(const std::vector<int>& items) {
  std::wstring value;
  for (int item : items) {
    const std::wstring_view id = ToolbarItemSettingId(item);
    if (id.empty()) {
      continue;
    }
    if (!value.empty()) {
      value += L",";
    }
    value += id;
  }
  return value;
}

std::vector<int> VisibleToolbarItemsWithSettings(const std::vector<int>& items) {
  std::vector<int> visible;
  visible.reserve(kToolbarCustomItemDefinitions.size());
  for (const auto& definition : kToolbarCustomItemDefinitions) {
    if (ContainsToolbarItem(items, definition.id)) {
      visible.push_back(definition.id);
    }
  }
  if (!ContainsToolbarItem(visible, kToolbarItemSettings)) {
    visible.push_back(kToolbarItemSettings);
  }
  return visible;
}

int ScaleToolbarHalfDipsFloor(int half_dips, UINT dpi) {
  return static_cast<int>((static_cast<long long>(half_dips) * static_cast<long long>(dpi)) / 192);
}

int ToolbarThicknessPixels(UINT dpi) {
  return ScaleToolbarHalfDipsFloor(kToolbarHeightDips * 2, dpi);
}

int ToolbarWindowWidthPixels(bool vertical, size_t item_count, UINT dpi) {
  if (vertical) {
    return ToolbarThicknessPixels(dpi);
  }
  const int base_dips =
      kToolbarDragWidthDips + static_cast<int>(item_count) * kToolbarItemSlotDips;
  return ScaleForDpi(base_dips, dpi) + ScaleToolbarHalfDipsFloor(kToolbarTailPaddingHalfDips, dpi);
}

int ToolbarWindowHeightPixels(bool vertical, size_t item_count, UINT dpi) {
  if (!vertical) {
    return ToolbarThicknessPixels(dpi);
  }
  const int base_dips =
      kToolbarDragWidthDips + static_cast<int>(item_count) * kToolbarItemSlotDips;
  return ScaleForDpi(base_dips, dpi) + ScaleToolbarHalfDipsFloor(kToolbarTailPaddingHalfDips, dpi);
}

bool IsUsableScreenPoint(POINT point) {
  const int virtual_left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int virtual_top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int virtual_right = virtual_left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int virtual_bottom = virtual_top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (point.x == 0 && point.y == 0) {
    return false;
  }
  return point.x >= virtual_left && point.x < virtual_right && point.y >= virtual_top &&
         point.y < virtual_bottom;
}

std::wstring ProcessNameFromWindow(HWND window) {
  if (window == nullptr) {
    return {};
  }

  DWORD process_id = 0;
  GetWindowThreadProcessId(window, &process_id);
  if (process_id == 0) {
    return {};
  }

  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
  if (process == nullptr) {
    process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id);
  }
  if (process == nullptr) {
    return {};
  }

  std::wstring path(32768, L'\0');
  DWORD size = static_cast<DWORD>(path.size());
  std::wstring name;
  if (QueryFullProcessImageNameW(process, 0, path.data(), &size) && size != 0) {
    path.resize(size);
    name = std::filesystem::path(path).filename().wstring();
  }
  CloseHandle(process);
  return name;
}

std::wstring ForegroundProcessName() {
  return ProcessNameFromWindow(GetForegroundWindow());
}

bool ProcessNameMatchesPattern(std::wstring process_name, std::wstring pattern) {
  process_name = ToLower(Trim(process_name));
  pattern = ToLower(Trim(pattern));
  if (process_name.empty() || pattern.empty()) {
    return false;
  }
  if (pattern.find_first_of(L"*?") != std::wstring::npos) {
    return PathMatchSpecW(process_name.c_str(), pattern.c_str()) == TRUE;
  }
  if (process_name == pattern) {
    return true;
  }
  if (pattern.find(L'.') == std::wstring::npos) {
    const std::wstring exe_pattern = pattern + L".exe";
    return process_name == exe_pattern;
  }
  return false;
}

bool ProcessNameInList(std::wstring_view process_name, std::wstring_view list) {
  size_t start = 0;
  while (start < list.size()) {
    size_t end = list.find_first_of(L",;\r\n", start);
    if (end == std::wstring_view::npos) {
      end = list.size();
    }
    if (ProcessNameMatchesPattern(std::wstring(process_name),
                                  std::wstring(list.substr(start, end - start)))) {
      return true;
    }
    start = end + 1;
  }
  return false;
}

bool IsUsableTextRect(const RECT& rect) {
  if (rect.left == 0 && rect.top == 0 && rect.right == 0 && rect.bottom == 0) {
    return false;
  }
  if (rect.right < rect.left || rect.bottom < rect.top) {
    return false;
  }
  POINT point{rect.left, rect.bottom};
  return IsUsableScreenPoint(point);
}

bool SetCompartmentDword(ITfThreadMgr* thread_mgr,
                         TfClientId client_id,
                         REFGUID guid,
                         DWORD value) {
  if (thread_mgr == nullptr) {
    return false;
  }

  ITfCompartmentMgr* compartment_mgr = nullptr;
  HRESULT result = thread_mgr->QueryInterface(IID_ITfCompartmentMgr,
                                              reinterpret_cast<void**>(&compartment_mgr));
  if (FAILED(result) || compartment_mgr == nullptr) {
    return false;
  }

  ITfCompartment* compartment = nullptr;
  result = compartment_mgr->GetCompartment(guid, &compartment);
  compartment_mgr->Release();
  if (FAILED(result) || compartment == nullptr) {
    return false;
  }

  VARIANT variant{};
  VariantInit(&variant);
  variant.vt = VT_I4;
  variant.lVal = static_cast<LONG>(value);
  result = compartment->SetValue(client_id, &variant);
  compartment->Release();
  return SUCCEEDED(result);
}

bool SetGlobalCompartmentDword(ITfThreadMgr* thread_mgr,
                               TfClientId client_id,
                               REFGUID guid,
                               DWORD value) {
  if (thread_mgr == nullptr) {
    return false;
  }

  ITfCompartmentMgr* compartment_mgr = nullptr;
  HRESULT result = thread_mgr->GetGlobalCompartment(&compartment_mgr);
  if (FAILED(result) || compartment_mgr == nullptr) {
    return false;
  }

  ITfCompartment* compartment = nullptr;
  result = compartment_mgr->GetCompartment(guid, &compartment);
  compartment_mgr->Release();
  if (FAILED(result) || compartment == nullptr) {
    return false;
  }

  VARIANT variant{};
  VariantInit(&variant);
  variant.vt = VT_I4;
  variant.lVal = static_cast<LONG>(value);
  result = compartment->SetValue(client_id, &variant);
  compartment->Release();
  return SUCCEEDED(result);
}

struct CandidateLayoutMetrics {
  int width = 0;
  int height = 0;
  int top_height = 0;
  int row_height = 0;
  int content_left = 0;
  int content_right = 0;
  int candidate_top = 0;
  int expanded_columns = 0;
  RECT emoji_rect{};
  RECT previous_page_rect{};
  RECT next_page_rect{};
  RECT expand_rect{};
  RECT emoji_icon_rect{};
  RECT previous_page_icon_rect{};
  RECT next_page_icon_rect{};
  RECT expand_icon_rect{};
  RECT settings_rect{};
  RECT brand_rect{};
  RECT brand_icon_rect{};
  RECT brand_text_rect{};
  RECT tool_separator_rect{};
  RECT emoji_separator_rect{};
  RECT expand_separator_rect{};
  RECT expanded_header_separator_rect{};
  RECT expanded_footer_separator_rect{};
  std::vector<RECT> expanded_horizontal_separator_rects;
  std::vector<RECT> expanded_vertical_separator_rects;
  std::vector<RECT> tool_row_separator_rects;
  std::vector<RECT> candidate_rects;
  std::vector<int> candidate_rows;
  std::vector<int> candidate_columns;
};

thread_local const CandidateLayoutMetrics* g_candidate_render_layout = nullptr;

class CandidateRenderLayoutScope {
 public:
  explicit CandidateRenderLayoutScope(const CandidateLayoutMetrics* layout)
      : previous_(g_candidate_render_layout) {
    g_candidate_render_layout = layout;
  }

  CandidateRenderLayoutScope(const CandidateRenderLayoutScope&) = delete;
  CandidateRenderLayoutScope& operator=(const CandidateRenderLayoutScope&) = delete;

  ~CandidateRenderLayoutScope() {
    g_candidate_render_layout = previous_;
  }

 private:
  const CandidateLayoutMetrics* previous_ = nullptr;
};

struct CandidateLayoutCacheEntry {
  bool valid = false;
  HWND window = nullptr;
  UINT dpi = 0;
  bool horizontal = true;
  bool expanded = false;
  int compact_count = 0;
  int candidate_font_point_size = 0;
  CandidateFontFamily candidate_font_family = CandidateFontFamily::kMiSans;
  bool traditional = false;
  size_t candidate_count = 0;
  size_t candidate_hash = 0;
  RECT work_area{};
  CandidateLayoutMetrics layout;
};

thread_local CandidateLayoutCacheEntry g_candidate_layout_cache;

void InvalidateCandidateLayoutCache() {
  g_candidate_layout_cache.valid = false;
}

size_t HashCandidateLayoutInputs(
    const std::vector<fp::core::RimeCandidateView>& candidates) {
  size_t hash = candidates.size();
  std::hash<std::wstring> hasher;
  for (const auto& candidate : candidates) {
    hash ^= hasher(candidate.text) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    hash ^= hasher(candidate.comment) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
  }
  return hash;
}

RECT CandidateToolRect(const CandidateLayoutMetrics& layout, int tool) {
  switch (tool) {
    case kCandidateToolPrevious:
      return layout.previous_page_rect;
    case kCandidateToolNext:
      return layout.next_page_rect;
    case kCandidateToolEmoji:
      return layout.emoji_rect;
    case kCandidateToolExpand:
      return layout.expand_rect;
    case kCandidateToolSettings:
      return layout.settings_rect;
    case kCandidateToolBrand:
      return layout.brand_rect;
    default:
      return RECT{};
  }
}

RECT CandidateToolIconRect(const CandidateLayoutMetrics& layout, int tool) {
  switch (tool) {
    case kCandidateToolPrevious:
      return layout.previous_page_icon_rect.right > layout.previous_page_icon_rect.left
                 ? layout.previous_page_icon_rect
                 : layout.previous_page_rect;
    case kCandidateToolNext:
      return layout.next_page_icon_rect.right > layout.next_page_icon_rect.left
                 ? layout.next_page_icon_rect
                 : layout.next_page_rect;
    case kCandidateToolEmoji:
      return layout.emoji_icon_rect.right > layout.emoji_icon_rect.left ? layout.emoji_icon_rect
                                                                         : layout.emoji_rect;
    case kCandidateToolExpand:
      return layout.expand_icon_rect.right > layout.expand_icon_rect.left ? layout.expand_icon_rect
                                                                          : layout.expand_rect;
    default:
      return CandidateToolRect(layout, tool);
  }
}

int CandidateToolAtPoint(const CandidateLayoutMetrics& layout, int x, int y) {
  if (PtInRectInclusive(layout.previous_page_rect, x, y)) {
    return kCandidateToolPrevious;
  }
  if (PtInRectInclusive(layout.next_page_rect, x, y)) {
    return kCandidateToolNext;
  }
  if (PtInRectInclusive(layout.emoji_rect, x, y)) {
    return kCandidateToolEmoji;
  }
  if (PtInRectInclusive(layout.expand_rect, x, y)) {
    return kCandidateToolExpand;
  }
  if (PtInRectInclusive(layout.settings_rect, x, y)) {
    return kCandidateToolSettings;
  }
  if (PtInRectInclusive(layout.brand_rect, x, y)) {
    return kCandidateToolBrand;
  }
  return kCandidateToolNone;
}

bool IsCandidateToolEnabled(int tool, bool has_previous_page, bool has_next_page) {
  switch (tool) {
    case kCandidateToolPrevious:
      return has_previous_page;
    case kCandidateToolNext:
      return has_next_page;
    case kCandidateToolEmoji:
    case kCandidateToolExpand:
    case kCandidateToolSettings:
    case kCandidateToolBrand:
      return true;
    default:
      return false;
  }
}

std::wstring CandidateToolTooltipText(int tool, bool expanded) {
  switch (tool) {
    case kCandidateToolPrevious:
      return TextWithShortcut(L"\u4E0A\u4E00\u9875", L"shortcut_candidate_previous_page", L"PgUp");
    case kCandidateToolNext:
      return TextWithShortcut(L"\u4E0B\u4E00\u9875", L"shortcut_candidate_next_page", L"PgDn");
    case kCandidateToolEmoji:
      return std::wstring(L"\u8868\u60C5\u7B26\u53F7");
    case kCandidateToolExpand:
      return TextWithShortcut(expanded ? L"\u6536\u8D77" : L"\u5C55\u5F00",
                              L"shortcut_candidate_expand",
                              L"Tab");
    case kCandidateToolSettings:
      return std::wstring(L"\u8BBE\u7F6E");
    case kCandidateToolBrand:
      return std::wstring(L"\u5B98\u7F51\u94FE\u63A5");
    default:
      return std::wstring();
  }
}

int CandidateToolTooltipHorizontalPadding(UINT dpi) {
  return ScaleForDpi(kCandidateToolTooltipPaddingDips, dpi);
}

int CandidateToolTooltipHeight(UINT dpi) {
  return ScaleForDpi(kCandidateToolTooltipHeightDips, dpi);
}

int CandidateToolTooltipOverlap(UINT dpi) {
  return ScaleForDpi(kCandidateToolTooltipOverlapDips, dpi);
}

int CandidateToolTooltipCornerRadius(UINT dpi) {
  return ScaleHalfDipForDpi(kCandidateToolTooltipCornerRadiusHalfDips, dpi);
}

RECT CandidateToolFeedbackRect(RECT rect, int tool, UINT dpi, bool centered_tools) {
  if (tool == kCandidateToolBrand) {
    const int vertical_padding = ScaleForDpi(5, dpi);
    const int horizontal_padding = 0;
    RECT feedback{rect.left - horizontal_padding,
                  rect.top - vertical_padding,
                  rect.right + horizontal_padding,
                  rect.bottom + vertical_padding};
    if (feedback.bottom <= feedback.top) {
      feedback.top = rect.top;
      feedback.bottom = rect.bottom;
    }
    return feedback;
  }
  const int side = ScaleHalfDipForDpi(kCandidateToolFeedbackSize * 2 - 1, dpi);
  if (centered_tools && tool >= kCandidateToolPrevious && tool <= kCandidateToolExpand) {
    const int rect_width = rect.right - rect.left;
    const int rect_height = rect.bottom - rect.top;
    if (rect_width <= rect_height + ScaleForDpi(4, dpi)) {
      return rect;
    }
    const int vertical_inset = ScaleHalfDipForDpi(3, dpi);
    const int bottom_inset = ScaleHalfDipForDpi(7, dpi);
    RECT feedback{rect.left,
                  rect.top + vertical_inset,
                  rect.right,
                  rect.bottom - bottom_inset};
    if (feedback.bottom <= feedback.top) {
      feedback.top = rect.top;
      feedback.bottom = rect.bottom;
    }
    return feedback;
  }
  const int center_x = rect.left + (rect.right - rect.left) / 2;
  const int center_y = rect.top + (rect.bottom - rect.top) / 2;
  int visual_offset_x = 0;
  if (!centered_tools) {
    switch (tool) {
      case kCandidateToolPrevious:
        visual_offset_x = -ScaleHalfDipForDpi(4, dpi);
        break;
      case kCandidateToolEmoji:
        visual_offset_x = -ScaleHalfDipForDpi(3, dpi);
        break;
      case kCandidateToolExpand:
        visual_offset_x = -ScaleHalfDipForDpi(6, dpi);
        break;
      case kCandidateToolSettings:
        break;
      default:
        break;
    }
  }
  const int left = center_x + visual_offset_x - side / 2 + ScaleHalfDipForDpi(1, dpi);
  const int top = center_y - side / 2 + ScaleHalfDipForDpi(1, dpi);
  return RECT{left, top, left + side, top + side};
}

void DrawCandidateToolFeedback(HDC dc,
                               const CandidateLayoutMetrics& layout,
                               UINT dpi,
                               int hovered_tool,
                               int pressed_tool,
                               bool centered_tools,
                               COLORREF hover_color,
                               COLORREF pressed_color) {
  for (int tool = kCandidateToolPrevious; tool <= kCandidateToolBrand; ++tool) {
    const RECT tool_rect = CandidateToolRect(layout, tool);
    if (tool_rect.right <= tool_rect.left || tool_rect.bottom <= tool_rect.top) {
      continue;
    }
    const bool pressed = pressed_tool == tool;
    const bool hovered = hovered_tool == tool;
    if (!pressed && !hovered) {
      continue;
    }
    FillRoundedRectAntialias(dc,
                             CandidateToolFeedbackRect(tool_rect, tool, dpi, centered_tools),
                             ScaleForDpi(4, dpi),
                             pressed ? pressed_color : hover_color);
  }
}

CandidateLayoutMetrics CalculateCandidateLayout(HWND window,
                                                HDC dc,
                                                UINT dpi,
                                                bool horizontal,
                                                bool expanded,
                                                int compact_count,
                                                int candidate_font_point_size,
                                                bool traditional,
                                                const std::vector<fp::core::RimeCandidateView>& candidates,
                                                const RECT* work_area_override = nullptr) {
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const int candidate_limit =
      expanded ? ExpandedCandidatePageSize(horizontal, compact_count)
               : CompactCandidateCount(compact_count);
  size_t count = std::min(candidates.size(), static_cast<size_t>(candidate_limit));
  CandidateLayoutMetrics layout;
  layout.top_height = s(0);
  layout.row_height = s(CandidateItemHeightDips(30, candidate_font_point_size));
  layout.content_left = s(8);
  layout.content_right = s(8);
  layout.candidate_top = horizontal ? s(2) : s(kVerticalCandidateTopDips);

  RECT work_area{};
  if (work_area_override != nullptr) {
    work_area = *work_area_override;
  } else if (window != nullptr) {
    RECT window_rect{};
    if (GetWindowRect(window, &window_rect)) {
      const POINT window_center{window_rect.left + (window_rect.right - window_rect.left) / 2,
                                window_rect.top + (window_rect.bottom - window_rect.top) / 2};
      work_area = WorkAreaForPoint(window_center);
    }
  }
  if (work_area.right <= work_area.left || work_area.bottom <= work_area.top) {
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  }
  const int work_width = static_cast<int>(work_area.right - work_area.left);
  const int width_cap =
      expanded ? s(kCandidateWindowExpandedMaxWidth) : s(kCandidateWindowCompactMaxWidth);
  const int max_width = std::max(s(280), std::min(work_width - s(80), width_cap));

  const int tool_button = s(kCandidateToolButtonSize);
  const int page_tool_gap = s(8);
  const int tool_group_gap = ScaleHalfDipForDpi(21, dpi);
  const int tool_right_margin = s(2);
  const int separator_width = HairlineForDpi(dpi);
  const int tool_separator_offset = ScaleHalfDipForDpi(17, dpi);
  const int separator_top = ScaleHalfDipForDpi(7, dpi);
  const int separator_bottom = ScaleHalfDipForDpi(5, dpi);
  const int bottom_tool_bar_height = s(kVerticalFooterHeightDips);
  const int bottom_tool_group_width = 4 * tool_button + 3 * page_tool_gap;
  const int bottom_tool_min_width = bottom_tool_group_width + s(16);
  const int brand_icon_size = s(kHorizontalExpandedBrandIconSizeDips);
  const int horizontal_footer_height =
      std::max(s(kHorizontalExpandedFooterHeightDips),
               brand_icon_size + 2 * s(kHorizontalExpandedBrandVerticalInsetDips));
  const int vertical_brand_rail_width =
      std::max(s(kVerticalExpandedBrandRailWidthDips),
               brand_icon_size + 2 * s(kHorizontalExpandedBrandVerticalInsetDips));
  const int compact_vertical_tool_column_width =
      s(kVerticalCompactToolColumnLeftDips) + tool_button + s(kVerticalCompactToolIconRightInsetDips);
  const int min_width =
      expanded
          ? (horizontal ? s(360) : compact_vertical_tool_column_width + s(220))
          : (horizontal ? s(280) : compact_vertical_tool_column_width);
  int width = min_width;
  const int compact_tool_zone_width = tool_right_margin + tool_separator_offset +
                                      (4 * tool_button + 2 * page_tool_gap + tool_group_gap);
  const int tool_zone_width = compact_tool_zone_width;
  const int right_gutter = s(kCandidateToolCandidateGap) + tool_zone_width;
  const auto sum_widths = [](const std::vector<int>& widths) {
    int total = 0;
    for (const int value : widths) {
      total += value;
    }
    return total;
  };
  const auto shrink_widths = [](std::vector<int>& widths, int min_item_width, int target_total) {
    int current_total = 0;
    for (const int value : widths) {
      current_total += value;
    }
    while (current_total > target_total) {
      auto widest = widths.end();
      int shrinkable = 0;
      for (auto iter = widths.begin(); iter != widths.end(); ++iter) {
        const int current_shrinkable = *iter - min_item_width;
        if (current_shrinkable > shrinkable) {
          shrinkable = current_shrinkable;
          widest = iter;
        }
      }
      if (widest == widths.end() || shrinkable <= 0) {
        break;
      }
      --(*widest);
      --current_total;
    }
  };

  if (expanded && horizontal) {
    // Horizontal expansion keeps the compact first row as the width baseline
    // and appends three native-style equal-height rows below it.
    const int left = s(4);
    const int right = s(6);
    const int gap_x = s(7);
    const int item_height = s(CandidateItemHeightDips(33, candidate_font_point_size));
    const int row_step = s(CandidateRowStepDips(34, candidate_font_point_size));
    const int min_item_width = s(59);
    const int max_normal_item_width = s(176);

    int first_row_count = std::min(CompactCandidateCount(compact_count),
                                   static_cast<int>(count));
    if (first_row_count > 1 && !candidates.empty()) {
      const int first_desired_width =
          static_cast<int>(MeasureTextWithFallback(dc,
                                                   candidates.front().text,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   traditional)
                               .cx +
                           MeasureTextWithFallback(dc,
                                                   candidates.front().comment,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   traditional)
                               .cx) +
          s(48);
      auto first_capacity_for_count = [&](int visible_count) {
        const int gaps = visible_count > 0 ? (visible_count - 1) * gap_x : 0;
        const int available_candidate_width =
            max_width - left - right - right_gutter - gaps;
        const int tail_min_total =
            visible_count > 1 ? (visible_count - 1) * min_item_width : 0;
        return available_candidate_width - tail_min_total;
      };
      while (first_row_count > 1 &&
             first_desired_width > first_capacity_for_count(first_row_count)) {
        --first_row_count;
      }
    }

    const auto measure_item_width = [&](size_t index, bool first_item_in_first_row) {
      const auto text_size = MeasureTextWithFallback(dc,
                                                     candidates[index].text,
                                                     candidate_font_point_size,
                                                     dpi,
                                                     FW_NORMAL,
                                                     traditional);
      const auto comment_size = MeasureTextWithFallback(dc,
                                                        candidates[index].comment,
                                                        candidate_font_point_size,
                                                        dpi,
                                                        FW_NORMAL,
                                                        traditional);
      return static_cast<int>(text_size.cx + comment_size.cx) +
             (first_item_in_first_row ? s(48) : s(43));
    };
    const auto make_row_widths = [&](size_t start,
                                     int row_count,
                                     int available_candidate_width,
                                     bool first_row) {
      std::vector<int> item_widths;
      item_widths.reserve(static_cast<size_t>(row_count));
      const int first_capacity =
          first_row && row_count > 1
              ? std::max(min_item_width,
                         available_candidate_width -
                             (row_count - 1) * min_item_width)
              : available_candidate_width;
      for (int column = 0; column < row_count; ++column) {
        const size_t index = start + static_cast<size_t>(column);
        const bool first_item_in_first_row = first_row && column == 0;
        const int item_max_width =
            first_item_in_first_row ? first_capacity : max_normal_item_width;
        item_widths.push_back(std::clamp(measure_item_width(index, first_item_in_first_row),
                                         min_item_width,
                                         std::max(min_item_width, item_max_width)));
      }
      int current_total = sum_widths(item_widths);
      if (current_total > available_candidate_width && item_widths.size() > 1) {
        std::vector<int> tail_widths(item_widths.begin() + 1, item_widths.end());
        shrink_widths(tail_widths,
                       min_item_width,
                       std::max(0, available_candidate_width - item_widths.front()));
        for (size_t index = 1; index < item_widths.size(); ++index) {
          item_widths[index] = tail_widths[index - 1];
        }
        current_total = sum_widths(item_widths);
        if (current_total > available_candidate_width) {
          item_widths.front() = std::max(
              min_item_width, item_widths.front() - (current_total - available_candidate_width));
        }
      } else {
        shrink_widths(item_widths, min_item_width, available_candidate_width);
      }
      return item_widths;
    };

    const int first_row_available =
        max_width - left - right - right_gutter -
        (first_row_count > 0 ? (first_row_count - 1) * gap_x : 0);
    std::vector<int> first_row_widths =
        first_row_count > 0
            ? make_row_widths(0, first_row_count, first_row_available, true)
            : std::vector<int>{};
    if (first_row_count == 0) {
      width = std::max(width, s(140));
    } else {
      width = std::max(width,
                       left + right + right_gutter + sum_widths(first_row_widths) +
                           (first_row_count - 1) * gap_x + ScaleHalfDipForDpi(7, dpi));
    }
    width = std::min(std::max(width, min_width), max_width);

    const int max_tail_columns = kMaxCompactCandidateCount;
    layout.candidate_rects.reserve(count);
    layout.candidate_rows.reserve(count);
    layout.candidate_columns.reserve(count);

    int x = left;
    for (int column = 0; column < first_row_count; ++column) {
      RECT rect{x,
                layout.candidate_top,
                x + first_row_widths[static_cast<size_t>(column)],
                layout.candidate_top + item_height};
      layout.candidate_rects.push_back(rect);
      layout.candidate_rows.push_back(0);
      layout.candidate_columns.push_back(column);
      x += first_row_widths[static_cast<size_t>(column)] + gap_x;
    }

    const int tail_left = left;
    const int tail_right = width - right;
    const int tail_width = std::max(0, tail_right - tail_left);
    const int max_columns_for_width =
        std::max(1, (tail_width + gap_x) / (min_item_width + gap_x));
    const int tail_columns =
        std::clamp(max_columns_for_width, 1, max_tail_columns);
    layout.expanded_columns = tail_columns;
    const int tail_available =
        std::max(0, tail_right - tail_left - (tail_columns - 1) * gap_x);
    const int tail_cell_width =
        tail_columns > 0 ? tail_available / tail_columns : tail_available;
    const int tail_extra =
        tail_columns > 0 ? tail_available - tail_cell_width * tail_columns : 0;
    size_t index = static_cast<size_t>(first_row_count);
    for (int row = 1; row <= kHorizontalExpandedTailRows && index < count; ++row) {
      const int row_item_count = std::min(tail_columns, static_cast<int>(count - index));
      int row_x = tail_left;
      for (int column = 0; column < tail_columns; ++column) {
        const int cell_width = tail_cell_width + (column < tail_extra ? 1 : 0);
        if (column < row_item_count) {
          const int top = layout.candidate_top + row * row_step;
          RECT rect{row_x, top, row_x + cell_width, top + item_height};
          layout.candidate_rects.push_back(rect);
          layout.candidate_rows.push_back(row);
          layout.candidate_columns.push_back(column);
          ++index;
        }
        row_x += cell_width + gap_x;
      }
    }
    layout.height =
        row_step * (1 + kHorizontalExpandedTailRows) +
        s(kHorizontalExpandedFooterGapDips) + horizontal_footer_height;
  } else if (expanded) {
    // Vertical expansion is the horizontal-expanded template transposed:
    // compact vertical stays as the first column, then three equal-width
    // columns expand to the right, followed by a vertical brand/settings rail.
    const int left = s(4);
    const int right = s(kHorizontalExpandedSettingsBottomInsetDips);
    const int gap_x = s(7);
    const int item_height =
        s(CandidateItemHeightDips(kVerticalCandidateRowHeightDips, candidate_font_point_size));
    const int row_step =
        s(CandidateRowStepDips(kVerticalCandidateRowStepDips, candidate_font_point_size));
    const int first_column_count =
        std::min(CompactCandidateCount(compact_count), static_cast<int>(count));
    const int tail_columns = kHorizontalExpandedTailRows;
    const int min_item_width = s(kVerticalCompactMinItemWidthDips);
    const int min_tail_column_width = s(59);
    const int max_normal_item_width = s(176);
    const int number_width =
        std::max(s(9),
                 static_cast<int>(MeasureTextWithFallback(dc,
                                                          L"9",
                                                          candidate_font_point_size,
                                                          dpi,
                                                          FW_NORMAL,
                                                          traditional)
                                      .cx));
    const int text_extra_width = s(13) + number_width + s(2) + s(12);
    const int brand_rail_width = vertical_brand_rail_width;
    const int side_gap = s(kHorizontalExpandedFooterGapDips);
    const int compact_page_gap = s(4);
    const int compact_group_gap = s(12);

    int first_column_width = min_item_width;
    layout.candidate_rects.reserve(count);
    layout.candidate_rows.reserve(count);
    layout.candidate_columns.reserve(count);
    for (int index = 0; index < first_column_count; ++index) {
      const int text_width =
          static_cast<int>(MeasureTextWithFallback(dc,
                                                   candidates[static_cast<size_t>(index)].text,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   traditional)
                               .cx +
                           MeasureTextWithFallback(dc,
                                                   candidates[static_cast<size_t>(index)].comment,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   traditional)
                               .cx) +
          text_extra_width +
          (candidates[static_cast<size_t>(index)].comment.empty() ? 0 : s(108));
      first_column_width =
          std::max(first_column_width,
                   std::clamp(text_width,
                              min_item_width,
                              std::max(min_item_width, max_normal_item_width)));
    }
    first_column_width = std::max(first_column_width,
                                  2 * item_height + compact_page_gap +
                                      2 * s(kVerticalCompactToolColumnInsetDips));

    const int first_column_height =
        layout.candidate_top +
        (std::max(1, first_column_count) - 1) * row_step +
        item_height;
    const int tool_area_top = first_column_height + s(kVerticalFooterGapDips);
    const int tool_row_top =
        tool_area_top + separator_width + s(kVerticalCompactToolColumnInsetDips);
    const int tool_rows_bottom =
        tool_row_top + 2 * item_height + compact_group_gap +
        s(kVerticalCompactToolColumnInsetDips) + separator_width;
    layout.height = tool_rows_bottom;
    const int tail_capacity_track =
        std::max(0,
                 layout.height - layout.candidate_top - item_height -
                     s(kHorizontalExpandedHeaderSeparatorInsetDips));
    const int tail_rows =
        std::clamp(tail_capacity_track / std::max(1, row_step) + 1,
                   1,
                   kMaxCompactCandidateCount);

    const int first_column_x = left;
    for (int row = 0; row < first_column_count; ++row) {
      const int top = layout.candidate_top + row * row_step;
      layout.candidate_rects.push_back(
          RECT{first_column_x, top, first_column_x + first_column_width, top + item_height});
      layout.candidate_rows.push_back(row);
      layout.candidate_columns.push_back(0);
    }

    int tail_column_width = min_tail_column_width;
    for (size_t index = static_cast<size_t>(first_column_count); index < count; ++index) {
      const int measured =
          static_cast<int>(MeasureTextWithFallback(dc,
                                                   candidates[index].text,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   traditional)
                               .cx +
                           MeasureTextWithFallback(dc,
                                                   candidates[index].comment,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   traditional)
                               .cx) +
          text_extra_width + (candidates[index].comment.empty() ? 0 : s(108));
      tail_column_width =
          std::max(tail_column_width,
                   std::clamp(measured,
                              min_tail_column_width,
                              std::max(min_tail_column_width, max_normal_item_width)));
    }

    const int first_separator_x = first_column_x + first_column_width + side_gap;
    const int tail_left = first_separator_x + separator_width + side_gap;
    const int tail_left_margin = tail_left - first_column_x - first_column_width;
    const int rail_gap = side_gap + separator_width + side_gap;
    const int tail_gap_total = tail_columns > 1 ? (tail_columns - 1) * gap_x : 0;
    const int max_tail_total =
        std::max(0,
                 max_width - tail_left - tail_gap_total - rail_gap - brand_rail_width - right);
    if (tail_columns > 0 && tail_column_width * tail_columns > max_tail_total) {
      tail_column_width = std::max(min_tail_column_width, max_tail_total / tail_columns);
    }
    const int tail_row_track = tail_capacity_track;
    auto tail_row_top = [&](int row) {
      if (row <= 0 || tail_rows <= 1) {
        return layout.candidate_top;
      }
      return layout.candidate_top + MulDiv(row, tail_row_track, tail_rows - 1);
    };
    size_t index = static_cast<size_t>(first_column_count);
    for (int column = 1; column <= tail_columns && index < count; ++column) {
      const int column_item_count =
          std::min(tail_rows, static_cast<int>(count - index));
      for (int row = 0; row < tail_rows; ++row) {
        if (row < column_item_count) {
          const int x = tail_left + (column - 1) * (tail_column_width + gap_x);
          const int top = tail_row_top(row);
          layout.candidate_rects.push_back(
              RECT{x, top, x + tail_column_width, top + item_height});
          layout.candidate_rows.push_back(row);
          layout.candidate_columns.push_back(column);
          ++index;
        }
      }
    }
    layout.expanded_columns = 1 + tail_columns;
    width = first_column_x + first_column_width + tail_left_margin +
            tail_columns * tail_column_width + tail_gap_total +
            rail_gap + brand_rail_width + right;
  } else if (horizontal) {
    // Compact horizontal layout is the tuned single-row baseline. It may grow
    // for sentence candidates and only reduces visible count when the max width
    // would otherwise force the first candidate to truncate.
    const int left = s(4);
    const int right = s(6);
    const int gap_x = s(7);
    const int item_height = s(CandidateItemHeightDips(33, candidate_font_point_size));
    const int min_item_width = s(59);
    const int max_normal_item_width = s(176);
    if (count > 1 && !candidates.empty()) {
      const int first_desired_width =
          static_cast<int>(MeasureTextWithFallback(dc,
                                                   candidates.front().text,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   traditional)
                               .cx +
                           MeasureTextWithFallback(dc,
                                                   candidates.front().comment,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   traditional)
                               .cx) +
          s(48);
      auto first_capacity_for_count = [&](size_t visible_count) {
        const int gaps = visible_count > 0 ? static_cast<int>(visible_count - 1) * gap_x : 0;
        const int available_candidate_width =
            max_width - left - right - right_gutter - gaps;
        const int tail_min_total =
            visible_count > 1 ? static_cast<int>(visible_count - 1) * min_item_width : 0;
        return available_candidate_width - tail_min_total;
      };
      while (count > 1 && first_desired_width > first_capacity_for_count(count)) {
        --count;
      }
    }

    const int available_candidate_width =
        max_width - left - right - right_gutter -
        (count > 0 ? static_cast<int>(count - 1) * gap_x : 0);
    const int first_capacity =
        count > 1
            ? std::max(min_item_width,
                       available_candidate_width -
                           static_cast<int>(count - 1) * min_item_width)
            : available_candidate_width;
    std::vector<int> item_widths;
    item_widths.reserve(count);
    for (size_t index = 0; index < count; ++index) {
      const auto text_size = MeasureTextWithFallback(dc,
                                                     candidates[index].text,
                                                     candidate_font_point_size,
                                                     dpi,
                                                     FW_NORMAL,
                                                     traditional);
      const auto comment_size = MeasureTextWithFallback(dc,
                                                        candidates[index].comment,
                                                        candidate_font_point_size,
                                                        dpi,
                                                        FW_NORMAL,
                                                        traditional);
      const int desired_width = static_cast<int>(text_size.cx + comment_size.cx) +
                                (index == 0 ? s(48) : s(43));
      const int item_max_width = index == 0 ? first_capacity : max_normal_item_width;
      item_widths.push_back(
          std::clamp(desired_width, min_item_width, std::max(min_item_width, item_max_width)));
    }
    int current_total = sum_widths(item_widths);
    if (current_total > available_candidate_width && item_widths.size() > 1) {
      std::vector<int> tail_widths(item_widths.begin() + 1, item_widths.end());
      shrink_widths(tail_widths,
                     min_item_width,
                     std::max(0, available_candidate_width - item_widths.front()));
      for (size_t index = 1; index < item_widths.size(); ++index) {
        item_widths[index] = tail_widths[index - 1];
      }
      current_total = sum_widths(item_widths);
      if (current_total > available_candidate_width) {
        item_widths.front() = std::max(
            min_item_width, item_widths.front() - (current_total - available_candidate_width));
      }
    } else {
      shrink_widths(item_widths, min_item_width, available_candidate_width);
    }
    int x = left;
    layout.candidate_rects.reserve(count);
    for (size_t index = 0; index < count; ++index) {
      RECT rect{x,
                layout.candidate_top,
                x + item_widths[index],
                layout.candidate_top + item_height};
      layout.candidate_rects.push_back(rect);
      x += item_widths[index] + gap_x;
    }
    if (count == 0) {
      x = s(140);
    }
    width = std::max(width,
                     x - gap_x + right + right_gutter + ScaleHalfDipForDpi(7, dpi));
    layout.height = s(CandidateRowStepDips(34, candidate_font_point_size));
  } else {
    const int left = s(4);
    const int right = s(6);
    const int item_height =
        s(CandidateItemHeightDips(kVerticalCandidateRowHeightDips, candidate_font_point_size));
    const int row_step =
        s(CandidateRowStepDips(kVerticalCandidateRowStepDips, candidate_font_point_size));
    const int min_item_width =
        std::max(s(kVerticalCompactMinItemWidthDips),
                 compact_vertical_tool_column_width - left - right);
    const int max_candidate_width = std::max(min_item_width, max_width - left - right);
    const int number_width =
        std::max(s(9),
                 static_cast<int>(MeasureTextWithFallback(dc,
                                                          L"9",
                                                          candidate_font_point_size,
                                                          dpi,
                                                          FW_NORMAL,
                                                          traditional)
                                      .cx));
    const int text_extra_width = s(13) + number_width + s(2) + s(12);
    int candidate_width = min_item_width;
    layout.candidate_rects.reserve(count);
    for (size_t index = 0; index < count; ++index) {
      const int top = layout.candidate_top + static_cast<int>(index) * row_step;
      const int text_width =
          static_cast<int>(MeasureTextWithFallback(dc,
                                                   candidates[index].text,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   traditional)
                               .cx +
                           MeasureTextWithFallback(dc,
                                                   candidates[index].comment,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   traditional)
                               .cx) +
          text_extra_width + (candidates[index].comment.empty() ? 0 : s(108));
      candidate_width =
          std::max(candidate_width, std::clamp(text_width, min_item_width, max_candidate_width));
      RECT rect{left, top, left + candidate_width, top + item_height};
      layout.candidate_rects.push_back(rect);
    }
    for (RECT& rect : layout.candidate_rects) {
      rect.right = rect.left + candidate_width;
    }
    width = left + candidate_width + right;
    const int visible_rows = std::max(1, static_cast<int>(count));
    const int candidate_height = layout.candidate_top + (visible_rows - 1) * row_step + item_height;
    const int compact_group_gap = s(12);
    const int tool_row_top =
        candidate_height + s(kVerticalFooterGapDips) + separator_width +
        s(kVerticalCompactToolColumnInsetDips);
    layout.height =
        tool_row_top + 2 * item_height + compact_group_gap +
        s(kVerticalCompactToolColumnInsetDips) + separator_width;
  }

  layout.width = std::min(std::max(width, min_width), max_width);
  if (expanded && horizontal && layout.width != width) {
    const int adjusted_width = layout.width;
    const int tail_left = s(4);
    const int tail_right = adjusted_width - s(6);
    const int gap_x = s(7);
    const int item_height = s(CandidateItemHeightDips(33, candidate_font_point_size));
    const int row_step = s(CandidateRowStepDips(34, candidate_font_point_size));
    const int max_tail_columns = kMaxCompactCandidateCount;
    const int min_item_width = s(59);
    const int tail_width = std::max(0, tail_right - tail_left);
    const int tail_columns =
        std::clamp(std::max(1, (tail_width + gap_x) / (min_item_width + gap_x)),
                   1,
                   max_tail_columns);
    layout.expanded_columns = tail_columns;
    const int tail_available =
        std::max(0, tail_right - tail_left - (tail_columns - 1) * gap_x);
    const int tail_cell_width =
        tail_columns > 0 ? tail_available / tail_columns : tail_available;
    const int tail_extra =
        tail_columns > 0 ? tail_available - tail_cell_width * tail_columns : 0;
    for (size_t index = 0; index < layout.candidate_rects.size(); ++index) {
      if (index >= layout.candidate_rows.size() || index >= layout.candidate_columns.size() ||
          layout.candidate_rows[index] == 0) {
        continue;
      }
      const int column = layout.candidate_columns[index];
      if (column >= tail_columns) {
        layout.candidate_rects[index] = RECT{};
        continue;
      }
      int x = tail_left;
      for (int previous = 0; previous < column; ++previous) {
        x += tail_cell_width + (previous < tail_extra ? 1 : 0) + gap_x;
      }
      const int cell_width = tail_cell_width + (column < tail_extra ? 1 : 0);
      const int top = layout.candidate_top + layout.candidate_rows[index] * row_step;
      layout.candidate_rects[index] = {x, top, x + cell_width, top + item_height};
    }
  }
  if (!horizontal && !expanded) {
    const int footer_separator_left = s(kVerticalFooterSeparatorInsetDips);
    const int footer_separator_right =
        std::max(footer_separator_left + separator_width,
                 layout.width - s(kVerticalFooterSeparatorInsetDips));
    const int vertical_item_height =
        s(CandidateItemHeightDips(kVerticalCandidateRowHeightDips, candidate_font_point_size));
    const int candidate_bottom =
        layout.candidate_rects.empty() ? layout.candidate_top + vertical_item_height
                                       : layout.candidate_rects.back().bottom;
    const int tool_area_top = candidate_bottom + s(kVerticalFooterGapDips);
    const int tool_row_top = tool_area_top + separator_width + s(kVerticalCompactToolColumnInsetDips);
    const int tool_row_left = s(kVerticalCompactToolColumnLeftDips);
    const int tool_row_right = std::max(tool_row_left + 2 * vertical_item_height + s(4),
                                        layout.width - s(kVerticalCompactToolColumnInsetDips));
    const int tool_cell_size = vertical_item_height;
    const int tool_cell_gap = s(4);
    const int tool_grid_right = tool_row_right;
    const int tool_grid_left = tool_grid_right - (2 * tool_cell_size + tool_cell_gap);
    const int tool_second_row_top = tool_row_top + tool_cell_size + s(12);
    auto make_tool_cell = [&](int column, int row) {
      const int left = tool_grid_left + column * (tool_cell_size + tool_cell_gap);
      const int top = row == 0 ? tool_row_top : tool_second_row_top;
      return RECT{left, top, left + tool_cell_size, top + tool_cell_size};
    };
    auto make_icon_rect = [&](RECT row_rect) {
      const int left = static_cast<int>(row_rect.left) +
                       std::max(0,
                                (static_cast<int>(row_rect.right - row_rect.left) -
                                 tool_button) /
                                    2);
      const int top = static_cast<int>(row_rect.top) +
                      std::max(0,
                               (static_cast<int>(row_rect.bottom - row_rect.top) -
                                tool_button) /
                                   2);
      return RECT{left, top, left + tool_button, top + tool_button};
    };
    layout.previous_page_rect = make_tool_cell(0, 0);
    layout.next_page_rect = make_tool_cell(1, 0);
    layout.emoji_rect = make_tool_cell(0, 1);
    layout.expand_rect = make_tool_cell(1, 1);
    layout.previous_page_icon_rect = make_icon_rect(layout.previous_page_rect);
    layout.next_page_icon_rect = make_icon_rect(layout.next_page_rect);
    layout.emoji_icon_rect = make_icon_rect(layout.emoji_rect);
    layout.expand_icon_rect = make_icon_rect(layout.expand_rect);
    layout.tool_separator_rect = {tool_row_left,
                                  tool_area_top,
                                  tool_row_right,
                                  tool_area_top + separator_width};
    layout.tool_row_separator_rects.reserve(2);
    const int middle_separator_y = tool_row_top + tool_cell_size + s(6);
    layout.tool_row_separator_rects.push_back(
        RECT{tool_row_left, middle_separator_y, tool_row_right, middle_separator_y + separator_width});
    const int bottom_separator_y =
        tool_second_row_top + tool_cell_size + s(kVerticalCompactToolColumnInsetDips);
    layout.tool_row_separator_rects.push_back(
        RECT{tool_row_left, bottom_separator_y, tool_row_right, bottom_separator_y + separator_width});
  } else if (!horizontal && expanded) {
    const int vertical_item_height =
        s(CandidateItemHeightDips(kVerticalCandidateRowHeightDips, candidate_font_point_size));
    const int first_column_left = s(4);
    int first_column_right = first_column_left + s(kVerticalCompactMinItemWidthDips);
    int first_column_bottom = layout.candidate_top + vertical_item_height;
    for (size_t index = 0; index < layout.candidate_rects.size(); ++index) {
      if (index < layout.candidate_columns.size() && layout.candidate_columns[index] == 0) {
        first_column_right =
            std::max(first_column_right, static_cast<int>(layout.candidate_rects[index].right));
        first_column_bottom =
            std::max(first_column_bottom, static_cast<int>(layout.candidate_rects[index].bottom));
      }
    }

    const int vertical_gap = s(kHorizontalExpandedFooterGapDips);
    const int header_separator_top = s(kHorizontalExpandedHeaderSeparatorInsetDips);
    const int header_separator_bottom =
        std::max(header_separator_top + separator_width,
                 layout.height - s(kHorizontalExpandedHeaderSeparatorInsetDips));
    const int first_separator_x = first_column_right + vertical_gap;
    layout.expanded_header_separator_rect = {first_separator_x,
                                             header_separator_top,
                                             first_separator_x + separator_width,
                                             header_separator_bottom};

    const int rail_width = vertical_brand_rail_width;
    const int rail_right =
        std::max(s(kHorizontalExpandedSettingsBottomInsetDips) + tool_button,
                 layout.width - s(kHorizontalExpandedSettingsBottomInsetDips));
    const int rail_left =
        std::max(rail_right - rail_width,
                 static_cast<int>(layout.expanded_header_separator_rect.right));
    const int footer_separator_x =
        std::max(static_cast<int>(layout.expanded_header_separator_rect.right),
                 rail_left - vertical_gap - separator_width);
    const int footer_separator_top = s(kHorizontalExpandedFooterSeparatorInsetDips);
    const int footer_separator_bottom =
        std::max(footer_separator_top + separator_width,
                 layout.height - s(kHorizontalExpandedFooterSeparatorInsetDips));
    layout.expanded_footer_separator_rect = {footer_separator_x,
                                             footer_separator_top,
                                             footer_separator_x + separator_width,
                                             footer_separator_bottom};

    const int tool_area_top = first_column_bottom + s(kVerticalFooterGapDips);
    const int tool_row_top =
        tool_area_top + separator_width + s(kVerticalCompactToolColumnInsetDips);
    const int tool_row_left = s(kVerticalCompactToolColumnLeftDips);
    const int tool_row_right =
        std::max(tool_row_left + 2 * vertical_item_height + s(4),
                 first_column_right - s(kVerticalCompactToolColumnInsetDips));
    const int tool_cell_size = vertical_item_height;
    const int tool_cell_gap = s(4);
    const int tool_grid_right = tool_row_right;
    const int tool_grid_left = tool_grid_right - (2 * tool_cell_size + tool_cell_gap);
    const int tool_second_row_top = tool_row_top + tool_cell_size + s(12);
    auto make_tool_cell = [&](int column, int row) {
      const int left = tool_grid_left + column * (tool_cell_size + tool_cell_gap);
      const int top = row == 0 ? tool_row_top : tool_second_row_top;
      return RECT{left, top, left + tool_cell_size, top + tool_cell_size};
    };
    auto make_icon_rect = [&](RECT row_rect) {
      const int left = static_cast<int>(row_rect.left) +
                       std::max(0,
                                (static_cast<int>(row_rect.right - row_rect.left) -
                                 tool_button) /
                                    2);
      const int top = static_cast<int>(row_rect.top) +
                      std::max(0,
                               (static_cast<int>(row_rect.bottom - row_rect.top) -
                                tool_button) /
                                   2);
      return RECT{left, top, left + tool_button, top + tool_button};
    };
    layout.previous_page_rect = make_tool_cell(0, 0);
    layout.next_page_rect = make_tool_cell(1, 0);
    layout.emoji_rect = make_tool_cell(0, 1);
    layout.expand_rect = make_tool_cell(1, 1);
    layout.previous_page_icon_rect = make_icon_rect(layout.previous_page_rect);
    layout.next_page_icon_rect = make_icon_rect(layout.next_page_rect);
    layout.emoji_icon_rect = make_icon_rect(layout.emoji_rect);
    layout.expand_icon_rect = make_icon_rect(layout.expand_rect);
    layout.tool_separator_rect = {tool_row_left,
                                  tool_area_top,
                                  tool_row_right,
                                  tool_area_top + separator_width};
    layout.tool_row_separator_rects.reserve(2);
    const int middle_separator_y = tool_row_top + tool_cell_size + s(6);
    layout.tool_row_separator_rects.push_back(
        RECT{tool_row_left, middle_separator_y, tool_row_right, middle_separator_y + separator_width});
    const int bottom_separator_y =
        tool_second_row_top + tool_cell_size + s(kVerticalCompactToolColumnInsetDips);
    layout.tool_row_separator_rects.push_back(
        RECT{tool_row_left, bottom_separator_y, tool_row_right, bottom_separator_y + separator_width});

    const int settings_bottom =
        std::max(tool_button, layout.height - s(kHorizontalExpandedSettingsBottomInsetDips));
    layout.settings_rect = {rail_right - tool_button,
                            settings_bottom - tool_button,
                            rail_right,
                            settings_bottom};

    const int brand_top = s(kHorizontalExpandedBrandLeftInsetDips);
    const int brand_icon_left =
        rail_left + std::max(0, (rail_right - rail_left - brand_icon_size) / 2);
    layout.brand_icon_rect = {brand_icon_left,
                              brand_top,
                              brand_icon_left + brand_icon_size,
                              brand_top + brand_icon_size};
    const int brand_text_top = layout.brand_icon_rect.bottom + s(kHorizontalExpandedBrandTextGapDips);
    const std::wstring brand_text = L"\u6D41\u7545\u62FC\u97F3";
    const SIZE brand_text_size =
        MeasureTextWithFallback(
            dc, brand_text, kHorizontalExpandedBrandTextPointSize, dpi, FW_NORMAL, traditional);
    const int brand_text_height =
        std::max(static_cast<int>(brand_text_size.cy) * static_cast<int>(brand_text.size()),
                 s(48));
    const int brand_text_limit =
        layout.settings_rect.top - s(kHorizontalExpandedBrandSettingsGapDips);
    const int brand_text_bottom =
        std::min(brand_text_limit, brand_text_top + brand_text_height);
    layout.brand_text_rect = {rail_left, brand_text_top, rail_right, brand_text_bottom};
    if (layout.brand_text_rect.bottom <= layout.brand_text_rect.top) {
      layout.brand_text_rect.bottom = layout.brand_text_rect.top;
    }
    layout.brand_rect = {rail_left,
                         brand_top,
                         rail_right,
                         std::max(layout.brand_icon_rect.bottom, layout.brand_text_rect.bottom)};
  } else {
    const int tool_right = std::max(s(8), layout.width - tool_right_margin);
    const int horizontal_tool_row_height =
        s(CandidateRowStepDips(34, candidate_font_point_size));
    const int tool_top =
        std::max(s(6), (horizontal_tool_row_height - tool_button) / 2);
    const int compact_page_gap = s(4);
    const int compact_group_gap = s(12);
    layout.expand_rect = {tool_right - tool_button,
                          tool_top,
                          tool_right,
                          tool_top + tool_button};
    layout.emoji_rect = {layout.expand_rect.left - compact_group_gap - tool_button,
                         tool_top,
                         layout.expand_rect.left - compact_group_gap,
                         tool_top + tool_button};
    layout.next_page_rect = {layout.emoji_rect.left - compact_group_gap - tool_button,
                             tool_top,
                             layout.emoji_rect.left - compact_group_gap,
                             tool_top + tool_button};
    layout.previous_page_rect = {layout.next_page_rect.left - compact_page_gap - tool_button,
                                 tool_top,
                                 layout.next_page_rect.left - compact_page_gap,
                                 tool_top + tool_button};
    const int separator_bottom_y =
        expanded ? std::max(separator_top + s(1), horizontal_tool_row_height - separator_bottom)
                 : std::max(separator_top + s(1), layout.height - separator_bottom);
    const int compact_page_separator_offset = tool_separator_offset - ScaleHalfDipForDpi(2, dpi);
    const int compact_emoji_separator_offset = tool_separator_offset - ScaleHalfDipForDpi(3, dpi);
    layout.tool_separator_rect = {layout.previous_page_rect.left - compact_page_separator_offset,
                                   separator_top,
                                   layout.previous_page_rect.left - compact_page_separator_offset +
                                       separator_width,
                                   separator_bottom_y};
    layout.emoji_separator_rect = {layout.emoji_rect.left - compact_emoji_separator_offset,
                                   separator_top,
                                   layout.emoji_rect.left - compact_emoji_separator_offset +
                                       separator_width,
                                   separator_bottom_y};
    layout.expand_separator_rect = {layout.expand_rect.left - tool_separator_offset,
                                     separator_top,
                                     layout.expand_rect.left - tool_separator_offset + separator_width,
                                     separator_bottom_y};
    if (expanded) {
      const int expanded_row_step = s(CandidateRowStepDips(34, candidate_font_point_size));
      const int header_separator_left = s(kHorizontalExpandedHeaderSeparatorInsetDips);
      const int header_separator_right =
          std::max(header_separator_left + separator_width,
                   layout.width - s(kHorizontalExpandedHeaderSeparatorInsetDips));
      const int footer_separator_left = s(kHorizontalExpandedFooterSeparatorInsetDips);
      const int footer_separator_right =
          std::max(footer_separator_left + separator_width,
                   layout.width - s(kHorizontalExpandedFooterSeparatorInsetDips));
      const int first_row_separator_y = expanded_row_step;
      const int expanded_content_bottom =
          expanded_row_step * (1 + kHorizontalExpandedTailRows);
      const int footer_top = expanded_content_bottom + s(kHorizontalExpandedFooterGapDips);
      layout.expanded_header_separator_rect = {header_separator_left,
                                               first_row_separator_y,
                                               header_separator_right,
                                               first_row_separator_y + separator_width};
      layout.expanded_footer_separator_rect = {footer_separator_left,
                                               footer_top,
                                               footer_separator_right,
                                               footer_top + separator_width};
      const int settings_right = std::max(s(8), layout.width - s(8));
      const int settings_bottom =
          std::max(footer_top + tool_button,
                   layout.height - s(kHorizontalExpandedSettingsBottomInsetDips));
      layout.settings_rect = {settings_right - tool_button,
                              settings_bottom - tool_button,
                              settings_right,
                              settings_bottom};
      const int footer_bottom = layout.height - s(1);
      const int brand_icon_side = brand_icon_size;
      const int brand_icon_top =
          footer_top + std::max(0, (footer_bottom - footer_top - brand_icon_side) / 2);
      const int brand_icon_left = s(kHorizontalExpandedBrandLeftInsetDips);
      layout.brand_icon_rect = {brand_icon_left,
                                brand_icon_top,
                                brand_icon_left + brand_icon_side,
                                brand_icon_top + brand_icon_side};
      const std::wstring brand_text = L"\u6D41\u7545\u62FC\u97F3";
      const SIZE brand_text_size =
          MeasureTextWithFallback(
              dc, brand_text, kHorizontalExpandedBrandTextPointSize, dpi, FW_NORMAL, traditional);
      const int brand_text_left = layout.brand_icon_rect.right + s(kHorizontalExpandedBrandTextGapDips);
      const int brand_text_right =
          std::min(settings_right - tool_button - s(kHorizontalExpandedBrandSettingsGapDips),
                   brand_text_left + static_cast<int>(brand_text_size.cx) +
                       s(kHorizontalExpandedBrandRightPaddingDips));
      const int brand_text_height = static_cast<int>(brand_text_size.cy);
      const int brand_text_top =
          footer_top + std::max(0, (footer_bottom - footer_top - brand_text_height) / 2);
      layout.brand_text_rect = {brand_text_left,
                                brand_text_top,
                                brand_text_right,
                                brand_text_top + brand_text_height};
      layout.brand_rect = {layout.brand_icon_rect.left,
                           std::min(layout.brand_icon_rect.top, layout.brand_text_rect.top),
                           std::max(layout.brand_icon_rect.right, layout.brand_text_rect.right),
                           std::max(layout.brand_icon_rect.bottom, layout.brand_text_rect.bottom)};
    }
  }
  const int candidate_right =
      horizontal
          ? std::max<LONG>(s(72), layout.tool_separator_rect.left - s(kCandidateToolCandidateGap))
          : layout.width - s(10);
  for (size_t index = 0; index < layout.candidate_rects.size(); ++index) {
    auto& rect = layout.candidate_rects[index];
    const bool compact_tool_row_candidate =
        horizontal && (!expanded ||
                       index >= layout.candidate_rows.size() ||
                       layout.candidate_rows[index] == 0);
    if (compact_tool_row_candidate && rect.left >= candidate_right) {
      rect.right = rect.left;
    } else if (compact_tool_row_candidate && rect.right > candidate_right) {
      rect.right = candidate_right;
    }
  }
  return layout;
}

bool IsSelectableCandidateRect(const RECT& rect) {
  return rect.right > rect.left && rect.bottom > rect.top;
}

int CandidatePageSizeLimit(bool horizontal, bool expanded, int compact_count);

std::vector<size_t> SelectableCandidateIndices(const CandidateLayoutMetrics& layout,
                                               size_t candidate_count) {
  std::vector<size_t> indices;
  const size_t count = std::min(candidate_count, layout.candidate_rects.size());
  indices.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    if (IsSelectableCandidateRect(layout.candidate_rects[index])) {
      indices.push_back(index);
    }
  }
  return indices;
}

std::vector<size_t> ContiguousCandidateIndices(size_t candidate_count, size_t limit) {
  const size_t count = std::min(candidate_count, limit);
  std::vector<size_t> indices;
  indices.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    indices.push_back(index);
  }
  return indices;
}

CandidateLayoutMetrics CandidateLayoutForWindow(
    HWND window,
    bool horizontal,
    bool expanded,
    int compact_count,
    int candidate_font_point_size,
    bool simplified_charset,
    CandidateFontFamily candidate_font_family,
    const std::vector<fp::core::RimeCandidateView>& candidates,
    UINT* dpi_out = nullptr) {
  if (dpi_out != nullptr) {
    *dpi_out = window != nullptr ? ReadableDpiForWindow(window) : 96;
  }
  if (window == nullptr) {
    return {};
  }

  const UINT dpi = ReadableDpiForWindow(window);
  if (dpi_out != nullptr) {
    *dpi_out = dpi;
  }
  const size_t candidate_hash = HashCandidateLayoutInputs(candidates);
  RECT cache_work_area = WorkAreaForPoint(POINT{0, 0});
  RECT window_rect{};
  if (GetWindowRect(window, &window_rect)) {
    const POINT window_center{window_rect.left + (window_rect.right - window_rect.left) / 2,
                              window_rect.top + (window_rect.bottom - window_rect.top) / 2};
    cache_work_area = WorkAreaForPoint(window_center);
  }
  if (g_candidate_layout_cache.valid &&
      g_candidate_layout_cache.window == window &&
      g_candidate_layout_cache.dpi == dpi &&
      g_candidate_layout_cache.horizontal == horizontal &&
      g_candidate_layout_cache.expanded == expanded &&
      g_candidate_layout_cache.compact_count == compact_count &&
      g_candidate_layout_cache.candidate_font_point_size == candidate_font_point_size &&
      g_candidate_layout_cache.candidate_font_family == candidate_font_family &&
      g_candidate_layout_cache.traditional == !simplified_charset &&
      g_candidate_layout_cache.candidate_count == candidates.size() &&
      g_candidate_layout_cache.candidate_hash == candidate_hash &&
      EqualRect(&g_candidate_layout_cache.work_area, &cache_work_area)) {
    return g_candidate_layout_cache.layout;
  }

  HWND dc_window = window;
  HDC dc = GetDC(window);
  if (dc == nullptr) {
    dc_window = nullptr;
    dc = GetDC(nullptr);
  }
  if (dc == nullptr) {
    return {};
  }

  HFONT font = CreateUiFontForDpi(
      candidate_font_point_size,
      dpi,
      FW_NORMAL,
      CandidateUiFontFamily(candidate_font_family, !simplified_charset));
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  CandidateLayoutMetrics layout = CalculateCandidateLayout(window,
                                                           dc,
                                                           dpi,
                                                           horizontal,
                                                           expanded,
                                                           compact_count,
                                                           candidate_font_point_size,
                                                           !simplified_charset,
                                                           candidates);
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
  ReleaseDC(dc_window, dc);

  g_candidate_layout_cache.valid = true;
  g_candidate_layout_cache.window = window;
  g_candidate_layout_cache.dpi = dpi;
  g_candidate_layout_cache.horizontal = horizontal;
  g_candidate_layout_cache.expanded = expanded;
  g_candidate_layout_cache.compact_count = compact_count;
  g_candidate_layout_cache.candidate_font_point_size = candidate_font_point_size;
  g_candidate_layout_cache.candidate_font_family = candidate_font_family;
  g_candidate_layout_cache.traditional = !simplified_charset;
  g_candidate_layout_cache.candidate_count = candidates.size();
  g_candidate_layout_cache.candidate_hash = candidate_hash;
  g_candidate_layout_cache.work_area = cache_work_area;
  g_candidate_layout_cache.layout = layout;
  return layout;
}

CandidateLayoutMetrics CandidateLayoutForWindowAtDpi(
    HWND window,
    UINT dpi,
    const RECT& work_area,
    bool horizontal,
    bool expanded,
    int compact_count,
    int candidate_font_point_size,
    bool simplified_charset,
    CandidateFontFamily candidate_font_family,
    const std::vector<fp::core::RimeCandidateView>& candidates,
    UINT* dpi_out = nullptr) {
  dpi = ReadableDpi(dpi);
  if (dpi_out != nullptr) {
    *dpi_out = dpi;
  }
  if (window == nullptr) {
    return {};
  }

  const size_t candidate_hash = HashCandidateLayoutInputs(candidates);
  if (g_candidate_layout_cache.valid &&
      g_candidate_layout_cache.window == window &&
      g_candidate_layout_cache.dpi == dpi &&
      g_candidate_layout_cache.horizontal == horizontal &&
      g_candidate_layout_cache.expanded == expanded &&
      g_candidate_layout_cache.compact_count == compact_count &&
      g_candidate_layout_cache.candidate_font_point_size == candidate_font_point_size &&
      g_candidate_layout_cache.candidate_font_family == candidate_font_family &&
      g_candidate_layout_cache.traditional == !simplified_charset &&
      g_candidate_layout_cache.candidate_count == candidates.size() &&
      g_candidate_layout_cache.candidate_hash == candidate_hash &&
      EqualRect(&g_candidate_layout_cache.work_area, &work_area)) {
    return g_candidate_layout_cache.layout;
  }

  HWND dc_window = window;
  HDC dc = GetDC(window);
  if (dc == nullptr) {
    dc_window = nullptr;
    dc = GetDC(nullptr);
  }
  if (dc == nullptr) {
    return {};
  }

  HFONT font = CreateUiFontForDpi(
      candidate_font_point_size,
      dpi,
      FW_NORMAL,
      CandidateUiFontFamily(candidate_font_family, !simplified_charset));
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  CandidateLayoutMetrics layout = CalculateCandidateLayout(window,
                                                           dc,
                                                           dpi,
                                                           horizontal,
                                                           expanded,
                                                           compact_count,
                                                           candidate_font_point_size,
                                                           !simplified_charset,
                                                           candidates,
                                                           &work_area);
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
  ReleaseDC(dc_window, dc);

  g_candidate_layout_cache.valid = true;
  g_candidate_layout_cache.window = window;
  g_candidate_layout_cache.dpi = dpi;
  g_candidate_layout_cache.horizontal = horizontal;
  g_candidate_layout_cache.expanded = expanded;
  g_candidate_layout_cache.compact_count = compact_count;
  g_candidate_layout_cache.candidate_font_point_size = candidate_font_point_size;
  g_candidate_layout_cache.candidate_font_family = candidate_font_family;
  g_candidate_layout_cache.traditional = !simplified_charset;
  g_candidate_layout_cache.candidate_count = candidates.size();
  g_candidate_layout_cache.candidate_hash = candidate_hash;
  g_candidate_layout_cache.work_area = work_area;
  g_candidate_layout_cache.layout = layout;
  return layout;
}

std::vector<size_t> SelectableCandidateIndicesForWindow(
    HWND window,
    bool horizontal,
    bool expanded,
    int compact_count,
    int candidate_font_point_size,
    bool simplified_charset,
    CandidateFontFamily candidate_font_family,
    const std::vector<fp::core::RimeCandidateView>& candidates,
    CandidateLayoutMetrics* layout_out = nullptr) {
  if (candidates.empty()) {
    if (layout_out != nullptr) {
      *layout_out = CandidateLayoutMetrics{};
    }
    return {};
  }

  if (window == nullptr) {
    if (layout_out != nullptr) {
      *layout_out = CandidateLayoutMetrics{};
    }
    return ContiguousCandidateIndices(
        candidates.size(),
        static_cast<size_t>(CandidatePageSizeLimit(horizontal, expanded, compact_count)));
  }

  CandidateLayoutMetrics layout = CandidateLayoutForWindow(window,
                                                           horizontal,
                                                           expanded,
                                                           compact_count,
                                                           candidate_font_point_size,
                                                           simplified_charset,
                                                           candidate_font_family,
                                                           candidates);
  if (layout.candidate_rects.empty()) {
    if (layout_out != nullptr) {
      *layout_out = CandidateLayoutMetrics{};
    }
    return ContiguousCandidateIndices(
        candidates.size(),
        static_cast<size_t>(CandidatePageSizeLimit(horizontal, expanded, compact_count)));
  }
  if (layout_out != nullptr) {
    *layout_out = layout;
  }
  return SelectableCandidateIndices(layout, candidates.size());
}

int CandidatePageSizeLimit(bool horizontal, bool expanded, int compact_count) {
  return expanded ? ExpandedCandidatePageSize(horizontal, compact_count)
                  : CompactCandidateCount(compact_count);
}

int CandidatePageSizeLimit(bool expanded, int compact_count) {
  return CandidatePageSizeLimit(true, expanded, compact_count);
}

size_t CalculateVisibleCandidateCountForWindow(
    HWND window,
    bool horizontal,
    bool expanded,
    int compact_count,
    int candidate_font_point_size,
    bool simplified_charset,
    CandidateFontFamily candidate_font_family,
    const std::vector<fp::core::RimeCandidateView>& candidates) {
  CandidateLayoutMetrics layout;
  const auto selectable_indices = SelectableCandidateIndicesForWindow(window,
                                                                      horizontal,
                                                                      expanded,
                                                                      compact_count,
                                                                      candidate_font_point_size,
                                                                      simplified_charset,
                                                                      candidate_font_family,
                                                                      candidates,
                                                                      &layout);
  return selectable_indices.size();
}

enum class TrayInputIconMode {
  kChinese,
  kEnglish,
  kDisabled,
};

constexpr int kTrayIconReferenceSize = 36;
constexpr int kTrayStatusGlyphWidthUnits = 29;
constexpr int kTrayStatusGlyphHeightUnits = 33;
constexpr int kTrayDisabledCircleSideUnits = kTrayStatusGlyphHeightUnits;
constexpr int kTrayDisabledMarkSideUnits = 16;

unsigned char TrayIconSourceAlpha(const unsigned char* pixel) {
  return std::max(pixel[0], std::max(pixel[1], pixel[2]));
}

unsigned char TrayIconSampleAlpha(const std::vector<unsigned char>& alpha,
                                  int width,
                                  int height,
                                  float x,
                                  float y) {
  if (width <= 0 || height <= 0) {
    return 0;
  }
  x = std::clamp(x, 0.0f, static_cast<float>(width - 1));
  y = std::clamp(y, 0.0f, static_cast<float>(height - 1));
  const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, width - 1);
  const int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, height - 1);
  const int x1 = std::min(x0 + 1, width - 1);
  const int y1 = std::min(y0 + 1, height - 1);
  const float fx = x - static_cast<float>(x0);
  const float fy = y - static_cast<float>(y0);
  const auto at = [&](int sx, int sy) -> float {
    return static_cast<float>(alpha[static_cast<size_t>(sy) * width + sx]);
  };
  const float top = at(x0, y0) * (1.0f - fx) + at(x1, y0) * fx;
  const float bottom = at(x0, y1) * (1.0f - fx) + at(x1, y1) * fx;
  return static_cast<unsigned char>(
      std::clamp(std::lround(top * (1.0f - fy) + bottom * fy), 0L, 255L));
}

void DownsampleTrayIcon(const std::vector<unsigned char>& high_res,
                        int width,
                        int height,
                        int scale,
                        unsigned char* pixels) {
  const int render_width = width * scale;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int alpha_sum = 0;
      int red_sum = 0;
      int green_sum = 0;
      int blue_sum = 0;
      for (int sy = 0; sy < scale; ++sy) {
        for (int sx = 0; sx < scale; ++sx) {
          const size_t offset =
              (static_cast<size_t>(y * scale + sy) * render_width + (x * scale + sx)) * 4;
          const int alpha = high_res[offset + 3];
          alpha_sum += alpha;
          blue_sum += high_res[offset + 0] * alpha;
          green_sum += high_res[offset + 1] * alpha;
          red_sum += high_res[offset + 2] * alpha;
        }
      }
      const int sample_count = scale * scale;
      const int alpha = std::clamp((alpha_sum + sample_count / 2) / sample_count, 0, 255);
      unsigned char* pixel = pixels + (static_cast<size_t>(y) * width + x) * 4;
      if (alpha_sum > 0) {
        pixel[0] = static_cast<unsigned char>(std::clamp(blue_sum / alpha_sum, 0, 255));
        pixel[1] = static_cast<unsigned char>(std::clamp(green_sum / alpha_sum, 0, 255));
        pixel[2] = static_cast<unsigned char>(std::clamp(red_sum / alpha_sum, 0, 255));
        pixel[3] = static_cast<unsigned char>(alpha);
      } else {
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
        pixel[3] = 0;
      }
    }
  }
}

int TrayIconUnitsToPixels(int units, int dimension) {
  return std::clamp(MulDiv(units, dimension, kTrayIconReferenceSize), 1, dimension);
}

RECT TrayIconTargetRectHr(int units_width,
                          int units_height,
                          int render_width,
                          int render_height) {
  const int target_width =
      TrayIconUnitsToPixels(units_width, render_width);
  const int target_height =
      TrayIconUnitsToPixels(units_height, render_height);
  const int target_left = std::max(0, (render_width - target_width) / 2);
  const int target_top = std::max(0, (render_height - target_height) / 2);
  return RECT{target_left,
              target_top,
              std::min(render_width, target_left + target_width),
              std::min(render_height, target_top + target_height)};
}

bool IsSongtiFace(std::wstring_view face) {
  return TextFaceMatchesFamilyName(face, L"SimSun") ||
         TextFaceMatchesFamilyName(face, L"NSimSun") ||
         TextFaceMatchesFamilyName(face, L"宋体") ||
         TextFaceMatchesFamilyName(face, L"新宋体");
}

HFONT CreateTrayStatusFontForPixels(int pixel_height, const wchar_t* family) {
  return CreateFontW(pixel_height,
                     0,
                     0,
                     0,
                     FW_NORMAL,
                     FALSE,
                     FALSE,
                     FALSE,
                     DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS,
                     ANTIALIASED_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE,
                     family);
}

HFONT SelectTrayStatusFont(HDC dc, int pixel_height, const wchar_t* text, HGDIOBJ* old_font) {
  if (old_font == nullptr) {
    return nullptr;
  }
  *old_font = nullptr;
  EnsureUiFontsLoaded();

  constexpr std::array<const wchar_t*, 3> kFamilies{
      L"MiSans",
      L"Microsoft YaHei UI",
      L"Microsoft YaHei",
  };
  for (const wchar_t* family : kFamilies) {
    HFONT font = CreateTrayStatusFontForPixels(pixel_height, family);
    if (font == nullptr) {
      continue;
    }
    HGDIOBJ selected_old_font = SelectObject(dc, font);
    const std::wstring face = CurrentTextFace(dc);
    const bool accepted =
        !IsSongtiFace(face) && FontHasText(dc, text) &&
        (TextFaceMatchesFamily(face, family) || TextFaceMatchesFamilyName(face, family));
    if (accepted) {
      *old_font = selected_old_font;
      return font;
    }
    if (selected_old_font != nullptr) {
      SelectObject(dc, selected_old_font);
    }
    DeleteObject(font);
  }

  return nullptr;
}

HICON CreateTrayInputIcon(TrayInputIconMode mode) {
  constexpr int kFallbackSize = 16;
  const int width =
      GetSystemMetrics(SM_CXSMICON) > 0 ? GetSystemMetrics(SM_CXSMICON) : kFallbackSize;
  const int height =
      GetSystemMetrics(SM_CYSMICON) > 0 ? GetSystemMetrics(SM_CYSMICON) : kFallbackSize;
  constexpr int kScale = 8;
  const int render_width = width * kScale;
  const int render_height = height * kScale;

  HDC screen_dc = GetDC(nullptr);
  if (screen_dc == nullptr) {
    return nullptr;
  }

  HDC memory_dc = CreateCompatibleDC(screen_dc);
  if (memory_dc == nullptr) {
    ReleaseDC(nullptr, screen_dc);
    return nullptr;
  }

  BITMAPINFO bitmap_info{};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = render_width;
  bitmap_info.bmiHeader.biHeight = -render_height;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP color_bitmap =
      CreateDIBSection(screen_dc, &bitmap_info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (color_bitmap == nullptr || bits == nullptr) {
    if (color_bitmap != nullptr) {
      DeleteObject(color_bitmap);
    }
    DeleteDC(memory_dc);
    ReleaseDC(nullptr, screen_dc);
    return nullptr;
  }

  std::memset(bits, 0, static_cast<size_t>(render_width) *
                           static_cast<size_t>(render_height) * 4);

  HGDIOBJ old_bitmap = SelectObject(memory_dc, color_bitmap);
  HFONT font = nullptr;
  HGDIOBJ old_font = nullptr;
  std::vector<unsigned char> high_res(static_cast<size_t>(render_width) *
                                      static_cast<size_t>(render_height) * 4);
  const COLORREF text_color = TrayTextColor();

  if (mode == TrayInputIconMode::kDisabled) {
    const unsigned char fill_blue = GetBValue(text_color);
    const unsigned char fill_green = GetGValue(text_color);
    const unsigned char fill_red = GetRValue(text_color);
    const COLORREF cross_color = TrayInverseTextColor();
    const unsigned char cross_blue = GetBValue(cross_color);
    const unsigned char cross_green = GetGValue(cross_color);
    const unsigned char cross_red = GetRValue(cross_color);
    const int circle_side = TrayIconUnitsToPixels(
        kTrayDisabledCircleSideUnits, std::min(render_width, render_height));
    const RECT circle_rect{std::max(0, (render_width - circle_side) / 2),
                           std::max(0, (render_height - circle_side) / 2),
                           std::min(render_width, (render_width + circle_side) / 2),
                           std::min(render_height, (render_height + circle_side) / 2)};
    const float center_x =
        (static_cast<float>(circle_rect.left) + static_cast<float>(circle_rect.right)) * 0.5f;
    const float center_y =
        (static_cast<float>(circle_rect.top) + static_cast<float>(circle_rect.bottom)) * 0.5f;
    const float radius =
        std::min(static_cast<float>(circle_rect.right - circle_rect.left),
                 static_cast<float>(circle_rect.bottom - circle_rect.top)) *
        0.5f;
    for (int y = 0; y < render_height; ++y) {
      for (int x = 0; x < render_width; ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        const float py = static_cast<float>(y) + 0.5f;
        const float dx = px - center_x;
        const float dy = py - center_y;
        const size_t offset = (static_cast<size_t>(y) * render_width + x) * 4;
        if (dx * dx + dy * dy <= radius * radius) {
          high_res[offset + 0] = fill_blue;
          high_res[offset + 1] = fill_green;
          high_res[offset + 2] = fill_red;
          high_res[offset + 3] = 255;
        }
      }
    }
    const int close_font_height = -MulDiv(17, render_height, 36);
    font = CreateFontW(close_font_height,
                       0,
                       0,
                       0,
                       FW_NORMAL,
                       FALSE,
                       FALSE,
                       FALSE,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       L"Segoe Fluent Icons");
    if (font == nullptr) {
      font = CreateFontW(close_font_height,
                         0,
                         0,
                         0,
                         FW_NORMAL,
                         FALSE,
                         FALSE,
                         FALSE,
                         DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS,
                         ANTIALIASED_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE,
                         L"Segoe MDL2 Assets");
    }
    if (font != nullptr) {
      old_font = SelectObject(memory_dc, font);
    }
    SetBkMode(memory_dc, TRANSPARENT);
    SetTextColor(memory_dc, RGB(255, 255, 255));
    RECT close_rect{0, 0, render_width, render_height};
    DrawTextW(memory_dc,
              L"\uE8BB",
              -1,
              &close_rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
    auto* render_pixels = static_cast<unsigned char*>(bits);
    std::vector<unsigned char> source_alpha(static_cast<size_t>(render_width) *
                                            static_cast<size_t>(render_height));
    int source_left = render_width;
    int source_top = render_height;
    int source_right = 0;
    int source_bottom = 0;
    for (int y = 0; y < render_height; ++y) {
      for (int x = 0; x < render_width; ++x) {
        const size_t pixel_offset = (static_cast<size_t>(y) * render_width + x) * 4;
        const unsigned char alpha = TrayIconSourceAlpha(render_pixels + pixel_offset);
        source_alpha[static_cast<size_t>(y) * render_width + x] = alpha;
        if (alpha > 0) {
          source_left = std::min(source_left, x);
          source_top = std::min(source_top, y);
          source_right = std::max(source_right, x + 1);
          source_bottom = std::max(source_bottom, y + 1);
        }
      }
    }
    if (source_left < source_right && source_top < source_bottom) {
      const RECT target_rect =
          TrayIconTargetRectHr(kTrayDisabledMarkSideUnits,
                               kTrayDisabledMarkSideUnits,
                               render_width,
                               render_height);
      const int target_side_hr = std::min(target_rect.right - target_rect.left,
                                          target_rect.bottom - target_rect.top);
      const float source_width = static_cast<float>(source_right - source_left);
      const float source_height = static_cast<float>(source_bottom - source_top);
      for (int y = 0; y < target_side_hr; ++y) {
        for (int x = 0; x < target_side_hr; ++x) {
          const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(target_side_hr);
          const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(target_side_hr);
          const float sx = static_cast<float>(source_left) + u * source_width;
          const float sy = static_cast<float>(source_top) + v * source_height;
          const unsigned char alpha =
              TrayIconSampleAlpha(source_alpha, render_width, render_height, sx, sy);
          if (alpha == 0) {
            continue;
          }
          const size_t offset =
              (static_cast<size_t>(target_rect.top + y) * render_width +
               (target_rect.left + x)) *
              4;
          high_res[offset + 0] = cross_blue;
          high_res[offset + 1] = cross_green;
          high_res[offset + 2] = cross_red;
          high_res[offset + 3] = std::max(high_res[offset + 3], alpha);
        }
      }
    }
  } else {
    const int font_pixel_height = -render_height;
    const wchar_t* text = mode == TrayInputIconMode::kEnglish ? L"\u82F1" : L"\u4E2D";
    font = SelectTrayStatusFont(memory_dc, font_pixel_height, text, &old_font);
    if (font == nullptr) {
      if (old_bitmap != nullptr) {
        SelectObject(memory_dc, old_bitmap);
      }
      DeleteObject(color_bitmap);
      DeleteDC(memory_dc);
      ReleaseDC(nullptr, screen_dc);
      return nullptr;
    }

    SetBkMode(memory_dc, TRANSPARENT);
    SetTextColor(memory_dc, RGB(255, 255, 255));
    RECT text_rect{-render_width,
                   -render_height,
                   render_width * 2,
                   render_height * 2};
    DrawTextW(memory_dc,
              text,
              -1,
              &text_rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);

    auto* render_pixels = static_cast<unsigned char*>(bits);
    std::vector<unsigned char> source_alpha(static_cast<size_t>(render_width) *
                                            static_cast<size_t>(render_height));
    int source_left = render_width;
    int source_top = render_height;
    int source_right = 0;
    int source_bottom = 0;
    for (int y = 0; y < render_height; ++y) {
      for (int x = 0; x < render_width; ++x) {
        const size_t pixel_offset = (static_cast<size_t>(y) * render_width + x) * 4;
        const unsigned char alpha = TrayIconSourceAlpha(render_pixels + pixel_offset);
        source_alpha[static_cast<size_t>(y) * render_width + x] = alpha;
        if (alpha > 0) {
          source_left = std::min(source_left, x);
          source_top = std::min(source_top, y);
          source_right = std::max(source_right, x + 1);
          source_bottom = std::max(source_bottom, y + 1);
        }
      }
    }

    if (source_left < source_right && source_top < source_bottom) {
      const RECT target_rect =
          TrayIconTargetRectHr(kTrayStatusGlyphWidthUnits,
                               kTrayStatusGlyphHeightUnits,
                               render_width,
                               render_height);
      const int target_width_hr = target_rect.right - target_rect.left;
      const int target_height_hr = target_rect.bottom - target_rect.top;
      const float source_width = static_cast<float>(source_right - source_left);
      const float source_height = static_cast<float>(source_bottom - source_top);
      const unsigned char blue = GetBValue(text_color);
      const unsigned char green = GetGValue(text_color);
      const unsigned char red = GetRValue(text_color);
      for (int y = 0; y < target_height_hr; ++y) {
        for (int x = 0; x < target_width_hr; ++x) {
          const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(target_width_hr);
          const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(target_height_hr);
          const float sx = static_cast<float>(source_left) + u * source_width;
          const float sy = static_cast<float>(source_top) + v * source_height;
          const unsigned char alpha =
              TrayIconSampleAlpha(source_alpha, render_width, render_height, sx, sy);
          const size_t offset =
              (static_cast<size_t>(target_rect.top + y) * render_width +
               (target_rect.left + x)) *
              4;
          high_res[offset + 0] = blue;
          high_res[offset + 1] = green;
          high_res[offset + 2] = red;
          high_res[offset + 3] = alpha;
        }
      }
    }
  }

  BITMAPINFO icon_bitmap_info{};
  icon_bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  icon_bitmap_info.bmiHeader.biWidth = width;
  icon_bitmap_info.bmiHeader.biHeight = -height;
  icon_bitmap_info.bmiHeader.biPlanes = 1;
  icon_bitmap_info.bmiHeader.biBitCount = 32;
  icon_bitmap_info.bmiHeader.biCompression = BI_RGB;

  void* icon_bits = nullptr;
  HBITMAP icon_bitmap =
      CreateDIBSection(screen_dc, &icon_bitmap_info, DIB_RGB_COLORS, &icon_bits, nullptr, 0);
  if (icon_bitmap == nullptr || icon_bits == nullptr) {
    if (old_font != nullptr) {
      SelectObject(memory_dc, old_font);
    }
    if (font != nullptr) {
      DeleteObject(font);
    }
    if (old_bitmap != nullptr) {
      SelectObject(memory_dc, old_bitmap);
    }
    DeleteObject(color_bitmap);
    DeleteDC(memory_dc);
    ReleaseDC(nullptr, screen_dc);
    return nullptr;
  }

  auto* pixels = static_cast<unsigned char*>(icon_bits);
  DownsampleTrayIcon(high_res, width, height, kScale, pixels);

  const int mask_stride = ((width + 15) / 16) * 2;
  std::vector<unsigned char> mask_bits(static_cast<size_t>(mask_stride) *
                                       static_cast<size_t>(height));
  HBITMAP mask_bitmap = CreateBitmap(width, height, 1, 1, mask_bits.data());
  ICONINFO icon_info{};
  icon_info.fIcon = TRUE;
  icon_info.hbmMask = mask_bitmap;
  icon_info.hbmColor = icon_bitmap;
  HICON icon = mask_bitmap != nullptr ? CreateIconIndirect(&icon_info) : nullptr;

  if (old_font != nullptr) {
    SelectObject(memory_dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
  if (old_bitmap != nullptr) {
    SelectObject(memory_dc, old_bitmap);
  }
  if (mask_bitmap != nullptr) {
    DeleteObject(mask_bitmap);
  }
  DeleteObject(icon_bitmap);
  DeleteObject(color_bitmap);
  DeleteDC(memory_dc);
  ReleaseDC(nullptr, screen_dc);

  return icon;
}

class InputModeLangBarItem final : public ITfLangBarItemButton, public ITfSource {
 public:
  explicit InputModeLangBarItem(TsfTextService* owner) : owner_(owner) {
    if (owner_ != nullptr) {
      owner_->AddRef();
    }
  }
  InputModeLangBarItem(const InputModeLangBarItem&) = delete;
  InputModeLangBarItem& operator=(const InputModeLangBarItem&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }

    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) ||
        IsEqualIID(riid, IID_ITfLangBarItemButton)) {
      *object = static_cast<ITfLangBarItemButton*>(this);
      AddRef();
      return S_OK;
    }
    if (IsEqualIID(riid, IID_ITfSource)) {
      *object = static_cast<ITfSource*>(this);
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }

    return count;
  }

  STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* info) override {
    if (info == nullptr) {
      return E_POINTER;
    }

    info->clsidService = kTextServiceClsid;
    info->guidItem = kInputModeLangBarItemGuid;
    info->dwStyle = TF_LBI_STYLE_SHOWNINTRAY | TF_LBI_STYLE_BTN_BUTTON |
                    TF_LBI_STYLE_BTN_MENU | TF_LBI_STYLE_TEXTCOLORICON;
    info->ulSort = 0;
    wcsncpy_s(info->szDescription, L"\u4E2D\u82F1\u5207\u6362", _TRUNCATE);
    return S_OK;
  }

  STDMETHODIMP GetStatus(DWORD* status) override {
    if (status == nullptr) {
      return E_POINTER;
    }

    *status = 0;
    return S_OK;
  }

  STDMETHODIMP Show(BOOL) override { return S_OK; }

  STDMETHODIMP GetTooltipString(BSTR* tooltip) override {
    if (tooltip == nullptr) {
      return E_POINTER;
    }

    const bool enabled = owner_ != nullptr && owner_->has_input_focus();
    *tooltip = SysAllocString(!enabled
                                  ? L"\u8F93\u5165\u6CD5\u672A\u542F\u7528\r\n\r\n\u5355\u51FB\u53F3\u952E\u4EE5\u67E5\u770B\u66F4\u591A\u9009\u9879"
                                  : owner_->ascii_mode()
                                        ? L"\u82F1\u6587\u6A21\u5F0F\r\n\r\n\u5355\u51FB\u53F3\u952E\u4EE5\u67E5\u770B\u66F4\u591A\u9009\u9879"
                                        : L"\u4E2D\u6587\u6A21\u5F0F\r\n\r\n\u5355\u51FB\u53F3\u952E\u4EE5\u67E5\u770B\u66F4\u591A\u9009\u9879");
    return *tooltip != nullptr ? S_OK : E_OUTOFMEMORY;
  }

  STDMETHODIMP OnClick(TfLBIClick click, POINT point, const RECT*) override {
    if (owner_ == nullptr) {
      return S_OK;
    }
    if (click == TF_LBI_CLK_LEFT) {
      if (owner_->has_input_focus()) {
        owner_->ToggleAsciiMode();
      } else {
        owner_->ShowContextMenu(point);
      }
    } else if (click == TF_LBI_CLK_RIGHT) {
      owner_->ShowContextMenu(point);
    }
    return S_OK;
  }

  STDMETHODIMP InitMenu(ITfMenu* menu) override {
    if (menu == nullptr) {
      return E_POINTER;
    }

    AddSubMenu(menu,
               kMenuFullShape,
               owner_ != nullptr && owner_->full_shape_mode() ? L"\u5168\u534A\u89D2(\u5168\u89D2)"
                                                              : L"\u5168\u534A\u89D2(\u534A\u89D2)",
               {{kMenuFullShapeHalf,
                 L"\u534A\u89D2",
                 owner_ == nullptr || !owner_->full_shape_mode()},
                {kMenuFullShapeFull, L"\u5168\u89D2", owner_ != nullptr && owner_->full_shape_mode()}});
    AddSubMenu(menu,
               kMenuPunctuation,
               owner_ != nullptr && owner_->chinese_punctuation_mode()
                   ? L"\u4E2D\u6587\u6807\u70B9"
                   : L"\u82F1\u6587\u6807\u70B9",
               {{kMenuPunctuationChinese,
                 L"\u4E2D\u6587\u6807\u70B9",
                 owner_ == nullptr || owner_->chinese_punctuation_mode()},
                {kMenuPunctuationEnglish,
                 L"\u82F1\u6587\u6807\u70B9",
                 owner_ != nullptr && !owner_->chinese_punctuation_mode()}});
    AddSubMenu(menu,
               kMenuCharset,
               owner_ != nullptr && !owner_->simplified_charset()
                   ? L"\u8F93\u5165\u5B57\u7B26(\u7E41\u4F53)"
                   : L"\u8F93\u5165\u5B57\u7B26(\u7B80\u4F53)",
               {{kMenuCharsetSimplified,
                 L"\u7B80\u4F53",
                 owner_ == nullptr || owner_->simplified_charset()},
                {kMenuCharsetTraditional,
                 L"\u7E41\u4F53",
                 owner_ != nullptr && !owner_->simplified_charset()}});
    AddSeparator(menu);
    AddItem(menu, kMenuEmoji, L"\u8868\u60C5\u7B26\u53F7\u548C\u7B26\u53F7");
    AddSeparator(menu);
    AddItem(menu, kMenuCustomPhrases, L"\u7528\u6237\u81EA\u5B9A\u4E49\u77ED\u8BED");
    AddItem(menu, kMenuLexiconManagement, L"\u8BCD\u5E93\u7BA1\u7406");
    AddSeparator(menu);
    AddItem(menu, kMenuKeyConfig, L"\u6309\u952E\u914D\u7F6E");
    AddSeparator(menu);
    AddItem(menu,
            kMenuToolbar,
            owner_ != nullptr && owner_->toolbar_visible()
                ? L"\u8F93\u5165\u6CD5\u5DE5\u5177\u680F(\u5F00)"
                : L"\u8F93\u5165\u6CD5\u5DE5\u5177\u680F(\u5173)",
            owner_ != nullptr && owner_->toolbar_visible());
    AddSeparator(menu);
    AddItem(menu, kMenuSettings, L"\u8BBE\u7F6E");
    AddItem(menu, kMenuRedeploy, L"\u91CD\u542F");
    return S_OK;
  }

  STDMETHODIMP OnMenuSelect(UINT id) override {
    if (owner_ != nullptr) {
      owner_->HandleLangBarMenuCommand(id);
    }
    return S_OK;
  }

  STDMETHODIMP GetIcon(HICON* icon) override {
    if (icon == nullptr) {
      return E_POINTER;
    }

    TrayInputIconMode mode = TrayInputIconMode::kDisabled;
    if (owner_ != nullptr && owner_->has_input_focus() && owner_->ascii_mode()) {
      mode = TrayInputIconMode::kEnglish;
    } else if (owner_ != nullptr && owner_->has_input_focus()) {
      mode = TrayInputIconMode::kChinese;
    }
    HICON icon_handle = CreateTrayInputIcon(mode);
    *icon = icon_handle;
    return *icon != nullptr ? S_OK : E_FAIL;
  }

  STDMETHODIMP GetText(BSTR* text) override {
    if (text == nullptr) {
      return E_POINTER;
    }

    *text = SysAllocString(L"\u7545");
    return *text != nullptr ? S_OK : E_OUTOFMEMORY;
  }

  STDMETHODIMP AdviseSink(REFIID riid, IUnknown* punk, DWORD* cookie) override {
    if (cookie == nullptr) {
      return E_POINTER;
    }
    *cookie = TF_INVALID_COOKIE;
    if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) {
      return CONNECT_E_CANNOTCONNECT;
    }
    if (punk == nullptr) {
      return E_INVALIDARG;
    }
    if (sink_ != nullptr) {
      return CONNECT_E_ADVISELIMIT;
    }

    ITfLangBarItemSink* sink = nullptr;
    const HRESULT result = punk->QueryInterface(IID_ITfLangBarItemSink,
                                                reinterpret_cast<void**>(&sink));
    if (FAILED(result) || sink == nullptr) {
      return CONNECT_E_CANNOTCONNECT;
    }

    sink_ = sink;
    sink_cookie_ = kSinkCookie;
    *cookie = sink_cookie_;
    return S_OK;
  }

  STDMETHODIMP UnadviseSink(DWORD cookie) override {
    if (cookie != sink_cookie_ || sink_ == nullptr) {
      return CONNECT_E_NOCONNECTION;
    }
    sink_->Release();
    sink_ = nullptr;
    sink_cookie_ = TF_INVALID_COOKIE;
    return S_OK;
  }

  void NotifyUpdated(DWORD flags = TF_LBI_BTNALL | TF_LBI_STATUS | TF_LBI_ICON) {
    if (sink_ != nullptr) {
      sink_->OnUpdate(flags);
    }
  }

 private:
  ~InputModeLangBarItem() {
    if (sink_ != nullptr) {
      sink_->Release();
      sink_ = nullptr;
    }
    if (owner_ != nullptr) {
      owner_->Release();
      owner_ = nullptr;
    }
  }

  struct SubMenuItem {
    UINT id;
    const wchar_t* text;
    bool checked;
  };

  void AddItem(ITfMenu* menu,
               UINT id,
               const wchar_t* text,
               bool checked = false,
               DWORD extra_flags = 0) {
    const DWORD flags = extra_flags | (checked ? TF_LBMENUF_CHECKED : 0);
    menu->AddMenuItem(id, flags, nullptr, nullptr, text, static_cast<ULONG>(wcslen(text)), nullptr);
  }

  void AddSeparator(ITfMenu* menu) {
    menu->AddMenuItem(0, TF_LBMENUF_SEPARATOR, nullptr, nullptr, nullptr, 0, nullptr);
  }

  void AddSubMenu(ITfMenu* menu,
                  UINT id,
                  const wchar_t* text,
                  std::initializer_list<SubMenuItem> items) {
    ITfMenu* submenu = nullptr;
    menu->AddMenuItem(id,
                      TF_LBMENUF_SUBMENU,
                      nullptr,
                      nullptr,
                      text,
                      static_cast<ULONG>(wcslen(text)),
                      &submenu);
    if (submenu == nullptr) {
      return;
    }
    for (const auto& item : items) {
      AddItem(submenu, item.id, item.text, item.checked, TF_LBMENUF_RADIOCHECKED);
    }
    submenu->Release();
  }

  std::atomic<unsigned long> ref_count_{1};
  TsfTextService* owner_ = nullptr;
  ITfLangBarItemSink* sink_ = nullptr;
  DWORD sink_cookie_ = TF_INVALID_COOKIE;
  static constexpr DWORD kSinkCookie = 1;
};

bool ReplaceTextBeforeSelection(ITfContext* context,
                                TfEditCookie edit_cookie,
                                LONG old_length,
                                std::wstring_view replacement) {
  if (context == nullptr || old_length < 0) {
    return false;
  }

  TF_SELECTION selection{};
  ULONG fetched = 0;
  HRESULT result =
      context->GetSelection(edit_cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
  if (FAILED(result) || fetched == 0 || selection.range == nullptr) {
    return false;
  }

  ITfRange* range = selection.range;
  bool succeeded = true;
  BOOL selection_empty = TRUE;
  if (FAILED(range->IsEmpty(edit_cookie, &selection_empty))) {
    selection_empty = TRUE;
  }

  if (old_length > 0 && selection_empty) {
    LONG shifted = 0;
    result = range->ShiftStart(edit_cookie, -old_length, &shifted, nullptr);
    if (FAILED(result)) {
      succeeded = false;
    }
  }

  if (succeeded) {
    result = range->SetText(edit_cookie,
                            0,
                            replacement.data(),
                            static_cast<LONG>(replacement.size()));
    succeeded = SUCCEEDED(result);
  }

  if (succeeded) {
    range->Collapse(edit_cookie, TF_ANCHOR_END);
    selection.style.ase = TF_AE_NONE;
    selection.style.fInterimChar = FALSE;
    result = context->SetSelection(edit_cookie, 1, &selection);
    succeeded = SUCCEEDED(result);
  }

  range->Release();
  return succeeded;
}

class ReplaceTextEditSession final : public ITfEditSession {
 public:
  ReplaceTextEditSession(ITfContext* context,
                         LONG old_length,
                         std::wstring replacement)
      : context_(context),
        old_length_(old_length),
        replacement_(std::move(replacement)) {
    if (context_ != nullptr) {
      context_->AddRef();
    }
  }

  ReplaceTextEditSession(const ReplaceTextEditSession&) = delete;
  ReplaceTextEditSession& operator=(const ReplaceTextEditSession&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }

    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
      *object = static_cast<ITfEditSession*>(this);
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    succeeded_ =
        ReplaceTextBeforeSelection(context_, edit_cookie, old_length_, replacement_);
    return succeeded_ ? S_OK : E_FAIL;
  }

  bool succeeded() const noexcept { return succeeded_; }

 private:
  ~ReplaceTextEditSession() {
    if (context_ != nullptr) {
      context_->Release();
      context_ = nullptr;
    }
  }

  std::atomic<unsigned long> ref_count_{1};
  ITfContext* context_ = nullptr;
  LONG old_length_ = 0;
  std::wstring replacement_;
  bool succeeded_ = false;
};

class DisplayAttributeInfoInput final : public ITfDisplayAttributeInfo {
 public:
  DisplayAttributeInfoInput() = default;
  DisplayAttributeInfoInput(const DisplayAttributeInfoInput&) = delete;
  DisplayAttributeInfoInput& operator=(const DisplayAttributeInfoInput&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }
    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfDisplayAttributeInfo)) {
      *object = static_cast<ITfDisplayAttributeInfo*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHODIMP GetGUID(GUID* guid) override {
    if (guid == nullptr) {
      return E_POINTER;
    }
    *guid = kDisplayAttributeInputGuid;
    return S_OK;
  }

  STDMETHODIMP GetDescription(BSTR* description) override {
    if (description == nullptr) {
      return E_POINTER;
    }
    *description = SysAllocString(L"FluentPinyin Display Attribute Input");
    return *description != nullptr ? S_OK : E_OUTOFMEMORY;
  }

  STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* attribute) override {
    if (attribute == nullptr) {
      return E_POINTER;
    }
    attribute->crText.type = TF_CT_NONE;
    attribute->crBk.type = TF_CT_NONE;
    attribute->lsStyle = TF_LS_DOT;
    attribute->fBoldLine = FALSE;
    attribute->crLine.type = TF_CT_NONE;
    attribute->bAttr = TF_ATTR_INPUT;
    return S_OK;
  }

  STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) override { return E_NOTIMPL; }

  STDMETHODIMP Reset() override { return S_OK; }

 private:
  ~DisplayAttributeInfoInput() = default;

  std::atomic<unsigned long> ref_count_{1};
};

class DisplayAttributeInfoEnumerator final : public IEnumTfDisplayAttributeInfo {
 public:
  DisplayAttributeInfoEnumerator() = default;
  DisplayAttributeInfoEnumerator(const DisplayAttributeInfoEnumerator&) = delete;
  DisplayAttributeInfoEnumerator& operator=(const DisplayAttributeInfoEnumerator&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }
    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IEnumTfDisplayAttributeInfo)) {
      *object = static_cast<IEnumTfDisplayAttributeInfo*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** enum_info) override {
    if (enum_info == nullptr) {
      return E_POINTER;
    }
    auto* clone = new (std::nothrow) DisplayAttributeInfoEnumerator();
    if (clone == nullptr) {
      *enum_info = nullptr;
      return E_OUTOFMEMORY;
    }
    clone->index_ = index_;
    *enum_info = clone;
    return S_OK;
  }

  STDMETHODIMP Next(ULONG count,
                    ITfDisplayAttributeInfo** info,
                    ULONG* fetched_count) override {
    if (fetched_count != nullptr) {
      *fetched_count = 0;
    }
    if (count == 0) {
      return S_OK;
    }
    if (info == nullptr) {
      return E_POINTER;
    }
    for (ULONG index = 0; index < count; ++index) {
      info[index] = nullptr;
    }
    if (index_ != 0) {
      return S_FALSE;
    }
    auto* attribute = new (std::nothrow) DisplayAttributeInfoInput();
    if (attribute == nullptr) {
      return E_OUTOFMEMORY;
    }
    info[0] = attribute;
    index_ = 1;
    if (fetched_count != nullptr) {
      *fetched_count = 1;
    }
    return count == 1 ? S_OK : S_FALSE;
  }

  STDMETHODIMP Reset() override {
    index_ = 0;
    return S_OK;
  }

  STDMETHODIMP Skip(ULONG count) override {
    if (count == 0) {
      return S_OK;
    }
    const bool skipped_all = index_ == 0 && count == 1;
    index_ = 1;
    return skipped_all ? S_OK : S_FALSE;
  }

 private:
  ~DisplayAttributeInfoEnumerator() = default;

  std::atomic<unsigned long> ref_count_{1};
  ULONG index_ = 0;
};

bool RequestTextReplacement(TfClientId client_id,
                            ITfContext* context,
                            LONG old_length,
                            const std::wstring& replacement) {
  auto* session = new (std::nothrow) ReplaceTextEditSession(context, old_length, replacement);
  if (session == nullptr) {
    return false;
  }

  HRESULT edit_result = E_FAIL;
  const HRESULT request_result = context->RequestEditSession(
      client_id, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
  const bool succeeded =
      SUCCEEDED(request_result) && SUCCEEDED(edit_result) && session->succeeded();
  session->Release();
  return succeeded;
}

bool SetRangeDisplayAttribute(ITfContext* context,
                              TfEditCookie edit_cookie,
                              ITfRange* range,
                              TfGuidAtom display_attribute_atom) {
  if (context == nullptr || range == nullptr ||
      display_attribute_atom == TF_INVALID_GUIDATOM) {
    return false;
  }

  ITfProperty* property = nullptr;
  HRESULT result = context->GetProperty(GUID_PROP_ATTRIBUTE, &property);
  if (FAILED(result) || property == nullptr) {
    return false;
  }

  VARIANT value{};
  VariantInit(&value);
  value.vt = VT_I4;
  value.lVal = static_cast<LONG>(display_attribute_atom);
  result = property->SetValue(edit_cookie, range, &value);
  property->Release();
  return SUCCEEDED(result);
}

void ClearRangeDisplayAttribute(ITfContext* context, TfEditCookie edit_cookie, ITfRange* range) {
  if (context == nullptr || range == nullptr) {
    return;
  }

  ITfProperty* property = nullptr;
  HRESULT result = context->GetProperty(GUID_PROP_ATTRIBUTE, &property);
  if (FAILED(result) || property == nullptr) {
    return;
  }

  property->Clear(edit_cookie, range);
  property->Release();
}

bool SetRangeText(ITfRange* range, TfEditCookie edit_cookie, std::wstring_view text) {
  if (range == nullptr) {
    return false;
  }

  const HRESULT result =
      range->SetText(edit_cookie, 0, text.data(), static_cast<LONG>(text.size()));
  return SUCCEEDED(result);
}

bool MoveSelectionToRangeEnd(ITfContext* context, TfEditCookie edit_cookie, ITfRange* range) {
  if (context == nullptr || range == nullptr) {
    return false;
  }

  ITfRange* caret_range = nullptr;
  HRESULT result = range->Clone(&caret_range);
  if (FAILED(result) || caret_range == nullptr) {
    return false;
  }

  result = caret_range->Collapse(edit_cookie, TF_ANCHOR_END);
  if (FAILED(result)) {
    caret_range->Release();
    return false;
  }

  TF_SELECTION selection{};
  selection.range = caret_range;
  selection.style.ase = TF_AE_NONE;
  selection.style.fInterimChar = FALSE;
  result = context->SetSelection(edit_cookie, 1, &selection);
  caret_range->Release();
  return SUCCEEDED(result);
}

enum class CompositionEditAction {
  kUpdate,
  kCommit,
  kCancel,
};

class CompositionEditSession final : public ITfEditSession {
 public:
  CompositionEditSession(ITfContext* context,
                         ITfCompositionSink* sink,
                         ITfComposition** composition_slot,
                         TfGuidAtom display_attribute_atom,
                         std::wstring text,
                         CompositionEditAction action)
      : context_(context),
        sink_(sink),
        composition_slot_(composition_slot),
        display_attribute_atom_(display_attribute_atom),
        text_(std::move(text)),
        action_(action) {
    if (context_ != nullptr) {
      context_->AddRef();
    }
    if (sink_ != nullptr) {
      sink_->AddRef();
    }
  }

  CompositionEditSession(const CompositionEditSession&) = delete;
  CompositionEditSession& operator=(const CompositionEditSession&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }

    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
      *object = static_cast<ITfEditSession*>(this);
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    switch (action_) {
      case CompositionEditAction::kUpdate:
        succeeded_ = Update(edit_cookie);
        break;
      case CompositionEditAction::kCommit:
        succeeded_ = Commit(edit_cookie);
        break;
      case CompositionEditAction::kCancel:
        succeeded_ = Cancel(edit_cookie);
        break;
    }
    return succeeded_ ? S_OK : E_FAIL;
  }

  bool succeeded() const noexcept { return succeeded_; }

 private:
  ~CompositionEditSession() {
    if (sink_ != nullptr) {
      sink_->Release();
      sink_ = nullptr;
    }
    if (context_ != nullptr) {
      context_->Release();
      context_ = nullptr;
    }
  }

  bool Start(TfEditCookie edit_cookie) {
    if (context_ == nullptr || sink_ == nullptr || composition_slot_ == nullptr ||
        *composition_slot_ != nullptr) {
      return false;
    }

    ITfInsertAtSelection* insert_at_selection = nullptr;
    HRESULT result = context_->QueryInterface(IID_ITfInsertAtSelection,
                                              reinterpret_cast<void**>(&insert_at_selection));
    if (FAILED(result) || insert_at_selection == nullptr) {
      return false;
    }

    ITfRange* composition_range = nullptr;
    result = insert_at_selection->InsertTextAtSelection(edit_cookie,
                                                        TF_IAS_QUERYONLY,
                                                        nullptr,
                                                        0,
                                                        &composition_range);
    insert_at_selection->Release();
    if (FAILED(result) || composition_range == nullptr) {
      return false;
    }

    ITfContextComposition* context_composition = nullptr;
    result = context_->QueryInterface(IID_ITfContextComposition,
                                      reinterpret_cast<void**>(&context_composition));
    if (FAILED(result) || context_composition == nullptr) {
      composition_range->Release();
      return false;
    }

    ITfComposition* composition = nullptr;
    result =
        context_composition->StartComposition(edit_cookie, composition_range, sink_, &composition);
    context_composition->Release();
    if (FAILED(result) || composition == nullptr) {
      composition_range->Release();
      return false;
    }
    *composition_slot_ = composition;

    bool succeeded = SetRangeText(composition_range, edit_cookie, text_);
    if (succeeded) {
      SetRangeDisplayAttribute(context_,
                               edit_cookie,
                               composition_range,
                               display_attribute_atom_);
    }
    if (succeeded) {
      MoveSelectionToRangeEnd(context_, edit_cookie, composition_range);
    }
    if (!succeeded) {
      *composition_slot_ = nullptr;
      composition->EndComposition(edit_cookie);
      composition->Release();
    }

    composition_range->Release();
    return succeeded;
  }

  bool Update(TfEditCookie edit_cookie) {
    if (composition_slot_ == nullptr) {
      return false;
    }

    if (*composition_slot_ == nullptr) {
      return Start(edit_cookie);
    }

    ITfRange* range = nullptr;
    HRESULT result = (*composition_slot_)->GetRange(&range);
    if (FAILED(result) || range == nullptr) {
      return false;
    }

    bool succeeded = SetRangeText(range, edit_cookie, text_);
    if (succeeded) {
      SetRangeDisplayAttribute(context_, edit_cookie, range, display_attribute_atom_);
    }
    if (succeeded) {
      MoveSelectionToRangeEnd(context_, edit_cookie, range);
    }
    range->Release();
    return succeeded;
  }

  bool Commit(TfEditCookie edit_cookie) {
    if (composition_slot_ == nullptr || *composition_slot_ == nullptr) {
      return false;
    }

    ITfComposition* composition = *composition_slot_;
    ITfRange* range = nullptr;
    HRESULT result = composition->GetRange(&range);
    if (FAILED(result) || range == nullptr) {
      return false;
    }

    ClearRangeDisplayAttribute(context_, edit_cookie, range);
    bool text_succeeded = SetRangeText(range, edit_cookie, text_);
    if (text_succeeded) {
      text_succeeded = MoveSelectionToRangeEnd(context_, edit_cookie, range);
    }
    range->Release();
    if (!text_succeeded) {
      return false;
    }

    *composition_slot_ = nullptr;
    composition->EndComposition(edit_cookie);
    composition->Release();
    return true;
  }

  bool Cancel(TfEditCookie edit_cookie) {
    text_.clear();
    return Commit(edit_cookie);
  }

  std::atomic<unsigned long> ref_count_{1};
  ITfContext* context_ = nullptr;
  ITfCompositionSink* sink_ = nullptr;
  ITfComposition** composition_slot_ = nullptr;
  TfGuidAtom display_attribute_atom_ = TF_INVALID_GUIDATOM;
  std::wstring text_;
  CompositionEditAction action_;
  bool succeeded_ = false;
};

bool RequestCompositionEdit(TfClientId client_id,
                            ITfContext* context,
                            ITfCompositionSink* sink,
                            ITfComposition** composition_slot,
                            TfGuidAtom display_attribute_atom,
                            const std::wstring& text,
                            CompositionEditAction action) {
  if (context == nullptr || sink == nullptr || composition_slot == nullptr) {
    return false;
  }

  auto* session =
      new (std::nothrow) CompositionEditSession(context,
                                                sink,
                                                composition_slot,
                                                display_attribute_atom,
                                                text,
                                                action);
  if (session == nullptr) {
    return false;
  }

  HRESULT edit_result = E_FAIL;
  const HRESULT request_result = context->RequestEditSession(
      client_id, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
  const bool succeeded =
      SUCCEEDED(request_result) && SUCCEEDED(edit_result) && session->succeeded();
  session->Release();
  return succeeded;
}

class CommitAndRestartCompositionEditSession final : public ITfEditSession {
 public:
  CommitAndRestartCompositionEditSession(ITfContext* context,
                                         ITfCompositionSink* sink,
                                         ITfComposition** composition_slot,
                                         TfGuidAtom display_attribute_atom,
                                         std::wstring commit_text,
                                         std::wstring preedit_text)
      : context_(context),
        sink_(sink),
        composition_slot_(composition_slot),
        display_attribute_atom_(display_attribute_atom),
        commit_text_(std::move(commit_text)),
        preedit_text_(std::move(preedit_text)) {
    if (context_ != nullptr) {
      context_->AddRef();
    }
    if (sink_ != nullptr) {
      sink_->AddRef();
    }
  }

  CommitAndRestartCompositionEditSession(const CommitAndRestartCompositionEditSession&) = delete;
  CommitAndRestartCompositionEditSession& operator=(
      const CommitAndRestartCompositionEditSession&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }

    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
      *object = static_cast<ITfEditSession*>(this);
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    succeeded_ = CommitAndRestart(edit_cookie);
    return succeeded_ ? S_OK : E_FAIL;
  }

  bool succeeded() const noexcept { return succeeded_; }
  bool committed() const noexcept { return committed_; }

 private:
  ~CommitAndRestartCompositionEditSession() {
    if (sink_ != nullptr) {
      sink_->Release();
      sink_ = nullptr;
    }
    if (context_ != nullptr) {
      context_->Release();
      context_ = nullptr;
    }
  }

  bool StartPreedit(TfEditCookie edit_cookie) {
    if (context_ == nullptr || sink_ == nullptr || composition_slot_ == nullptr ||
        *composition_slot_ != nullptr) {
      return false;
    }

    ITfInsertAtSelection* insert_at_selection = nullptr;
    HRESULT result = context_->QueryInterface(IID_ITfInsertAtSelection,
                                              reinterpret_cast<void**>(&insert_at_selection));
    if (FAILED(result) || insert_at_selection == nullptr) {
      return false;
    }

    ITfRange* composition_range = nullptr;
    result = insert_at_selection->InsertTextAtSelection(edit_cookie,
                                                        TF_IAS_QUERYONLY,
                                                        nullptr,
                                                        0,
                                                        &composition_range);
    insert_at_selection->Release();
    if (FAILED(result) || composition_range == nullptr) {
      return false;
    }

    ITfContextComposition* context_composition = nullptr;
    result = context_->QueryInterface(IID_ITfContextComposition,
                                      reinterpret_cast<void**>(&context_composition));
    if (FAILED(result) || context_composition == nullptr) {
      composition_range->Release();
      return false;
    }

    ITfComposition* composition = nullptr;
    result =
        context_composition->StartComposition(edit_cookie, composition_range, sink_, &composition);
    context_composition->Release();
    if (FAILED(result) || composition == nullptr) {
      composition_range->Release();
      return false;
    }

    *composition_slot_ = composition;
    bool started = SetRangeText(composition_range, edit_cookie, preedit_text_);
    if (started) {
      SetRangeDisplayAttribute(context_,
                               edit_cookie,
                               composition_range,
                               display_attribute_atom_);
    }
    if (started) {
      MoveSelectionToRangeEnd(context_, edit_cookie, composition_range);
    }
    if (!started) {
      *composition_slot_ = nullptr;
      composition->EndComposition(edit_cookie);
      composition->Release();
    }

    composition_range->Release();
    return started;
  }

  bool CommitAndRestart(TfEditCookie edit_cookie) {
    if (context_ == nullptr || composition_slot_ == nullptr ||
        *composition_slot_ == nullptr || commit_text_.empty() || preedit_text_.empty()) {
      return false;
    }

    ITfComposition* previous_composition = *composition_slot_;
    ITfRange* previous_range = nullptr;
    HRESULT result = previous_composition->GetRange(&previous_range);
    if (FAILED(result) || previous_range == nullptr) {
      return false;
    }

    ClearRangeDisplayAttribute(context_, edit_cookie, previous_range);
    bool committed_text = SetRangeText(previous_range, edit_cookie, commit_text_);
    if (committed_text) {
      committed_text = MoveSelectionToRangeEnd(context_, edit_cookie, previous_range);
    }
    previous_range->Release();
    if (!committed_text) {
      return false;
    }

    *composition_slot_ = nullptr;
    result = previous_composition->EndComposition(edit_cookie);
    previous_composition->Release();
    committed_ = true;
    if (FAILED(result)) {
      return false;
    }

    return StartPreedit(edit_cookie);
  }

  std::atomic<unsigned long> ref_count_{1};
  ITfContext* context_ = nullptr;
  ITfCompositionSink* sink_ = nullptr;
  ITfComposition** composition_slot_ = nullptr;
  TfGuidAtom display_attribute_atom_ = TF_INVALID_GUIDATOM;
  std::wstring commit_text_;
  std::wstring preedit_text_;
  bool succeeded_ = false;
  bool committed_ = false;
};

bool RequestCompositionCommitAndRestart(TfClientId client_id,
                                        ITfContext* context,
                                        ITfCompositionSink* sink,
                                        ITfComposition** composition_slot,
                                        TfGuidAtom display_attribute_atom,
                                        const std::wstring& commit_text,
                                        const std::wstring& preedit_text,
                                        bool* committed) {
  if (committed != nullptr) {
    *committed = false;
  }
  if (context == nullptr || sink == nullptr || composition_slot == nullptr ||
      commit_text.empty() || preedit_text.empty()) {
    return false;
  }
  auto* session = new (std::nothrow) CommitAndRestartCompositionEditSession(context,
                                                                            sink,
                                                                            composition_slot,
                                                                            display_attribute_atom,
                                                                            commit_text,
                                                                            preedit_text);
  if (session == nullptr) {
    return false;
  }

  HRESULT edit_result = E_FAIL;
  const HRESULT request_result = context->RequestEditSession(
      client_id, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
  const bool succeeded =
      SUCCEEDED(request_result) && SUCCEEDED(edit_result) && session->succeeded();
  if (committed != nullptr) {
    *committed = session->committed();
  }
  session->Release();
  return succeeded;
}

class AnchorEditSession final : public ITfEditSession {
 public:
  AnchorEditSession(ITfContext* context, ITfComposition* composition, POINT* anchor)
      : context_(context), composition_(composition), anchor_(anchor) {
    if (context_ != nullptr) {
      context_->AddRef();
    }
    if (composition_ != nullptr) {
      composition_->AddRef();
    }
  }

  AnchorEditSession(const AnchorEditSession&) = delete;
  AnchorEditSession& operator=(const AnchorEditSession&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (object == nullptr) {
      return E_POINTER;
    }

    *object = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
      *object = static_cast<ITfEditSession*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_count_; }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG count = --ref_count_;
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    if (context_ == nullptr || anchor_ == nullptr) {
      return E_FAIL;
    }

    ITfContextView* view = nullptr;
    HRESULT result = context_->GetActiveView(&view);
    if (SUCCEEDED(result) && view != nullptr) {
      if (composition_ != nullptr) {
        ITfRange* composition_range = nullptr;
        result = composition_->GetRange(&composition_range);
        if (SUCCEEDED(result) && composition_range != nullptr) {
          if (!TryRangeAnchor(view, edit_cookie, composition_range, false)) {
            TryRangeAnchor(view, edit_cookie, composition_range, true);
          }
          composition_range->Release();
        }
      }

      if (!succeeded_) {
        TF_SELECTION selection{};
        ULONG fetched = 0;
        result = context_->GetSelection(edit_cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        if (SUCCEEDED(result) && fetched != 0 && selection.range != nullptr) {
          TryRangeAnchor(view, edit_cookie, selection.range, false);
          selection.range->Release();
        }
      }
      view->Release();
    }

    return succeeded_ ? S_OK : E_FAIL;
  }

  bool succeeded() const noexcept { return succeeded_; }

 private:
  ~AnchorEditSession() {
    if (composition_ != nullptr) {
      composition_->Release();
      composition_ = nullptr;
    }
    if (context_ != nullptr) {
      context_->Release();
      context_ = nullptr;
    }
  }

  bool TryRangeAnchor(ITfContextView* view,
                      TfEditCookie edit_cookie,
                      ITfRange* range,
                      bool collapse_to_end) {
    if (view == nullptr || range == nullptr) {
      return false;
    }

    ITfRange* target_range = nullptr;
    HRESULT result = range->Clone(&target_range);
    if (FAILED(result) || target_range == nullptr) {
      return false;
    }

    if (collapse_to_end) {
      result = target_range->Collapse(edit_cookie, TF_ANCHOR_END);
      if (FAILED(result)) {
        target_range->Release();
        return false;
      }
    }

    RECT text_rect{};
    BOOL clipped = FALSE;
    result = view->GetTextExt(edit_cookie, target_range, &text_rect, &clipped);
    target_range->Release();
    if (FAILED(result)) {
      return false;
    }

    if (!IsUsableTextRect(text_rect)) {
      return false;
    }

    anchor_->x = text_rect.left;
    anchor_->y = text_rect.bottom;
    succeeded_ = true;
    return true;
  }

  std::atomic<unsigned long> ref_count_{1};
  ITfContext* context_ = nullptr;
  ITfComposition* composition_ = nullptr;
  POINT* anchor_ = nullptr;
  bool succeeded_ = false;
};

bool RequestSelectionAnchor(TfClientId client_id,
                            ITfContext* context,
                            ITfComposition* composition,
                            POINT* anchor) {
  if (context == nullptr || anchor == nullptr) {
    return false;
  }

  auto* session = new (std::nothrow) AnchorEditSession(context, composition, anchor);
  if (session == nullptr) {
    return false;
  }

  HRESULT edit_result = E_FAIL;
  const HRESULT request_result =
      context->RequestEditSession(client_id, session, TF_ES_SYNC | TF_ES_READ, &edit_result);
  const bool succeeded =
      SUCCEEDED(request_result) && SUCCEEDED(edit_result) && session->succeeded();
  session->Release();
  return succeeded;
}

HWND ContextViewWindow(ITfContext* context) {
  if (context == nullptr) {
    return nullptr;
  }

  ITfContextView* view = nullptr;
  HRESULT result = context->GetActiveView(&view);
  if (FAILED(result) || view == nullptr) {
    return nullptr;
  }

  HWND window = nullptr;
  result = view->GetWnd(&window);
  view->Release();
  return SUCCEEDED(result) ? window : nullptr;
}

bool TryGuiThreadCaretAnchor(HWND window, POINT* anchor) {
  if (window == nullptr || anchor == nullptr) {
    return false;
  }

  GUITHREADINFO gui_info{};
  gui_info.cbSize = sizeof(gui_info);
  const DWORD thread_id = GetWindowThreadProcessId(window, nullptr);
  if (thread_id == 0 || !GetGUIThreadInfo(thread_id, &gui_info)) {
    return false;
  }

  const bool has_caret_rect = gui_info.rcCaret.left != gui_info.rcCaret.right ||
                              gui_info.rcCaret.top != gui_info.rcCaret.bottom;
  HWND caret_window = gui_info.hwndCaret != nullptr ? gui_info.hwndCaret : gui_info.hwndFocus;
  if (has_caret_rect && caret_window != nullptr) {
    POINT caret{gui_info.rcCaret.left, gui_info.rcCaret.bottom};
    ClientToScreen(caret_window, &caret);
    if (IsUsableScreenPoint(caret)) {
      *anchor = caret;
      return true;
    }
  }

  return false;
}

bool TryGuiThreadFocusAnchor(HWND window, POINT* anchor) {
  if (window == nullptr || anchor == nullptr) {
    return false;
  }

  GUITHREADINFO gui_info{};
  gui_info.cbSize = sizeof(gui_info);
  const DWORD thread_id = GetWindowThreadProcessId(window, nullptr);
  if (thread_id == 0 || !GetGUIThreadInfo(thread_id, &gui_info)) {
    return false;
  }

  HWND focus_window = gui_info.hwndFocus != nullptr ? gui_info.hwndFocus : window;
  RECT focus_rect{};
  if (focus_window != nullptr && GetWindowRect(focus_window, &focus_rect)) {
    POINT focus_point{focus_rect.left + ScaleForDpi(12, DpiForWindow(focus_window)),
                      focus_rect.top + ScaleForDpi(30, DpiForWindow(focus_window))};
    if (IsUsableScreenPoint(focus_point)) {
      *anchor = focus_point;
      return true;
    }
  }

  return false;
}

HWND RootWindowOf(HWND window) {
  return window != nullptr ? GetAncestor(window, GA_ROOT) : nullptr;
}

bool IsSameRootWindow(HWND first, HWND second) {
  HWND first_root = RootWindowOf(first);
  HWND second_root = RootWindowOf(second);
  return first_root != nullptr && second_root != nullptr && first_root == second_root;
}

void LogAnchorPoint(const wchar_t* source, POINT point) {
  (void)source;
  (void)point;
#if defined(FP_VERBOSE_ANCHOR_LOG)
  wchar_t buffer[128]{};
  swprintf_s(buffer, L"Candidate anchor %s: %ld,%ld", source, point.x, point.y);
  fp::LogInfo(L"tsf", buffer);
#endif
}

std::filesystem::path ModuleDirectory() {
  std::wstring buffer(32768, L'\0');
  const DWORD length =
      GetModuleFileNameW(g_module_instance, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) {
    return std::filesystem::current_path();
  }
  buffer.resize(length);
  return std::filesystem::path(buffer).parent_path();
}

std::wstring CurrentProcessImageName() {
  std::wstring buffer(32768, L'\0');
  DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) {
    return {};
  }
  buffer.resize(length);
  return std::filesystem::path(buffer).filename().wstring();
}

std::wstring CurrentProcessCommandLine() {
  const wchar_t* command_line = GetCommandLineW();
  return command_line != nullptr ? std::wstring(command_line) : std::wstring();
}

bool IsToolbarHostProcess() {
  std::wstring image_name = CurrentProcessImageName();
  std::transform(image_name.begin(), image_name.end(), image_name.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  if (image_name != L"fluent-pinyin-ui.exe") {
    return false;
  }
  std::wstring command_line = CurrentProcessCommandLine();
  std::transform(command_line.begin(), command_line.end(), command_line.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  return command_line.find(L"--toolbar") != std::wstring::npos ||
         command_line.find(L"--service") != std::wstring::npos;
}

std::wstring TextServiceClsidString() {
  LPOLESTR value = nullptr;
  if (StringFromCLSID(kTextServiceClsid, &value) != S_OK || value == nullptr) {
    return L"{76e3ad5b-1dd8-4584-b3cd-127df0239720}";
  }
  std::wstring result(value);
  CoTaskMemFree(value);
  return result;
}

std::optional<std::filesystem::path> RegisteredModulePathFromRoot(HKEY root) {
  const std::wstring key =
      L"Software\\Classes\\CLSID\\" + TextServiceClsidString() + L"\\InprocServer32";
  wchar_t buffer[MAX_PATH * 4]{};
  DWORD size = sizeof(buffer);
  const LSTATUS status =
      RegGetValueW(root, key.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, buffer, &size);
  if (status != ERROR_SUCCESS || buffer[0] == L'\0') {
    return std::nullopt;
  }
  return std::filesystem::path(buffer);
}

std::filesystem::path RegisteredInstallDirectory() {
  if (const auto path = RegisteredModulePathFromRoot(HKEY_CURRENT_USER);
      path.has_value() && std::filesystem::exists(*path)) {
    return path->parent_path();
  }
  if (const auto path = RegisteredModulePathFromRoot(HKEY_LOCAL_MACHINE);
      path.has_value() && std::filesystem::exists(*path)) {
    return path->parent_path();
  }
  return ModuleDirectory();
}

std::filesystem::path InstalledSiblingExecutable(std::wstring_view name) {
  const auto registered = RegisteredInstallDirectory() / std::wstring(name);
  if (std::filesystem::exists(registered)) {
    return registered;
  }
  return ModuleDirectory() / std::wstring(name);
}

std::wstring AsciiToWide(std::string_view value) {
  std::wstring result;
  result.reserve(value.size());
  for (const unsigned char ch : value) {
    result.push_back(static_cast<wchar_t>(ch));
  }
  return result;
}

bool IsAlphabetVirtualKey(WPARAM wparam) {
  return (wparam >= 'A' && wparam <= 'Z') || (wparam >= 'a' && wparam <= 'z');
}

char AlphabetVirtualKeyToLowerAscii(WPARAM wparam) {
  if (wparam >= 'A' && wparam <= 'Z') {
    return static_cast<char>('a' + (wparam - 'A'));
  }
  if (wparam >= 'a' && wparam <= 'z') {
    return static_cast<char>(wparam);
  }
  return '\0';
}

bool IsVirtualKeyDown(int virtual_key) {
  return (GetKeyState(virtual_key) & 0x8000) != 0;
}

bool IsCapsLockOn() {
  return (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
}

struct ShortcutChord {
  bool valid = false;
  bool ctrl = false;
  bool shift = false;
  bool alt = false;
  bool win = false;
  WPARAM key = 0;
};

std::wstring NormalizeShortcutToken(std::wstring token) {
  token = TrimShortcutDisplay(std::move(token));
  token.erase(std::remove_if(token.begin(), token.end(), [](wchar_t ch) {
                return std::iswspace(ch) != 0;
              }),
              token.end());
  std::transform(token.begin(), token.end(), token.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  return token;
}

std::optional<WPARAM> ShortcutKeyFromToken(const std::wstring& token) {
  if (token.empty()) {
    return std::nullopt;
  }
  if (token == L"." || token == L"period") {
    return VK_OEM_PERIOD;
  }
  if (token == L"," || token == L"comma") {
    return VK_OEM_COMMA;
  }
  if (token == L"-" || token == L"minus") {
    return VK_OEM_MINUS;
  }
  if (token == L"=" || token == L"plus") {
    return VK_OEM_PLUS;
  }
  if (token == L";" || token == L"semicolon") {
    return VK_OEM_1;
  }
  if (token == L"/" || token == L"slash") {
    return VK_OEM_2;
  }
  if (token == L"`" || token == L"grave") {
    return VK_OEM_3;
  }
  if (token == L"[" || token == L"leftbracket") {
    return VK_OEM_4;
  }
  if (token == L"\\" || token == L"backslash") {
    return VK_OEM_5;
  }
  if (token == L"]" || token == L"rightbracket") {
    return VK_OEM_6;
  }
  if (token == L"'" || token == L"quote") {
    return VK_OEM_7;
  }
  if (token == L"tab") {
    return VK_TAB;
  }
  if (token == L"pgup" || token == L"pageup") {
    return VK_PRIOR;
  }
  if (token == L"pgdn" || token == L"pagedown") {
    return VK_NEXT;
  }
  if (token == L"esc" || token == L"escape") {
    return VK_ESCAPE;
  }
  if (token == L"backspace") {
    return VK_BACK;
  }
  if (token == L"delete") {
    return VK_DELETE;
  }
  if (token == L"insert") {
    return VK_INSERT;
  }
  if (token == L"home") {
    return VK_HOME;
  }
  if (token == L"end") {
    return VK_END;
  }
  if (token == L"left") {
    return VK_LEFT;
  }
  if (token == L"right") {
    return VK_RIGHT;
  }
  if (token == L"up") {
    return VK_UP;
  }
  if (token == L"down") {
    return VK_DOWN;
  }
  if (token == L"space") {
    return VK_SPACE;
  }
  if (token == L"enter" || token == L"return") {
    return VK_RETURN;
  }
  if (token.size() >= 2 && token[0] == L'f') {
    try {
      const int function_key = std::stoi(token.substr(1));
      if (function_key >= 1 && function_key <= 12) {
        return static_cast<WPARAM>(VK_F1 + function_key - 1);
      }
    } catch (...) {
      return std::nullopt;
    }
  }
  if (token.size() == 1 && token[0] >= L'0' && token[0] <= L'9') {
    return static_cast<WPARAM>(token[0]);
  }
  if (token.size() == 1 && token[0] >= L'a' && token[0] <= L'z') {
    return static_cast<WPARAM>(std::towupper(token[0]));
  }
  return std::nullopt;
}

ShortcutChord ParseShortcutChord(std::wstring_view display) {
  ShortcutChord chord;
  std::wstring text(display);
  size_t start = 0;
  while (start <= text.size()) {
    const size_t separator = text.find(L'+', start);
    const size_t end = separator == std::wstring::npos ? text.size() : separator;
    const std::wstring token = NormalizeShortcutToken(text.substr(start, end - start));
    if (token == L"ctrl" || token == L"control") {
      chord.ctrl = true;
    } else if (token == L"shift") {
      chord.shift = true;
    } else if (token == L"alt") {
      chord.alt = true;
    } else if (token == L"win" || token == L"windows") {
      chord.win = true;
    } else if (const auto key = ShortcutKeyFromToken(token)) {
      chord.key = *key;
    } else {
      return {};
    }
    if (separator == std::wstring::npos) {
      break;
    }
    start = separator + 1;
  }
  if (chord.key == 0) {
    const int modifier_count =
        (chord.ctrl ? 1 : 0) + (chord.shift ? 1 : 0) + (chord.alt ? 1 : 0) +
        (chord.win ? 1 : 0);
    if (modifier_count == 1) {
      if (chord.ctrl) {
        chord.key = VK_CONTROL;
      } else if (chord.shift) {
        chord.key = VK_SHIFT;
      } else if (chord.alt) {
        chord.key = VK_MENU;
      } else if (chord.win) {
        chord.key = VK_LWIN;
      }
    }
  }
  chord.valid = chord.key != 0;
  return chord;
}

bool ShortcutKeyEquals(WPARAM expected, WPARAM actual) {
  if (expected == actual) {
    return true;
  }
  if (expected == VK_SHIFT) {
    return actual == VK_LSHIFT || actual == VK_RSHIFT;
  }
  if (expected == VK_CONTROL) {
    return actual == VK_LCONTROL || actual == VK_RCONTROL;
  }
  if (expected == VK_MENU) {
    return actual == VK_LMENU || actual == VK_RMENU;
  }
  if (expected == VK_LWIN || expected == VK_RWIN) {
    return actual == VK_LWIN || actual == VK_RWIN;
  }
  return false;
}

bool IsShortcutModifierVirtualKey(WPARAM wparam) {
  return wparam == VK_SHIFT || wparam == VK_LSHIFT || wparam == VK_RSHIFT ||
         wparam == VK_CONTROL || wparam == VK_LCONTROL || wparam == VK_RCONTROL ||
         wparam == VK_MENU || wparam == VK_LMENU || wparam == VK_RMENU ||
         wparam == VK_LWIN || wparam == VK_RWIN;
}

bool ShortcutMatches(std::wstring_view setting_key, std::wstring_view fallback, WPARAM wparam) {
  const ShortcutChord chord = ParseShortcutChord(ShortcutDisplay(setting_key, fallback));
  if (!chord.valid || !ShortcutKeyEquals(chord.key, wparam)) {
    return false;
  }
  return IsVirtualKeyDown(VK_CONTROL) == chord.ctrl && IsVirtualKeyDown(VK_SHIFT) == chord.shift &&
         IsVirtualKeyDown(VK_MENU) == chord.alt &&
         (IsVirtualKeyDown(VK_LWIN) || IsVirtualKeyDown(VK_RWIN)) == chord.win;
}

UINT ToolbarShortcutCommand(WPARAM wparam) {
  if (ShortcutMatches(L"shortcut_toolbar_input_mode", L"Shift", wparam)) {
    return kMenuInputMode;
  }
  if (ShortcutMatches(L"shortcut_toolbar_shape", L"Shift+.", wparam)) {
    return kMenuFullShape;
  }
  if (ShortcutMatches(L"shortcut_toolbar_punctuation", L"Ctrl+.", wparam)) {
    return kMenuPunctuation;
  }
  if (ShortcutMatches(L"shortcut_toolbar_charset", L"Ctrl+Shift+F", wparam)) {
    return kMenuCharset;
  }
  return 0;
}

enum class CandidateShortcutCommand {
  kNone,
  kExpand,
  kPreviousPage,
  kNextPage,
};

CandidateShortcutCommand CandidateShortcutForKey(WPARAM wparam) {
  if (ShortcutMatches(L"shortcut_candidate_expand", L"Tab", wparam)) {
    return CandidateShortcutCommand::kExpand;
  }
  if (ShortcutMatches(L"shortcut_candidate_previous_page", L"PgUp", wparam)) {
    return CandidateShortcutCommand::kPreviousPage;
  }
  if (ShortcutMatches(L"shortcut_candidate_next_page", L"PgDn", wparam)) {
    return CandidateShortcutCommand::kNextPage;
  }
  return CandidateShortcutCommand::kNone;
}

bool IsOnlyControlModifierDown() {
  return IsVirtualKeyDown(VK_CONTROL) && !IsVirtualKeyDown(VK_MENU) &&
         !IsVirtualKeyDown(VK_LWIN) && !IsVirtualKeyDown(VK_RWIN);
}

bool IsPrintableAsciiPunctuation(WPARAM wparam) {
  switch (wparam) {
    case VK_OEM_COMMA:
    case VK_OEM_PERIOD:
    case VK_OEM_1:
    case VK_OEM_2:
    case VK_OEM_3:
    case VK_OEM_4:
    case VK_OEM_5:
    case VK_OEM_6:
    case VK_OEM_7:
    case VK_OEM_MINUS:
    case VK_OEM_PLUS:
      return true;
    default:
      return false;
  }
}

std::wstring PunctuationForKey(WPARAM wparam) {
  const bool shifted = IsVirtualKeyDown(VK_SHIFT);
  switch (wparam) {
    case VK_OEM_COMMA:
      return shifted ? L"<" : L"\uFF0C";
    case VK_OEM_PERIOD:
      return shifted ? L">" : L"\u3002";
    case VK_OEM_1:
      return shifted ? L"\uFF1A" : L"\uFF1B";
    case VK_OEM_2:
      return shifted ? L"\uFF1F" : L"\u3001";
    case VK_OEM_3:
      return shifted ? L"~" : L"\u00B7";
    case VK_OEM_4:
      return shifted ? L"\u3010" : L"\uFF3B";
    case VK_OEM_5:
      return shifted ? L"|" : L"\u3001";
    case VK_OEM_6:
      return shifted ? L"\u3011" : L"\uFF3D";
    case VK_OEM_7:
      return shifted ? L"\u201C" : L"\u2018";
    case VK_OEM_MINUS:
      return shifted ? L"\u2014" : L"-";
    case VK_OEM_PLUS:
      return shifted ? L"+" : L"=";
    default:
      return {};
  }
}

std::wstring AsciiPunctuationForKey(WPARAM wparam) {
  const bool shifted = IsVirtualKeyDown(VK_SHIFT);
  switch (wparam) {
    case VK_OEM_COMMA:
      return shifted ? L"<" : L",";
    case VK_OEM_PERIOD:
      return shifted ? L">" : L".";
    case VK_OEM_1:
      return shifted ? L":" : L";";
    case VK_OEM_2:
      return shifted ? L"?" : L"/";
    case VK_OEM_3:
      return shifted ? L"~" : L"`";
    case VK_OEM_4:
      return shifted ? L"{" : L"[";
    case VK_OEM_5:
      return shifted ? L"|" : L"\\";
    case VK_OEM_6:
      return shifted ? L"}" : L"]";
    case VK_OEM_7:
      return shifted ? L"\"" : L"'";
    case VK_OEM_MINUS:
      return shifted ? L"_" : L"-";
    case VK_OEM_PLUS:
      return shifted ? L"+" : L"=";
    default:
      return {};
  }
}

}  // namespace

TsfTextService::TsfTextService() {
  ++g_object_count;
  toolbar_host_process_ = IsToolbarHostProcess();
  fp::LogInfo(L"tsf", L"TsfTextService created.");
}

TsfTextService::~TsfTextService() {
  {
    std::lock_guard<std::mutex> lock(rime_mutex_);
    service_active_ = false;
  }

  DestroyContextMenu();
  DestroyCandidateWindow();
  DestroyStatusTip();
  DestroyToolbarWindow();
  DestroyControlWindow();
  UninitializeRime();

  if (composition_ != nullptr) {
    composition_->Release();
    composition_ = nullptr;
  }

  if (active_context_ != nullptr) {
    active_context_->Release();
    active_context_ = nullptr;
  }

  if (keystroke_mgr_ != nullptr && client_id_ != 0) {
    keystroke_mgr_->UnadviseKeyEventSink(client_id_);
  }
  if (keystroke_mgr_ != nullptr) {
    keystroke_mgr_->Release();
    keystroke_mgr_ = nullptr;
  }

  if (lang_bar_item_mgr_ != nullptr && input_mode_item_ != nullptr) {
    lang_bar_item_mgr_->RemoveItem(input_mode_item_);
  }
  if (input_mode_item_ != nullptr) {
    input_mode_item_->Release();
    input_mode_item_ = nullptr;
  }
  if (lang_bar_item_mgr_ != nullptr) {
    lang_bar_item_mgr_->Release();
    lang_bar_item_mgr_ = nullptr;
  }

  if (thread_mgr_ != nullptr) {
    thread_mgr_->Release();
    thread_mgr_ = nullptr;
  }

  --g_object_count;
  fp::LogInfo(L"tsf", L"TsfTextService destroyed.");
}

STDMETHODIMP TsfTextService::QueryInterface(REFIID riid, void** object) {
  if (object == nullptr) {
    return E_POINTER;
  }

  *object = nullptr;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor) ||
      IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
    *object = static_cast<ITfTextInputProcessorEx*>(this);
    AddRef();
    return S_OK;
  }

  if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
    *object = static_cast<ITfKeyEventSink*>(this);
    AddRef();
    return S_OK;
  }

  if (IsEqualIID(riid, IID_ITfCompositionSink)) {
    *object = static_cast<ITfCompositionSink*>(this);
    AddRef();
    return S_OK;
  }

  if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
    *object = static_cast<ITfDisplayAttributeProvider*>(this);
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) TsfTextService::AddRef() {
  return ++ref_count_;
}

STDMETHODIMP_(ULONG) TsfTextService::Release() {
  const ULONG count = --ref_count_;
  if (count == 0) {
    delete this;
  }

  return count;
}

STDMETHODIMP TsfTextService::Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) {
  if (thread_mgr == nullptr) {
    return E_INVALIDARG;
  }

  const ULONGLONG activate_start_tick = GetTickCount64();

  if (thread_mgr_ != nullptr) {
    thread_mgr_->Release();
  }

  thread_mgr_ = thread_mgr;
  thread_mgr_->AddRef();
  client_id_ = client_id;
  {
    std::lock_guard<std::mutex> lock(rime_mutex_);
    service_active_ = true;
  }
  CreateControlWindow();
  HRESULT result = thread_mgr_->QueryInterface(IID_ITfKeystrokeMgr,
                                               reinterpret_cast<void**>(&keystroke_mgr_));
  if (SUCCEEDED(result) && keystroke_mgr_ != nullptr) {
    result = keystroke_mgr_->AdviseKeyEventSink(client_id_, this, TRUE);
    if (FAILED(result)) {
      fp::LogWarning(L"tsf", L"AdviseKeyEventSink failed.");
      keystroke_mgr_->Release();
      keystroke_mgr_ = nullptr;
    }
  } else {
    fp::LogWarning(L"tsf", L"ITfKeystrokeMgr unavailable.");
  }

  LoadUserSettings();
  ApplyDefaultInputStateFromSettings();
  RefreshProfileIconForSystemTheme();
  UpdateInputModeCompartments();

  ITfCategoryMgr* category_mgr = nullptr;
  result = CoCreateInstance(CLSID_TF_CategoryMgr,
                            nullptr,
                            CLSCTX_INPROC_SERVER,
                            IID_ITfCategoryMgr,
                            reinterpret_cast<void**>(&category_mgr));
  if (SUCCEEDED(result) && category_mgr != nullptr) {
    result = category_mgr->RegisterGUID(kDisplayAttributeInputGuid,
                                        &display_attribute_input_atom_);
    if (FAILED(result)) {
      display_attribute_input_atom_ = TF_INVALID_GUIDATOM;
      fp::LogWarning(L"tsf", L"Display attribute GUID registration failed.");
    }
    category_mgr->Release();
  } else {
    fp::LogWarning(L"tsf", L"ITfCategoryMgr unavailable.");
  }

  ApplyRimeOptions();

  result = thread_mgr_->QueryInterface(IID_ITfLangBarItemMgr,
                                       reinterpret_cast<void**>(&lang_bar_item_mgr_));
  if (SUCCEEDED(result)) {
      input_mode_item_ = new (std::nothrow) InputModeLangBarItem(this);
    if (input_mode_item_ == nullptr) {
      fp::LogWarning(L"tsf", L"Failed to allocate input mode language bar item.");
    } else {
      result = lang_bar_item_mgr_->AddItem(input_mode_item_);
      if (FAILED(result)) {
        wchar_t buffer[96]{};
        swprintf_s(buffer,
                   L"Failed to add input mode language bar item: 0x%08lX",
                   static_cast<unsigned long>(result));
        fp::LogWarning(L"tsf", buffer);
        input_mode_item_->Release();
        input_mode_item_ = nullptr;
      } else {
        NotifyInputModeChanged();
      }
    }
  } else {
    fp::LogWarning(L"tsf", L"Failed to query language bar item manager.");
  }

  fp::LogInfo(L"tsf",
              std::wstring(fp::kProductName) + L" text service activated in " +
                  std::to_wstring(GetTickCount64() - activate_start_tick) + L" ms.");
  return S_OK;
}

STDMETHODIMP TsfTextService::ActivateEx(ITfThreadMgr* thread_mgr,
                                        TfClientId client_id,
                                        DWORD flags) {
  (void)flags;
  return Activate(thread_mgr, client_id);
}

STDMETHODIMP TsfTextService::Deactivate() {
  {
    std::lock_guard<std::mutex> lock(rime_mutex_);
    service_active_ = false;
  }

  DestroyContextMenu();
  DestroyCandidateWindow();
  DestroyStatusTip();
  DestroyToolbarWindow();
  DestroyControlWindow();
  CancelComposition(nullptr);

  if (composition_ != nullptr) {
    composition_->Release();
    composition_ = nullptr;
  }

  if (active_context_ != nullptr) {
    active_context_->Release();
    active_context_ = nullptr;
  }

  if (keystroke_mgr_ != nullptr && client_id_ != 0) {
    keystroke_mgr_->UnadviseKeyEventSink(client_id_);
  }
  if (keystroke_mgr_ != nullptr) {
    keystroke_mgr_->Release();
    keystroke_mgr_ = nullptr;
  }

  if (lang_bar_item_mgr_ != nullptr && input_mode_item_ != nullptr) {
    lang_bar_item_mgr_->RemoveItem(input_mode_item_);
  }
  if (input_mode_item_ != nullptr) {
    input_mode_item_->Release();
    input_mode_item_ = nullptr;
  }
  if (lang_bar_item_mgr_ != nullptr) {
    lang_bar_item_mgr_->Release();
    lang_bar_item_mgr_ = nullptr;
  }

  if (thread_mgr_ != nullptr) {
    thread_mgr_->Release();
    thread_mgr_ = nullptr;
  }

  display_attribute_input_atom_ = TF_INVALID_GUIDATOM;
  client_id_ = 0;
  fp::LogInfo(L"tsf", std::wstring(fp::kProductName) + L" text service deactivated.");
  return S_OK;
}

STDMETHODIMP TsfTextService::OnSetFocus(BOOL foreground) {
  fp::LogInfo(L"tsf", foreground ? L"Key sink focused." : L"Key sink unfocused.");
  if (foreground) {
    has_input_focus_ = true;
    ApplyDefaultInputStateFromSettings();
    NotifyInputModeChanged();
    ApplyRimeOptions();
    WarmUpRimeAsync(120);
    if (toolbar_visible_) {
      RefreshToolbarHostIfVisible(toolbar_visible_);
      ShowToolbarWindow();
    }
    ShowStatusTip(active_context_);
  } else {
    CancelComposition(active_context_);
    has_input_focus_ = false;
    NotifyInputModeChanged();
    HideCandidateWindow();
    DestroyStatusTip();
    if (toolbar_visible_ && toolbar_window_ != nullptr) {
      KeepToolbarWindowTopmost();
    }
  }
  return S_OK;
}

STDMETHODIMP TsfTextService::OnTestKeyDown(ITfContext* context,
                                           WPARAM wparam,
                                           LPARAM lparam,
                                           BOOL* eaten) {
  (void)context;
  if (eaten == nullptr) {
    return E_POINTER;
  }

  const bool handled = IsKeyHandled(wparam, lparam);
  *eaten = handled ? TRUE : FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextService::OnKeyDown(ITfContext* context,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       BOOL* eaten) {
  if (eaten == nullptr) {
    return E_POINTER;
  }

  *eaten = HandleKey(context, wparam, lparam) ? TRUE : FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextService::OnTestKeyUp(ITfContext* context,
                                         WPARAM wparam,
                                         LPARAM lparam,
                                         BOOL* eaten) {
  (void)context;
  (void)lparam;
  if (eaten == nullptr) {
    return E_POINTER;
  }

  const bool shift_key =
      wparam == VK_SHIFT || wparam == VK_LSHIFT || wparam == VK_RSHIFT;
  *eaten = ((input_mode_shortcut_down_ &&
             ShortcutKeyEquals(input_mode_shortcut_key_, wparam)) ||
            (shift_key && shift_key_down_ && !IsModifierShortcutActive()))
               ? TRUE
               : FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextService::OnKeyUp(ITfContext* context,
                                     WPARAM wparam,
                                     LPARAM lparam,
                                     BOOL* eaten) {
  (void)lparam;
  if (eaten == nullptr) {
    return E_POINTER;
  }

  if (input_mode_shortcut_down_ &&
      ShortcutKeyEquals(input_mode_shortcut_key_, wparam)) {
    input_mode_shortcut_down_ = false;
    input_mode_shortcut_key_ = 0;
    shift_key_down_ = false;
    *eaten = ToggleAsciiModeFromKey(context, false) ? TRUE : FALSE;
    return S_OK;
  }

  if ((wparam == VK_SHIFT || wparam == VK_LSHIFT || wparam == VK_RSHIFT) && shift_key_down_) {
    shift_key_down_ = false;
    if (IsModifierShortcutActive()) {
      *eaten = FALSE;
      return S_OK;
    }
    *eaten = ToggleAsciiModeFromKey(context, false) ? TRUE : FALSE;
    return S_OK;
  }

  *eaten = FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextService::OnPreservedKey(ITfContext* context,
                                            REFGUID guid,
                                            BOOL* eaten) {
  (void)context;
  (void)guid;
  if (eaten == nullptr) {
    return E_POINTER;
  }

  *eaten = FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextService::OnCompositionTerminated(TfEditCookie edit_cookie,
                                                     ITfComposition* composition) {
  (void)edit_cookie;
  if (suppress_next_composition_termination_) {
    suppress_next_composition_termination_ = false;
    return S_OK;
  }
  if (composition_ != nullptr && composition_ != composition) {
    return S_OK;
  }
  if (pending_candidate_continuation_ && !composition_input_.empty()) {
    if (composition_ == composition && composition_ != nullptr) {
      composition_->Release();
      composition_ = nullptr;
    }
    preedit_length_ = 0;
    return S_OK;
  }
  if (composition_ == nullptr && !composition_input_.empty()) {
    preedit_length_ = 0;
    return S_OK;
  }
  if (composition_ == composition && composition_ != nullptr) {
    composition_->Release();
    composition_ = nullptr;
  }
  ClearCompositionState();
  HideCandidateWindow();
  HideStatusTip();
  return S_OK;
}

STDMETHODIMP TsfTextService::EnumDisplayAttributeInfo(
    IEnumTfDisplayAttributeInfo** enum_info) {
  if (enum_info == nullptr) {
    return E_POINTER;
  }
  auto* enumerator = new (std::nothrow) DisplayAttributeInfoEnumerator();
  if (enumerator == nullptr) {
    *enum_info = nullptr;
    return E_OUTOFMEMORY;
  }
  *enum_info = enumerator;
  return S_OK;
}

STDMETHODIMP TsfTextService::GetDisplayAttributeInfo(REFGUID guid,
                                                     ITfDisplayAttributeInfo** info) {
  if (info == nullptr) {
    return E_POINTER;
  }
  *info = nullptr;
  if (!IsEqualGUID(guid, kDisplayAttributeInputGuid)) {
    return E_INVALIDARG;
  }
  auto* attribute = new (std::nothrow) DisplayAttributeInfoInput();
  if (attribute == nullptr) {
    return E_OUTOFMEMORY;
  }
  *info = attribute;
  return S_OK;
}

bool TsfTextService::IsComposing() const noexcept {
  return !composition_input_.empty() || composition_ != nullptr || preedit_length_ > 0;
}

void TsfTextService::NotifyInputModeChanged() {
  UpdateInputModeCompartments();
  if (input_mode_item_ == nullptr) {
    return;
  }

  InputModeLangBarItem* item = static_cast<InputModeLangBarItem*>(input_mode_item_);
  item->NotifyUpdated();
  if (lang_bar_item_mgr_ != nullptr) {
    const GUID item_guid = kInputModeLangBarItemGuid;
    DWORD status = 0;
    if (FAILED(lang_bar_item_mgr_->GetItemsStatus(1, &item_guid, &status))) {
      fp::LogWarning(L"tsf", L"Failed to query input mode language bar item status.");
    }
  }
}

void TsfTextService::UpdateInputModeCompartments() {
  const DWORD open = ascii_mode_ ? 0 : 1;
  const DWORD conversion = ascii_mode_ ? TF_CONVERSIONMODE_ALPHANUMERIC
                                       : TF_CONVERSIONMODE_NATIVE;
  if (!SetGlobalCompartmentDword(thread_mgr_,
                                 client_id_,
                                 GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                 open) &&
      !SetCompartmentDword(thread_mgr_, client_id_, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, open)) {
    fp::LogWarning(L"tsf", L"Failed to update keyboard open/close compartment.");
  }
  if (!SetGlobalCompartmentDword(thread_mgr_,
                                 client_id_,
                                 GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
                                 conversion) &&
      !SetCompartmentDword(thread_mgr_,
                           client_id_,
                           GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
                           conversion)) {
    fp::LogWarning(L"tsf", L"Failed to update keyboard conversion compartment.");
  }
}

void TsfTextService::LoadUserSettings(bool force, bool allow_candidate_changes_during_composition) {
  const DWORD now = GetTickCount();
  if (!force && status_tip_settings_tick_ != 0 &&
      now - status_tip_settings_tick_ < kStatusTipSettingsRefreshMs) {
    return;
  }
  status_tip_settings_tick_ = now;

  const bool previous_horizontal_layout = horizontal_candidate_layout_;
  const int previous_candidate_count = compact_candidate_count_;
  const int previous_candidate_font_size_level = candidate_font_size_level_;
  const std::wstring previous_candidate_font_family = candidate_font_family_;
  const bool previous_toolbar_visible = toolbar_visible_;
  const std::wstring legacy_theme =
      ReadStringSetting(fp::kLegacyThemeSetting, fp::kThemeModeDark);
  const std::wstring candidate_layout = ReadStringSetting(L"candidate_layout");
  if (candidate_layout == L"horizontal") {
    horizontal_candidate_layout_ = true;
  } else if (candidate_layout == L"vertical") {
    horizontal_candidate_layout_ = false;
  } else {
    horizontal_candidate_layout_ = ReadBoolSetting(L"candidate_horizontal", true);
  }
  toolbar_visible_ = ReadToolbarVisibleSetting(toolbar_visible_);
  toolbar_vertical_layout_ = ReadStringSetting(kToolbarLayoutSetting, L"horizontal") == L"vertical";
  toolbar_visible_items_ = ParseToolbarVisibleItems(ReadStringSetting(kToolbarItemsSetting));
  super_abbrev_enabled_ = ReadBoolSetting(L"super_abbrev", true);
  toolbar_position_user_ = ReadBoolSetting(kToolbarPositionUserSetting, false);
  std::optional<POINT> toolbar_point =
      toolbar_position_user_ ? ReadPointSetting(kToolbarPositionSetting) : std::nullopt;
  if (toolbar_point) {
    toolbar_position_ = *toolbar_point;
    has_toolbar_position_ = true;
  } else {
    const bool keep_existing_user_position = toolbar_position_user_ && has_toolbar_position_;
    toolbar_position_user_ = keep_existing_user_position;
    has_toolbar_position_ = keep_existing_user_position;
  }
  SaveToolbarSetting();
  status_tip_enabled_ = ReadBoolSetting(L"status_tip_enabled", true);
  status_tip_blacklist_ = ReadStringSetting(L"status_tip_blacklist", kDefaultStatusTipBlacklist);
  compact_candidate_count_ = ReadIntSetting(L"candidate_count",
                                            kDefaultCompactCandidateCount,
                                            kMinCompactCandidateCount,
                                            kMaxCompactCandidateCount);
  candidate_font_size_level_ = ReadIntSetting(L"candidate_font_size_level",
                                              kDefaultCandidateFontSizeLevel,
                                              kMinCandidateFontSizeLevel,
                                              kMaxCandidateFontSizeLevel);
  candidate_font_family_ = NormalizeCandidateFontFamilySetting(
      ReadStringSetting(L"candidate_font_family", kDefaultCandidateFontFamily));
  theme_mode_ =
      fp::NormalizeThemeModeSetting(ReadStringSetting(fp::kThemeModeSetting, legacy_theme));
  theme_preset_ =
      fp::NormalizeThemePresetSetting(ReadStringSetting(fp::kThemePresetSetting, legacy_theme));
  apps_use_light_theme_ = AppsUseLightTheme();
  system_uses_light_theme_ = SystemUsesLightTheme();
  if (IsComposing()) {
    if (!allow_candidate_changes_during_composition) {
      horizontal_candidate_layout_ = previous_horizontal_layout;
      compact_candidate_count_ = previous_candidate_count;
      candidate_font_size_level_ = previous_candidate_font_size_level;
      candidate_font_family_ = previous_candidate_font_family;
    }
  }
  if (toolbar_visible_ != previous_toolbar_visible) {
    if (toolbar_visible_) {
      RefreshToolbarHostIfVisible(toolbar_visible_);
      ShowToolbarWindow();
    } else if (!toolbar_visible_) {
      DestroyToolbarWindow();
      RequestToolbarHostShutdown();
      RefreshToolbarHostIfVisible(toolbar_visible_);
    }
  } else if (toolbar_visible_) {
    if (toolbar_window_ != nullptr) {
      KeepToolbarWindowTopmost();
    }
  } else if (!toolbar_visible_) {
    DestroyToolbarWindow();
  }
}

void TsfTextService::ApplyInputStateFromSettings(bool allow_during_composition) {
  if (!allow_during_composition && IsComposing()) {
    return;
  }

  ascii_mode_ = ReadStringSetting(L"default_input_mode", L"zh") == L"en";
  caps_lock_ascii_mode_ = false;
  simplified_charset_ = ReadStringSetting(L"default_charset", L"simplified") != L"traditional";
  full_shape_mode_ = !ReadBoolSetting(L"default_shape_half", true);
  chinese_punctuation_mode_ = ReadBoolSetting(L"default_chinese_punctuation", true);
  if (status_tip_icon_mode_ == StatusTipIconMode::kChinesePunctuation ||
      status_tip_icon_mode_ == StatusTipIconMode::kEnglishPunctuation) {
    status_tip_icon_mode_ = chinese_punctuation_mode_ ? StatusTipIconMode::kChinesePunctuation
                                                      : StatusTipIconMode::kEnglishPunctuation;
  }
}

void TsfTextService::ApplyDefaultInputStateFromSettings() {
  ApplyInputStateFromSettings(false);
}

void TsfTextService::RefreshInputStateFromSettings() {
  const bool previous_ascii_mode = ascii_mode_;
  const bool previous_simplified_charset = simplified_charset_;
  const bool previous_full_shape_mode = full_shape_mode_;
  const bool previous_chinese_punctuation_mode = chinese_punctuation_mode_;
  const bool previous_super_abbrev_enabled = super_abbrev_enabled_;
  const bool previous_horizontal_candidate_layout = horizontal_candidate_layout_;
  const int previous_compact_candidate_count = compact_candidate_count_;
  const int previous_candidate_font_size_level = candidate_font_size_level_;
  const std::wstring previous_candidate_font_family = candidate_font_family_;
  const std::wstring previous_theme_mode = theme_mode_;
  const std::wstring previous_theme_preset = theme_preset_;
  const bool previous_apps_use_light_theme = apps_use_light_theme_;
  const bool previous_system_uses_light_theme = system_uses_light_theme_;

  LoadUserSettings(true, true);
  ApplyInputStateFromSettings(true);

  if (previous_full_shape_mode != full_shape_mode_) {
    status_tip_icon_mode_ = full_shape_mode_ ? StatusTipIconMode::kFullShape
                                             : StatusTipIconMode::kHalfShape;
  } else if (previous_chinese_punctuation_mode != chinese_punctuation_mode_) {
    status_tip_icon_mode_ = chinese_punctuation_mode_ ? StatusTipIconMode::kChinesePunctuation
                                                      : StatusTipIconMode::kEnglishPunctuation;
  }

  const bool rime_options_changed =
      previous_simplified_charset != simplified_charset_ ||
      previous_full_shape_mode != full_shape_mode_ ||
      previous_chinese_punctuation_mode != chinese_punctuation_mode_ ||
      previous_super_abbrev_enabled != super_abbrev_enabled_;
  if (rime_options_changed) {
    ApplyRimeOptions();
    last_candidate_query_input_.clear();
    last_candidate_query_page_index_ = -1;
    last_candidate_query_page_size_ = 0;
    if (IsComposing()) {
      candidate_page_index_ = 0;
      selected_candidate_index_ = 0;
      RefreshCandidates();
      ShowCandidateWindow(active_context_);
    }
  }

  const bool candidate_window_settings_changed =
      previous_horizontal_candidate_layout != horizontal_candidate_layout_ ||
      previous_compact_candidate_count != compact_candidate_count_ ||
      previous_candidate_font_size_level != candidate_font_size_level_ ||
      previous_candidate_font_family != candidate_font_family_;
  const bool theme_changed =
      previous_theme_mode != theme_mode_ || previous_theme_preset != theme_preset_ ||
      previous_apps_use_light_theme != apps_use_light_theme_ ||
      previous_system_uses_light_theme != system_uses_light_theme_;
  if (candidate_window_settings_changed && !rime_options_changed) {
    last_candidate_query_input_.clear();
    last_candidate_query_page_index_ = -1;
    last_candidate_query_page_size_ = 0;
    if (IsComposing()) {
      candidate_page_index_ = 0;
      selected_candidate_index_ = 0;
      RefreshCandidates();
      ShowCandidateWindow(active_context_);
    } else if (candidate_window_ != nullptr) {
      RenderCandidateLayeredWindow();
    }
  } else if (theme_changed && candidate_window_ != nullptr) {
    ApplyCandidateDwmFrame(candidate_window_, theme_mode_, theme_preset_);
    if (IsComposing()) {
      ShowCandidateWindow(active_context_);
    } else {
      RenderCandidateLayeredWindow();
    }
  }

  if (previous_ascii_mode != ascii_mode_ || rime_options_changed || theme_changed) {
    if (theme_changed) {
      RefreshProfileIconForSystemTheme();
    }
    NotifyInputModeChanged();
  }
  if (toolbar_window_ != nullptr) {
    RenderToolbarLayeredWindow();
  }
  if (toolbar_tooltip_window_ != nullptr && IsWindowVisible(toolbar_tooltip_window_)) {
    RenderToolbarTooltipLayeredWindow();
  }
  if (candidate_tooltip_window_ != nullptr && IsWindowVisible(candidate_tooltip_window_)) {
    RenderCandidateTooltipLayeredWindow();
  }
  if (context_menu_window_ != nullptr && IsWindowVisible(context_menu_window_)) {
    RenderContextMenuLayeredWindow();
  }
  if (context_submenu_window_ != nullptr && IsWindowVisible(context_submenu_window_)) {
    RenderContextSubmenuLayeredWindow();
  }
  if (previous_ascii_mode != ascii_mode_ || rime_options_changed || theme_changed ||
      previous_full_shape_mode != full_shape_mode_ ||
      previous_chinese_punctuation_mode != chinese_punctuation_mode_) {
    RefreshToolbarHostIfVisible(toolbar_visible_);
  }
  if (status_tip_window_ != nullptr && IsWindowVisible(status_tip_window_)) {
    ShowStatusTip(active_context_);
  }
}

void TsfTextService::ReloadCandidateWindowVisualSettings() {
  const std::wstring candidate_layout = ReadStringSetting(L"candidate_layout");
  if (candidate_layout == L"horizontal") {
    horizontal_candidate_layout_ = true;
  } else if (candidate_layout == L"vertical") {
    horizontal_candidate_layout_ = false;
  } else {
    horizontal_candidate_layout_ = ReadBoolSetting(L"candidate_horizontal", true);
  }
  compact_candidate_count_ = ReadIntSetting(L"candidate_count",
                                            kDefaultCompactCandidateCount,
                                            kMinCompactCandidateCount,
                                            kMaxCompactCandidateCount);
  candidate_font_size_level_ = ReadIntSetting(L"candidate_font_size_level",
                                              kDefaultCandidateFontSizeLevel,
                                              kMinCandidateFontSizeLevel,
                                              kMaxCandidateFontSizeLevel);
  candidate_font_family_ = NormalizeCandidateFontFamilySetting(
      ReadStringSetting(L"candidate_font_family", kDefaultCandidateFontFamily));
  const std::wstring legacy_theme =
      ReadStringSetting(fp::kLegacyThemeSetting, fp::kThemeModeDark);
  theme_mode_ =
      fp::NormalizeThemeModeSetting(ReadStringSetting(fp::kThemeModeSetting, legacy_theme));
  theme_preset_ =
      fp::NormalizeThemePresetSetting(ReadStringSetting(fp::kThemePresetSetting, legacy_theme));
}

void TsfTextService::PersistInputModeDefaults() const {
  WriteStringSetting(L"default_input_mode", ascii_mode_ ? L"en" : L"zh");
  WriteStringSetting(L"default_charset", simplified_charset_ ? L"simplified" : L"traditional");
  WriteBoolSetting(L"default_shape_half", !full_shape_mode_);
  WriteBoolSetting(L"default_chinese_punctuation", chinese_punctuation_mode_);
}

void TsfTextService::SaveCandidateLayoutSetting() const {
  WriteBoolSetting(L"candidate_horizontal", horizontal_candidate_layout_);
  WriteStringSetting(L"candidate_layout", horizontal_candidate_layout_ ? L"horizontal" : L"vertical");
}

void TsfTextService::SaveToolbarSetting() const {
  const auto path = SettingsPath();
  bool save_position_user = toolbar_position_user_;
  bool save_has_position = has_toolbar_position_;
  POINT save_position = toolbar_position_;
  if ((!save_position_user || !save_has_position) &&
      ReadBoolSetting(kToolbarPositionUserSetting, false)) {
    if (std::optional<POINT> saved_point = ReadPointSetting(kToolbarPositionSetting)) {
      save_position_user = true;
      save_has_position = true;
      save_position = *saved_point;
    }
  }

  std::lock_guard lock(SettingsCacheMutex());
  auto& cache = MutableSettingsCache();
  EnsureSettingsCacheLoadedLocked(cache);
  UpsertSettingLine(&cache.lines, kToolbarVisibleSetting, toolbar_visible_ ? L"1" : L"0");
  UpsertSettingLine(&cache.lines, kToolbarPositionUserSetting, save_position_user ? L"1" : L"0");
  UpsertSettingLine(&cache.lines,
                    kToolbarLayoutSetting,
                    toolbar_vertical_layout_ ? L"vertical" : L"horizontal");
  UpsertSettingLine(&cache.lines,
                    kToolbarItemsSetting,
                    SerializeToolbarVisibleItems(toolbar_visible_items_));
  UpsertSettingLine(&cache.lines, L"toolbar_visible", L"0");
  if (save_position_user && save_has_position) {
    UpsertSettingLine(&cache.lines,
                      kToolbarPositionSetting,
                      std::to_wstring(save_position.x) + L"," +
                          std::to_wstring(save_position.y));
  }
  FlushSettingLines(path, cache.lines);
  cache.path = path;
  cache.write_time = SettingsFileWriteTime(path);
  cache.loaded = true;
  RebuildSettingsIndex(cache);
}

void TsfTextService::SaveToolbarItemsSetting() const {
  SaveToolbarSetting();
  RequestToolbarHostRefresh();
}

void TsfTextService::ToggleToolbarItemVisibility(int item) {
  if (item == kToolbarItemNone) {
    toolbar_visible_items_ = DefaultToolbarVisibleItems();
  } else if (IsToolbarCustomItem(item)) {
    toolbar_visible_items_ = VisibleToolbarItemsWithSettings(toolbar_visible_items_);
    auto found = std::find(toolbar_visible_items_.begin(), toolbar_visible_items_.end(), item);
    if (found == toolbar_visible_items_.end()) {
      std::vector<int> updated;
      updated.reserve(kToolbarCustomItemDefinitions.size() + 1);
      for (const auto& definition : kToolbarCustomItemDefinitions) {
        if (definition.id == item || ContainsToolbarItem(toolbar_visible_items_, definition.id)) {
          updated.push_back(definition.id);
        }
      }
      updated.push_back(kToolbarItemSettings);
      toolbar_visible_items_ = std::move(updated);
    } else {
      toolbar_visible_items_.erase(found);
    }
    if (!ContainsToolbarItem(toolbar_visible_items_, kToolbarItemSettings)) {
      toolbar_visible_items_.push_back(kToolbarItemSettings);
    }
  }
  if (toolbar_window_ != nullptr) {
    PositionToolbarWindowKeepingCenter(false);
    RenderToolbarLayeredWindow();
    KeepToolbarWindowTopmost();
  } else {
    SaveToolbarSetting();
  }
  RequestToolbarHostRefresh();
}

void TsfTextService::ToggleToolbarLayout() {
  toolbar_vertical_layout_ = !toolbar_vertical_layout_;
  if (toolbar_window_ != nullptr) {
    PositionToolbarWindowKeepingCenter(false);
    RenderToolbarLayeredWindow();
    KeepToolbarWindowTopmost();
  } else {
    SaveToolbarSetting();
  }
  RequestToolbarHostRefresh();
}

int TsfTextService::CandidateFontPointSize() const noexcept {
  return CandidateFontPointSizeForLevel(candidate_font_size_level_);
}

ATOM TsfTextService::EnsureControlWindowClass() {
  static ATOM atom = 0;
  if (atom != 0) {
    return atom;
  }

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = StaticControlWindowProc;
  window_class.hInstance = g_module_instance;
  window_class.lpszClassName = L"FluentPinyinControlWindow";
  atom = RegisterClassExW(&window_class);
  return atom;
}

void TsfTextService::CreateControlWindow() {
  if (control_window_ != nullptr || EnsureControlWindowClass() == 0) {
    return;
  }

  control_window_ = CreateWindowExW(WS_EX_TOOLWINDOW,
                                    L"FluentPinyinControlWindow",
                                    L"",
                                    WS_POPUP,
                                    0,
                                    0,
                                    0,
                                    0,
                                    nullptr,
                                    nullptr,
                                    g_module_instance,
                                    this);
  if (control_window_ == nullptr) {
    fp::LogWarning(L"tsf", L"Failed to create FluentPinyin control window.");
  }
}

void TsfTextService::DestroyControlWindow() {
  if (control_window_ != nullptr) {
    DestroyWindow(control_window_);
    control_window_ = nullptr;
  }
}

LRESULT TsfTextService::ControlWindowProc(HWND window,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
  const UINT apply_input_config_message = ApplyInputConfigMessage();
  const UINT restart_input_core_message = RestartInputCoreMessage();
  if ((apply_input_config_message != 0 && message == apply_input_config_message) ||
      (restart_input_core_message != 0 && message == restart_input_core_message)) {
    fp::LogInfo(L"tsf", L"Settings requested input config apply; scheduling debounce.");
    if (control_window_ != nullptr) {
      KillTimer(control_window_, kControlInputCoreRestartTimer);
      SetTimer(control_window_, kControlInputCoreRestartTimer, 800, nullptr);
    } else {
      RestartRimeAndAlgorithmServiceAsync();
    }
    return 0;
  }
  if (message == ShutdownInputCoreMessage()) {
    ShutdownRimeForUninstall();
    return 0;
  }
  if (message == RefreshInputStateMessage() || message == LegacyRefreshInputStateMessage()) {
    fp::LogInfo(L"tsf", L"Settings requested input state refresh.");
    RefreshInputStateFromSettings();
    return 0;
  }
  if (message == WM_SETTINGCHANGE || message == WM_THEMECHANGED || message == WM_SYSCOLORCHANGE) {
    RefreshInputStateFromSettings();
    return 0;
  }
  if (message == ToolbarRefreshMessage()) {
    RefreshToolbarFromSettings();
    return 0;
  }
  if (message == WM_TIMER && wparam == kControlInputCoreRestartTimer) {
    KillTimer(control_window_, kControlInputCoreRestartTimer);
    RestartRimeAndAlgorithmServiceAsync();
    return 0;
  }
  if (message == WM_NCDESTROY) {
    if (control_window_ == window) {
      control_window_ = nullptr;
    }
    return 0;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TsfTextService::StaticControlWindowProc(HWND window,
                                                         UINT message,
                                                         WPARAM wparam,
                                                         LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window,
                      GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  auto* service =
      reinterpret_cast<TsfTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (service != nullptr) {
    return service->ControlWindowProc(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

bool TsfTextService::IsModifierShortcutActive() const {
  return IsVirtualKeyDown(VK_CONTROL) || IsVirtualKeyDown(VK_MENU) ||
         IsVirtualKeyDown(VK_LWIN) || IsVirtualKeyDown(VK_RWIN);
}

bool TsfTextService::IsKeyHandled(WPARAM wparam, LPARAM lparam) const {
  (void)lparam;
  const UINT toolbar_command = ToolbarShortcutCommand(wparam);
  if (toolbar_command != 0 && toolbar_command != kMenuInputMode) {
    return true;
  }
  if (toolbar_command == kMenuInputMode) {
    return true;
  }
  if (wparam == VK_SHIFT || wparam == VK_LSHIFT || wparam == VK_RSHIFT) {
    return false;
  }
  if (wparam == VK_CAPITAL) {
    return false;
  }

  if (IsComposing()) {
    if (CandidateShortcutForKey(wparam) != CandidateShortcutCommand::kNone) {
      return true;
    }
    if (IsModifierShortcutActive()) {
      return false;
    }
    if (wparam == VK_BACK || wparam == VK_ESCAPE || wparam == VK_RETURN ||
        wparam == VK_SPACE || wparam == VK_LEFT || wparam == VK_RIGHT ||
        wparam == VK_UP || wparam == VK_DOWN) {
      return true;
    }
    if (wparam >= '1' && wparam <= '9') {
      return true;
    }
    if (IsPrintableAsciiPunctuation(wparam)) {
      return true;
    }
  }

  if (IsModifierShortcutActive()) {
    return false;
  }

  if (ascii_mode_) {
    return false;
  }

  if (chinese_punctuation_mode_ && IsPrintableAsciiPunctuation(wparam)) {
    return true;
  }

  if (IsAlphabetVirtualKey(wparam)) {
    return true;
  }

  return false;
}

bool TsfTextService::HandleKey(ITfContext* context, WPARAM wparam, LPARAM lparam) {
  (void)lparam;
  if (context == nullptr) {
    return false;
  }
  if (input_mode_shortcut_down_ &&
      !ShortcutKeyEquals(input_mode_shortcut_key_, wparam)) {
    input_mode_shortcut_down_ = false;
    input_mode_shortcut_key_ = 0;
  }
  if (const UINT command = ToolbarShortcutCommand(wparam); command != 0) {
    if (command == kMenuInputMode) {
      if (IsShortcutModifierVirtualKey(wparam)) {
        input_mode_shortcut_down_ = true;
        input_mode_shortcut_key_ = wparam;
        shift_key_down_ = false;
        return true;
      }
      input_mode_shortcut_down_ = false;
      input_mode_shortcut_key_ = 0;
      shift_key_down_ = false;
      return ToggleAsciiModeFromKey(context, false);
    }
    input_mode_shortcut_down_ = false;
    input_mode_shortcut_key_ = 0;
    shift_key_down_ = false;
    HandleLangBarMenuCommand(command);
    return true;
  }
  if (IsModifierShortcutActive() && !IsComposing()) {
    shift_key_down_ = false;
    return false;
  }
  if (wparam == VK_SHIFT || wparam == VK_LSHIFT || wparam == VK_RSHIFT) {
    return false;
  }
  if (wparam == VK_CAPITAL) {
    return false;
  }

  if (active_context_ != context) {
    if (IsComposing()) {
      CancelComposition(active_context_);
    }
    if (active_context_ != nullptr) {
      active_context_->Release();
    }
    active_context_ = context;
    active_context_->AddRef();
  }

  if (ascii_mode_ && !IsComposing()) {
    return false;
  }
  if (!IsComposing() && IsCapsLockOn() && IsAlphabetVirtualKey(wparam)) {
    return false;
  }

  if (!IsComposing() && chinese_punctuation_mode_ && IsPrintableAsciiPunctuation(wparam)) {
    HideStatusTip();
    return CommitText(context, PunctuationForKey(wparam));
  }

  if (IsComposing()) {
    switch (CandidateShortcutForKey(wparam)) {
      case CandidateShortcutCommand::kExpand:
        return SetCandidateExpansion(context, !expanded_candidate_window_);
      case CandidateShortcutCommand::kPreviousPage:
        HideStatusTip();
        ChangeCandidatePage(context, -1);
        return true;
      case CandidateShortcutCommand::kNextPage:
        HideStatusTip();
        ChangeCandidatePage(context, 1);
        return true;
      case CandidateShortcutCommand::kNone:
        break;
    }
  }

  if (IsAlphabetVirtualKey(wparam)) {
    HideStatusTip();
    if (!composition_input_.empty() && composition_ == nullptr && preedit_length_ == 0) {
      const std::wstring recovered_preedit = AsciiToWide(composition_input_);
      if (UpdatePreedit(context, recovered_preedit)) {
        pending_candidate_continuation_ = false;
      }
    }
    composition_input_.push_back(AlphabetVirtualKeyToLowerAscii(wparam));
    candidate_page_index_ = 0;
    selected_candidate_index_ = 0;
    expanded_candidate_window_ = false;

    if (EnsureRimeReadyForKey()) {
      RefreshCandidates();
    } else {
      fp::LogWarning(L"tsf", L"Rime not ready for alphabet key; showing preedit without candidates.");
      candidates_.clear();
      has_previous_candidate_page_ = false;
      has_next_candidate_page_ = false;
      WarmUpRimeAsync(0);
    }
    const bool succeeded = UpdatePreedit(context, AsciiToWide(composition_input_));
    if (succeeded) {
      if (!candidates_.empty()) {
        ShowCandidateWindow(context);
      } else {
        HideCandidateWindow();
      }
    } else {
      fp::LogWarning(L"tsf", L"UpdatePreedit failed for alphabet key.");
      const bool initial_alphabet_key =
          composition_input_.size() == 1 && composition_ == nullptr && preedit_length_ == 0;
      ClearCompositionState();
      HideCandidateWindow();
      if (initial_alphabet_key) {
        return false;
      }
    }
    return true;
  }

  if (!IsComposing()) {
    return false;
  }

  if (IsModifierShortcutActive()) {
    shift_key_down_ = false;
    return false;
  }

  if (wparam == VK_BACK) {
    HideStatusTip();
    composition_input_.pop_back();
    candidate_page_index_ = 0;
    selected_candidate_index_ = 0;
    expanded_candidate_window_ = false;
    RefreshCandidates();
    if (composition_input_.empty()) {
      return CancelComposition(context);
    }
    const bool succeeded = UpdatePreedit(context, AsciiToWide(composition_input_));
    if (succeeded) {
      ShowCandidateWindow(context);
    } else {
      fp::LogWarning(L"tsf", L"UpdatePreedit failed after backspace.");
      ClearCompositionState();
      HideCandidateWindow();
    }
    return succeeded;
  }

  if (wparam == VK_ESCAPE) {
    HideStatusTip();
    return CancelComposition(context);
  }

  if (wparam == VK_RETURN) {
    HideStatusTip();
    return CommitText(context, AsciiToWide(composition_input_));
  }

  if (wparam == VK_SPACE) {
    HideStatusTip();
    if (!candidates_.empty()) {
      ClampCandidateSelection();
      return CommitCandidate(context, selected_candidate_index_);
    }
    return CommitText(context, AsciiToWide(composition_input_));
  }

  if (wparam == VK_LEFT || wparam == VK_RIGHT || wparam == VK_UP || wparam == VK_DOWN) {
    HideStatusTip();
    MoveCandidateSelection(wparam);
    if (candidate_window_ != nullptr) {
      RenderCandidateLayeredWindow();
    }
    return true;
  }

  if (wparam >= '1' && wparam <= '9') {
    HideStatusTip();
    const size_t digit_index = static_cast<size_t>(wparam - '1');
    const size_t index = CandidateIndexForDigit(digit_index);
    if (index < candidates_.size()) {
      return CommitCandidate(context, index);
    }
    return true;
  }

  if (IsPrintableAsciiPunctuation(wparam)) {
    HideStatusTip();
    std::wstring commit;
    if (!candidates_.empty()) {
      ClampCandidateSelection();
      commit = selected_candidate_index_ < candidates_.size()
                   ? candidates_[selected_candidate_index_].text
                   : AsciiToWide(composition_input_);
    } else {
      commit = AsciiToWide(composition_input_);
    }
    commit += chinese_punctuation_mode_ ? PunctuationForKey(wparam) : AsciiPunctuationForKey(wparam);
    return CommitText(context, commit);
  }

  return false;
}

bool TsfTextService::IsRimeReady() const {
  std::lock_guard<std::mutex> lock(rime_mutex_);
  return rime_ready_ && rime_ != nullptr;
}

bool TsfTextService::EnsureRimeReadyForKey() {
  {
    std::lock_guard<std::mutex> lock(rime_mutex_);
    if (rime_ready_ && rime_ != nullptr) {
      return true;
    }
    if (!service_active_) {
      return false;
    }
  }

  InitializeRime();
  return IsRimeReady();
}

void TsfTextService::WarmUpRimeAsync(DWORD delay_ms) {
  {
    std::lock_guard<std::mutex> lock(rime_mutex_);
    if (!service_active_ || rime_ready_ || rime_warmup_requested_) {
      return;
    }
    rime_warmup_requested_ = true;
  }

  AddRef();
  try {
    std::thread([this, delay_ms]() {
      try {
        const bool delayed_warmup = delay_ms > 0;
        BOOL background_mode = FALSE;
        if (delayed_warmup) {
          background_mode = SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
        }
        if (delayed_warmup && !background_mode) {
          SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        }
        if (delay_ms > 0) {
          Sleep(delay_ms);
        }
        bool should_warm_up = false;
        {
          std::lock_guard<std::mutex> lock(rime_mutex_);
          should_warm_up = service_active_;
        }
        if (should_warm_up) {
          InitializeRime();
        }
        if (background_mode) {
          SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
        }
      } catch (...) {
        fp::LogError(L"tsf", L"Rime warm-up failed with an unexpected exception.");
      }
      {
        std::lock_guard<std::mutex> lock(rime_mutex_);
        rime_warmup_requested_ = false;
      }
      Release();
    }).detach();
  } catch (...) {
    {
      std::lock_guard<std::mutex> lock(rime_mutex_);
      rime_warmup_requested_ = false;
    }
    Release();
    fp::LogError(L"tsf", L"Failed to start Rime warm-up thread.");
  }
}

void TsfTextService::InitializeRime() {
  const ULONGLONG init_start_tick = GetTickCount64();
  std::lock_guard<std::mutex> lock(rime_mutex_);
  if (rime_ready_ && rime_ != nullptr) {
    return;
  }

  auto engine = std::make_unique<fp::core::RimeEngine>();
  fp::core::RimeEngineOptions options;
  auto status = engine->Initialize(options);
  if (status.initialized) {
    rime_ = std::move(engine);
  }
  rime_ready_ = status.initialized;
  if (!rime_ready_) {
    fp::LogError(L"tsf", L"Rime init failed: " + status.message);
  } else {
    ApplyRimeOptionsLocked();
    fp::LogInfo(L"tsf",
                L"Rime init succeeded in " +
                    std::to_wstring(GetTickCount64() - init_start_tick) + L" ms: " +
                    rime_->shared_data_dir().wstring());
  }
}

void TsfTextService::UninitializeRime() {
  std::lock_guard<std::mutex> lock(rime_mutex_);
  if (rime_ != nullptr) {
    rime_->Shutdown();
    rime_.reset();
  }
  rime_ready_ = false;
}

void TsfTextService::ApplyRimeOptions() {
  std::lock_guard<std::mutex> lock(rime_mutex_);
  ApplyRimeOptionsLocked();
}

void TsfTextService::ApplyRimeOptionsLocked() {
  if (!rime_ready_ || rime_ == nullptr) {
    return;
  }

  rime_->SetOption("ascii_punct", !chinese_punctuation_mode_);
  rime_->SetOption("full_shape", full_shape_mode_);
  rime_->SetOption("s2s", false);
  rime_->SetOption("s2t", false);
  rime_->SetOption("s2hk", false);
  rime_->SetOption("s2tw", false);
  rime_->SetOption(simplified_charset_ ? "s2s" : "s2t", true);
  rime_->SetOption("abbrev", super_abbrev_enabled_);
  rime_->ResetComposition();
}

void TsfTextService::RefreshCandidates() {
  const ULONGLONG refresh_start_tick = GetTickCount64();
  InvalidateCandidateLayoutCache();
  const int requested_page_size = CandidatePageSizeLimit(horizontal_candidate_layout_,
                                                         expanded_candidate_window_,
                                                         compact_candidate_count_);
  int page_size = requested_page_size;
  {
    std::lock_guard<std::mutex> lock(rime_mutex_);
    if (rime_ready_ && rime_ != nullptr && !composition_input_.empty() &&
        last_candidate_query_input_ == composition_input_ &&
        last_candidate_query_page_index_ == candidate_page_index_ &&
        last_candidate_query_page_size_ == page_size) {
      ClampCandidateSelection();
      return;
    }
  }

  candidates_.clear();
  has_previous_candidate_page_ = false;
  has_next_candidate_page_ = false;
  if (!composition_input_.empty()) {
    std::lock_guard<std::mutex> lock(rime_mutex_);
    if (!rime_ready_ || rime_ == nullptr) {
      last_candidate_query_input_.clear();
      last_candidate_query_page_index_ = -1;
      last_candidate_query_page_size_ = 0;
      ClampCandidateSelection();
      return;
    }
    const auto page =
        rime_->GetCandidatePageForInput(composition_input_,
                                        candidate_page_index_,
                                        page_size);
    candidates_ = page.candidates;
    has_previous_candidate_page_ = page.has_previous_page;
    has_next_candidate_page_ = page.has_next_page;
    const size_t visible_count =
        CalculateVisibleCandidateCountForWindow(candidate_window_,
                                                horizontal_candidate_layout_,
                                                expanded_candidate_window_,
                                                compact_candidate_count_,
                                                CandidateFontPointSize(),
                                                simplified_charset_,
                                                CandidateFontFamilyFromSetting(candidate_font_family_),
                                                candidates_);
    if (visible_count > 0 && visible_count < candidates_.size() &&
        candidate_page_index_ == 0) {
      page_size = static_cast<int>(visible_count);
      candidates_.resize(visible_count);
      has_next_candidate_page_ = true;
    } else if (visible_count > 0 && visible_count < candidates_.size()) {
      page_size = static_cast<int>(visible_count);
      const auto visible_page =
          rime_->GetCandidatePageForInput(composition_input_,
                                          candidate_page_index_,
                                          page_size);
      candidates_ = visible_page.candidates;
      has_previous_candidate_page_ = visible_page.has_previous_page;
      has_next_candidate_page_ = visible_page.has_next_page;
    }
    last_candidate_query_input_ = composition_input_;
    last_candidate_query_page_index_ = candidate_page_index_;
    last_candidate_query_page_size_ = page_size;
  } else {
    last_candidate_query_input_.clear();
    last_candidate_query_page_index_ = -1;
    last_candidate_query_page_size_ = 0;
  }
  ClampCandidateSelection();
  LogTsfPerfIfSlow(L"RefreshCandidates",
                   GetTickCount64() - refresh_start_tick,
                   20,
                   L"input_len=" + std::to_wstring(composition_input_.size()) +
                       L", requested_page_size=" + std::to_wstring(requested_page_size) +
                       L", final_page_size=" + std::to_wstring(page_size) +
                       L", candidates=" + std::to_wstring(candidates_.size()) +
                       L", expanded=" + (expanded_candidate_window_ ? std::wstring(L"yes")
                                                                     : std::wstring(L"no")));
}

size_t TsfTextService::VisibleCandidateCount() const {
  return SelectableCandidateIndicesForWindow(candidate_window_,
                                             horizontal_candidate_layout_,
                                             expanded_candidate_window_,
                                             compact_candidate_count_,
                                             CandidateFontPointSize(),
                                             simplified_charset_,
                                             CandidateFontFamilyFromSetting(candidate_font_family_),
                                             candidates_)
      .size();
}

int TsfTextService::CandidateNavigationColumns() const {
  if (!expanded_candidate_window_) {
    return 1;
  }
  const size_t visible_count = VisibleCandidateCount();
  if (visible_count == 0) {
    return 1;
  }
  if (candidate_window_ != nullptr) {
    const CandidateLayoutMetrics layout =
        CandidateLayoutForWindow(candidate_window_,
                                 horizontal_candidate_layout_,
                                 expanded_candidate_window_,
                                 compact_candidate_count_,
                                 CandidateFontPointSize(),
                                 simplified_charset_,
                                 CandidateFontFamilyFromSetting(candidate_font_family_),
                                 candidates_);
    if (layout.expanded_columns > 0) {
      return layout.expanded_columns;
    }
  }
  return std::max(1,
                  std::min(ExpandedCandidateColumnCount(horizontal_candidate_layout_,
                                                        compact_candidate_count_),
                           static_cast<int>(visible_count)));
}

size_t TsfTextService::CandidateIndexForDigit(size_t digit_index) const {
  CandidateLayoutMetrics layout;
  const std::vector<size_t> selectable_indices =
      SelectableCandidateIndicesForWindow(candidate_window_,
                                          horizontal_candidate_layout_,
                                          expanded_candidate_window_,
                                          compact_candidate_count_,
                                          CandidateFontPointSize(),
                                          simplified_charset_,
                                          CandidateFontFamilyFromSetting(candidate_font_family_),
                                          candidates_,
                                          &layout);
  if (digit_index >= kMaxCompactCandidateCount || selectable_indices.empty()) {
    return candidates_.size();
  }
  if (!expanded_candidate_window_) {
    return digit_index < selectable_indices.size() ? selectable_indices[digit_index]
                                                   : candidates_.size();
  }

  const auto active_iter =
      std::find(selectable_indices.begin(), selectable_indices.end(), selected_candidate_index_);
  const size_t active_index =
      active_iter == selectable_indices.end() ? selectable_indices.front() : selected_candidate_index_;

  if (active_index < layout.candidate_rows.size() &&
      active_index < layout.candidate_columns.size() &&
      layout.candidate_rows.size() == layout.candidate_columns.size()) {
    const int active_row = layout.candidate_rows[active_index];
    const int active_column = layout.candidate_columns[active_index];
    for (const size_t index : selectable_indices) {
      if (index >= layout.candidate_rows.size() || index >= layout.candidate_columns.size()) {
        continue;
      }
      const bool target =
          horizontal_candidate_layout_
              ? (layout.candidate_rows[index] == active_row &&
                 layout.candidate_columns[index] == static_cast<int>(digit_index))
              : (layout.candidate_columns[index] == active_column &&
                 layout.candidate_rows[index] == static_cast<int>(digit_index));
      if (target) {
        return index;
      }
    }
    return candidates_.size();
  }

  const size_t columns = static_cast<size_t>(std::max(1, CandidateNavigationColumns()));
  const size_t selected_position =
      active_iter == selectable_indices.end()
          ? 0
          : static_cast<size_t>(std::distance(selectable_indices.begin(), active_iter));
  const size_t row = selected_position / columns;
  const size_t target_position = row * columns + digit_index;
  return target_position < selectable_indices.size() ? selectable_indices[target_position]
                                                     : candidates_.size();
}

void TsfTextService::ClampCandidateSelection() {
  const std::vector<size_t> selectable_indices =
      SelectableCandidateIndicesForWindow(candidate_window_,
                                          horizontal_candidate_layout_,
                                          expanded_candidate_window_,
                                          compact_candidate_count_,
                                          CandidateFontPointSize(),
                                          simplified_charset_,
                                          CandidateFontFamilyFromSetting(candidate_font_family_),
                                          candidates_);
  if (selectable_indices.empty()) {
    selected_candidate_index_ = 0;
    return;
  }
  if (std::find(selectable_indices.begin(), selectable_indices.end(), selected_candidate_index_) ==
      selectable_indices.end()) {
    selected_candidate_index_ = selectable_indices.front();
  }
}

bool TsfTextService::MoveCandidateSelection(WPARAM wparam) {
  CandidateLayoutMetrics layout;
  const std::vector<size_t> selectable_indices =
      SelectableCandidateIndicesForWindow(candidate_window_,
                                          horizontal_candidate_layout_,
                                          expanded_candidate_window_,
                                          compact_candidate_count_,
                                          CandidateFontPointSize(),
                                          simplified_charset_,
                                          CandidateFontFamilyFromSetting(candidate_font_family_),
                                          candidates_,
                                          &layout);
  if (selectable_indices.empty()) {
    selected_candidate_index_ = 0;
    return false;
  }

  ClampCandidateSelection();
  size_t next_index = selected_candidate_index_;
  if (expanded_candidate_window_) {
    bool used_layout_navigation = false;
    if (selected_candidate_index_ < layout.candidate_rows.size() &&
        layout.candidate_rows.size() == layout.candidate_columns.size()) {
      const int row = layout.candidate_rows[selected_candidate_index_];
      const int column = layout.candidate_columns[selected_candidate_index_];
      auto find_at = [&](int target_row, int target_column) -> size_t {
        size_t best = candidates_.size();
        int best_distance = INT_MAX;
        for (const size_t index : selectable_indices) {
          if (index >= layout.candidate_rows.size() || index >= layout.candidate_columns.size()) {
            continue;
          }
          const bool same_axis =
              horizontal_candidate_layout_
                  ? layout.candidate_rows[index] == target_row
                  : layout.candidate_columns[index] == target_column;
          if (!same_axis) {
            continue;
          }
          const int distance =
              horizontal_candidate_layout_
                  ? std::abs(layout.candidate_columns[index] - target_column)
                  : std::abs(layout.candidate_rows[index] - target_row);
          if (distance < best_distance) {
            best = index;
            best_distance = distance;
          }
        }
        return best;
      };
      switch (wparam) {
        case VK_LEFT:
          if (horizontal_candidate_layout_) {
            for (auto iter = std::find(selectable_indices.begin(),
                                        selectable_indices.end(),
                                        selected_candidate_index_);
                 iter != selectable_indices.begin() && iter != selectable_indices.end();) {
              --iter;
              if (*iter < layout.candidate_rows.size() && layout.candidate_rows[*iter] == row) {
                next_index = *iter;
                break;
              }
            }
          } else {
            const size_t target = find_at(row, column - 1);
            if (target < candidates_.size()) {
              next_index = target;
            }
          }
          break;
        case VK_RIGHT:
          if (horizontal_candidate_layout_) {
            const auto selected_iter = std::find(selectable_indices.begin(),
                                                 selectable_indices.end(),
                                                 selected_candidate_index_);
            if (selected_iter != selectable_indices.end()) {
              for (auto iter = selected_iter + 1; iter != selectable_indices.end(); ++iter) {
                if (*iter < layout.candidate_rows.size() && layout.candidate_rows[*iter] == row) {
                  next_index = *iter;
                  break;
                }
              }
            }
          } else {
            const size_t target = find_at(row, column + 1);
            if (target < candidates_.size()) {
              next_index = target;
            }
          }
          break;
        case VK_UP: {
          const size_t target = find_at(row - 1, column);
          if (target < candidates_.size()) {
            next_index = target;
          }
          break;
        }
        case VK_DOWN: {
          const size_t target = find_at(row + 1, column);
          if (target < candidates_.size()) {
            next_index = target;
          }
          break;
        }
        default:
          break;
      }
      used_layout_navigation = true;
    }
    if (!used_layout_navigation) {
      const int columns = CandidateNavigationColumns();
      const auto selected_iter =
          std::find(selectable_indices.begin(), selectable_indices.end(), selected_candidate_index_);
      const size_t selected_position =
          selected_iter == selectable_indices.end()
              ? 0
              : static_cast<size_t>(std::distance(selectable_indices.begin(), selected_iter));
      size_t target_position = selected_position;
      switch (wparam) {
        case VK_LEFT:
          if (target_position > 0) {
            --target_position;
          }
          break;
        case VK_RIGHT:
          if (target_position + 1 < selectable_indices.size()) {
            ++target_position;
          }
          break;
        case VK_UP:
          if (target_position >= static_cast<size_t>(columns)) {
            target_position -= static_cast<size_t>(columns);
          }
          break;
        case VK_DOWN:
          if (target_position + static_cast<size_t>(columns) < selectable_indices.size()) {
            target_position += static_cast<size_t>(columns);
          }
          break;
        default:
          break;
      }
      next_index = selectable_indices[target_position];
    }
  } else if (horizontal_candidate_layout_) {
    const auto selected_iter =
        std::find(selectable_indices.begin(), selectable_indices.end(), selected_candidate_index_);
    size_t selected_position =
        selected_iter == selectable_indices.end()
            ? 0
            : static_cast<size_t>(std::distance(selectable_indices.begin(), selected_iter));
    if ((wparam == VK_DOWN || wparam == VK_RIGHT) &&
        selected_position + 1 < selectable_indices.size()) {
      ++selected_position;
    } else if ((wparam == VK_UP || wparam == VK_LEFT) && selected_position > 0) {
      --selected_position;
    }
    next_index = selectable_indices[selected_position];
  } else {
    const auto selected_iter =
        std::find(selectable_indices.begin(), selectable_indices.end(), selected_candidate_index_);
    size_t selected_position =
        selected_iter == selectable_indices.end()
            ? 0
            : static_cast<size_t>(std::distance(selectable_indices.begin(), selected_iter));
    if (wparam == VK_DOWN && selected_position + 1 < selectable_indices.size()) {
      ++selected_position;
    } else if (wparam == VK_UP && selected_position > 0) {
      --selected_position;
    }
    next_index = selectable_indices[selected_position];
  }

  const bool changed = next_index != selected_candidate_index_;
  selected_candidate_index_ = next_index;
  return changed;
}

bool TsfTextService::SetCandidateExpansion(ITfContext* context, bool expanded) {
  const ULONGLONG expansion_start_tick = GetTickCount64();
  HideStatusTip();
  if (!IsComposing()) {
    return true;
  }
  if (expanded_candidate_window_ == expanded) {
    ShowCandidateWindow(context);
    fp::LogInfo(L"tsf",
                std::wstring(L"Candidate expansion unchanged; show completed in ") +
                    std::to_wstring(GetTickCount64() - expansion_start_tick) + L" ms.");
    return true;
  }
  expanded_candidate_window_ = expanded;
  InvalidateCandidateLayoutCache();
  candidate_page_index_ = 0;
  selected_candidate_index_ = 0;
  RefreshCandidates();
  ShowCandidateWindow(context);
  fp::LogInfo(L"tsf",
              std::wstring(expanded ? L"Candidate expand" : L"Candidate collapse") +
                  L" completed in " +
                  std::to_wstring(GetTickCount64() - expansion_start_tick) +
                  L" ms with " + std::to_wstring(candidates_.size()) + L" candidates.");
  return true;
}

bool TsfTextService::IsCurrentKeyboardProfile() const {
  ITfInputProcessorProfileMgr* profile_mgr = nullptr;
  HRESULT result = CoCreateInstance(CLSID_TF_InputProcessorProfiles,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_ITfInputProcessorProfileMgr,
                                    reinterpret_cast<void**>(&profile_mgr));
  if (FAILED(result) || profile_mgr == nullptr) {
    return true;
  }

  TF_INPUTPROCESSORPROFILE profile{};
  result = profile_mgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &profile);
  profile_mgr->Release();
  if (FAILED(result)) {
    return true;
  }

  return IsEqualCLSID(profile.clsid, kTextServiceClsid) &&
         IsEqualGUID(profile.guidProfile, kProfileGuid);
}

bool TsfTextService::ShouldHostToolbarWindow() const {
  if (toolbar_host_process_ || !service_active_) {
    return false;
  }
  return IsCurrentKeyboardProfile();
}

void TsfTextService::ChangeCandidatePage(ITfContext* context, int delta) {
  if (!IsComposing() || delta == 0) {
    return;
  }

  if (delta > 0 && !has_next_candidate_page_) {
    return;
  }
  if (delta < 0 && candidate_page_index_ == 0) {
    return;
  }

  candidate_page_index_ += delta;
  if (candidate_page_index_ < 0) {
    candidate_page_index_ = 0;
  }
  selected_candidate_index_ = 0;
  InvalidateCandidateLayoutCache();
  RefreshCandidates();
  ShowCandidateWindow(context);
}

bool TsfTextService::UpdatePreedit(ITfContext* context, const std::wstring& text) {
  if (context == nullptr) {
    return false;
  }

  const LONG old_length = preedit_length_;
  const bool succeeded = RequestCompositionEdit(client_id_,
                                                context,
                                                static_cast<ITfCompositionSink*>(this),
                                                &composition_,
                                                display_attribute_input_atom_,
                                                text,
                                                CompositionEditAction::kUpdate);
  if (succeeded || RequestTextReplacement(client_id_, context, old_length, text)) {
    preedit_length_ = static_cast<LONG>(text.size());
    return true;
  }
  return false;
}

bool TsfTextService::CommitText(ITfContext* context, const std::wstring& text) {
  const LONG old_length = preedit_length_;
  bool succeeded = false;
  if (composition_ != nullptr) {
    succeeded = RequestCompositionEdit(client_id_,
                                       context,
                                       static_cast<ITfCompositionSink*>(this),
                                       &composition_,
                                       display_attribute_input_atom_,
                                       text,
                                       CompositionEditAction::kCommit);
  }
  if (!succeeded) {
    succeeded = RequestTextReplacement(client_id_, context, old_length, text);
  }
  if (succeeded) {
    ClearCompositionState();
    HideCandidateWindow();
  }
  return succeeded;
}

bool TsfTextService::CommitTextAndContinueComposition(ITfContext* context,
                                                      const std::wstring& commit_text,
                                                      const std::string& remaining_input,
                                                      const std::wstring& preedit_text) {
  if (context == nullptr || commit_text.empty() || preedit_text.empty()) {
    return false;
  }

  pending_candidate_continuation_ = true;
  auto keep_remaining_composition = [&]() {
    composition_input_ = remaining_input;
    preedit_length_ = composition_ != nullptr ? static_cast<LONG>(preedit_text.size()) : 0;
    candidate_page_index_ = 0;
    selected_candidate_index_ = 0;
    expanded_candidate_window_ = false;
    last_candidate_query_input_.clear();
    last_candidate_query_page_index_ = -1;
    last_candidate_query_page_size_ = 0;
    RefreshCandidates();
    if (!candidates_.empty()) {
      ShowCandidateWindow(context);
    } else {
      HideCandidateWindow();
    }
  };

  bool succeeded = false;
  bool committed = false;
  const bool attempted_composition_restart = composition_ != nullptr;
  if (attempted_composition_restart) {
    suppress_next_composition_termination_ = true;
    succeeded = RequestCompositionCommitAndRestart(client_id_,
                                                   context,
                                                   static_cast<ITfCompositionSink*>(this),
                                                   &composition_,
                                                   display_attribute_input_atom_,
                                                   commit_text,
                                                   preedit_text,
                                                   &committed);
    suppress_next_composition_termination_ = false;
  } else {
    const LONG old_length = preedit_length_;
    committed = RequestTextReplacement(client_id_, context, old_length, commit_text);
    if (committed) {
      preedit_length_ = 0;
      succeeded = RequestCompositionEdit(client_id_,
                                         context,
                                         static_cast<ITfCompositionSink*>(this),
                                         &composition_,
                                         display_attribute_input_atom_,
                                         preedit_text,
                                         CompositionEditAction::kUpdate);
    }
  }
  if (committed && !succeeded && composition_ == nullptr) {
    succeeded = RequestCompositionEdit(client_id_,
                                       context,
                                       static_cast<ITfCompositionSink*>(this),
                                       &composition_,
                                       display_attribute_input_atom_,
                                       preedit_text,
                                       CompositionEditAction::kUpdate);
  }
  if (committed && !succeeded) {
    keep_remaining_composition();
    return true;
  }
  if (succeeded) {
    keep_remaining_composition();
    pending_candidate_continuation_ = false;
  }
  return succeeded;
}

bool TsfTextService::CommitCandidate(ITfContext* context, size_t candidate_index) {
  if (candidate_index >= candidates_.size()) {
    return false;
  }
  const std::vector<size_t> selectable_indices =
      SelectableCandidateIndicesForWindow(candidate_window_,
                                          horizontal_candidate_layout_,
                                          expanded_candidate_window_,
                                          compact_candidate_count_,
                                          CandidateFontPointSize(),
                                          simplified_charset_,
                                          CandidateFontFamilyFromSetting(candidate_font_family_),
                                          candidates_);
  if (!selectable_indices.empty() &&
      std::find(selectable_indices.begin(), selectable_indices.end(), candidate_index) ==
          selectable_indices.end()) {
    return false;
  }
  const std::wstring fallback_text = candidates_[candidate_index].text;
  fp::core::RimeCandidateCommit commit;
  {
    std::lock_guard<std::mutex> lock(rime_mutex_);
    if (rime_ready_ && rime_ != nullptr) {
      const int fallback_page_size = CandidatePageSizeLimit(horizontal_candidate_layout_,
                                                            expanded_candidate_window_,
                                                            compact_candidate_count_);
      const bool last_query_matches_current_page =
          last_candidate_query_input_ == composition_input_ &&
          last_candidate_query_page_index_ == candidate_page_index_ &&
          last_candidate_query_page_size_ > 0;
      const int effective_page_size =
          last_query_matches_current_page ? last_candidate_query_page_size_ : fallback_page_size;
      commit = rime_->SelectCandidateForInput(composition_input_,
                                              candidate_page_index_,
                                              effective_page_size,
                                              candidate_index);
    }
  }

  if (commit.handled && !commit.text.empty()) {
    if (!commit.remaining_input.empty()) {
      composition_input_ = commit.remaining_input;
      const std::wstring remaining_preedit =
          commit.remaining_composition.empty() ? AsciiToWide(commit.remaining_input)
                                               : commit.remaining_composition;
      return CommitTextAndContinueComposition(context,
                                              commit.text,
                                              commit.remaining_input,
                                              remaining_preedit);
    }
    return CommitText(context, commit.text);
  }

  return CommitText(context, fallback_text);
}

bool TsfTextService::CommitCandidateFromMouse(size_t candidate_index) {
  if (active_context_ == nullptr || candidate_index >= candidates_.size()) {
    return false;
  }
  return CommitCandidate(active_context_, candidate_index);
}

bool TsfTextService::CancelComposition(ITfContext* context) {
  bool succeeded = true;
  if (context != nullptr && (preedit_length_ > 0 || composition_ != nullptr)) {
    if (composition_ != nullptr) {
      succeeded = RequestCompositionEdit(client_id_,
                                         context,
                                         static_cast<ITfCompositionSink*>(this),
                                         &composition_,
                                         display_attribute_input_atom_,
                                         L"",
                                         CompositionEditAction::kCancel);
    }
    if (!succeeded && preedit_length_ > 0) {
      succeeded = RequestTextReplacement(client_id_, context, preedit_length_, L"");
    }
  }
  ClearCompositionState();
  HideCandidateWindow();
  return succeeded;
}

void TsfTextService::ClearCompositionState() {
  composition_input_.clear();
  preedit_length_ = 0;
  pending_candidate_continuation_ = false;
  candidate_page_index_ = 0;
  has_previous_candidate_page_ = false;
  has_next_candidate_page_ = false;
  candidates_.clear();
  selected_candidate_index_ = 0;
  last_candidate_query_input_.clear();
  last_candidate_query_page_index_ = -1;
  last_candidate_query_page_size_ = 0;
  {
    std::lock_guard<std::mutex> lock(rime_mutex_);
    if (rime_ != nullptr) {
      rime_->ResetComposition();
    }
  }
  has_last_candidate_anchor_ = false;
  expanded_candidate_window_ = false;
}

void TsfTextService::HandleLangBarMenuCommand(UINT command_id) {
  bool status_changed = false;
  bool charset_changed = false;
  bool input_defaults_changed = false;
  StatusTipDetail status_detail = StatusTipDetail::kNone;
  switch (command_id) {
    case kMenuFullShape:
      full_shape_mode_ = !full_shape_mode_;
      input_defaults_changed = true;
      status_tip_icon_mode_ = full_shape_mode_ ? StatusTipIconMode::kFullShape
                                               : StatusTipIconMode::kHalfShape;
      status_changed = true;
      status_detail =
          full_shape_mode_ ? StatusTipDetail::kFullShape : StatusTipDetail::kHalfShape;
      break;
    case kMenuFullShapeFull:
      status_changed = !full_shape_mode_;
      full_shape_mode_ = true;
      input_defaults_changed = status_changed;
      status_tip_icon_mode_ = StatusTipIconMode::kFullShape;
      status_detail = StatusTipDetail::kFullShape;
      break;
    case kMenuPunctuation:
      chinese_punctuation_mode_ = !chinese_punctuation_mode_;
      input_defaults_changed = true;
      status_tip_icon_mode_ = chinese_punctuation_mode_ ? StatusTipIconMode::kChinesePunctuation
                                                        : StatusTipIconMode::kEnglishPunctuation;
      status_changed = true;
      status_detail = chinese_punctuation_mode_ ? StatusTipDetail::kChinesePunctuation
                                                : StatusTipDetail::kEnglishPunctuation;
      break;
    case kMenuPunctuationChinese:
      status_changed = !chinese_punctuation_mode_;
      chinese_punctuation_mode_ = true;
      input_defaults_changed = status_changed;
      status_tip_icon_mode_ = StatusTipIconMode::kChinesePunctuation;
      status_detail = StatusTipDetail::kChinesePunctuation;
      break;
    case kMenuPunctuationEnglish:
      status_changed = chinese_punctuation_mode_;
      chinese_punctuation_mode_ = false;
      input_defaults_changed = status_changed;
      status_tip_icon_mode_ = StatusTipIconMode::kEnglishPunctuation;
      status_detail = StatusTipDetail::kEnglishPunctuation;
      break;
    case kMenuCharset:
      simplified_charset_ = !simplified_charset_;
      status_changed = true;
      charset_changed = true;
      input_defaults_changed = true;
      break;
    case kMenuFullShapeHalf:
      status_changed = full_shape_mode_;
      full_shape_mode_ = false;
      input_defaults_changed = status_changed;
      status_tip_icon_mode_ = StatusTipIconMode::kHalfShape;
      status_detail = StatusTipDetail::kHalfShape;
      break;
    case kMenuCharsetSimplified:
      status_changed = !simplified_charset_;
      charset_changed = status_changed;
      simplified_charset_ = true;
      input_defaults_changed = status_changed;
      break;
    case kMenuCharsetTraditional:
      status_changed = simplified_charset_;
      charset_changed = status_changed;
      simplified_charset_ = false;
      input_defaults_changed = status_changed;
      break;
    case kMenuCandidateLayoutHorizontal:
      horizontal_candidate_layout_ = true;
      SaveCandidateLayoutSetting();
      ShowCandidateWindow(active_context_);
      break;
    case kMenuCandidateLayoutVertical:
      horizontal_candidate_layout_ = false;
      SaveCandidateLayoutSetting();
      ShowCandidateWindow(active_context_);
      break;
    case kMenuToolbar:
      ToggleToolbarWindow();
      break;
    case kMenuSettings:
      OpenConfigApp();
      break;
    case kMenuCustomPhrases:
      OpenConfigApp(L"phrases");
      break;
    case kMenuLexiconManagement:
      OpenConfigApp(L"lexicon-management");
      break;
    case kMenuKeyConfig:
      OpenConfigApp(L"hotkeys");
      break;
    case kMenuRedeploy:
      RestartRimeAndAlgorithmService();
      break;
    case kMenuEmoji:
      OpenEmojiPanel();
      break;
    case kMenuAbout:
      OpenConfigApp(L"about");
      break;
    default:
      break;
  }

  if (input_defaults_changed) {
    PersistInputModeDefaults();
    RefreshToolbarHostIfVisible(toolbar_visible_);
  }

  if (charset_changed || input_defaults_changed) {
    ApplyRimeOptions();
  }
  if (charset_changed) {
    candidate_page_index_ = 0;
    selected_candidate_index_ = 0;
    last_candidate_query_input_.clear();
    last_candidate_query_page_index_ = -1;
    last_candidate_query_page_size_ = 0;
    if (IsComposing()) {
      RefreshCandidates();
      ShowCandidateWindow(active_context_);
    }
    NotifyInputModeChanged();
  }

  if (toolbar_window_ != nullptr) {
    RenderToolbarLayeredWindow();
  }
  if (status_changed) {
    ShowStatusTip(active_context_, status_detail);
  }
}

void TsfTextService::ShowContextMenu(POINT point) {
  ShowContextMenu(point, false);
}

void TsfTextService::ShowToolbarGearMenu(POINT point) {
  ShowContextMenu(point, true);
}

void TsfTextService::ShowContextMenu(POINT point, bool toolbar_mode) {
  if (!IsUsableScreenPoint(point)) {
    GetCursorPos(&point);
  }

  DestroyContextMenu();
  context_menu_toolbar_mode_ = toolbar_mode;
  if (EnsureContextMenuWindowClass() == 0) {
    return;
  }

  HWND dpi_window = toolbar_window_ != nullptr ? toolbar_window_ : candidate_window_;
  if (dpi_window == nullptr) {
    dpi_window = WindowFromPoint(point);
  }
  if (dpi_window == nullptr) {
    dpi_window = GetForegroundWindow();
  }
  if (dpi_window == nullptr) {
    dpi_window = GetDesktopWindow();
  }

  const UINT dpi = ReadableDpiForWindow(dpi_window);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const int width = s(ContextMenuWidthDips(toolbar_mode));
  const int row_height = ScaleHalfDipForDpi(kContextMenuRowHeightHalfDips, dpi);
  const int separator_width = HairlineForDpi(dpi);
  int separator_count = 0;
  for (int row = 0; row < ContextMenuRowCount(toolbar_mode); ++row) {
    if (ContextMenuHasSeparatorAfter(row, toolbar_mode)) {
      ++separator_count;
    }
  }
  const int height = 2 * separator_width + ContextMenuRowCount(toolbar_mode) * row_height +
                     separator_count * separator_width;
  if (toolbar_mode && toolbar_window_ != nullptr) {
    RECT toolbar_rect{};
    if (GetWindowRect(toolbar_window_, &toolbar_rect)) {
      point.y = toolbar_rect.bottom - separator_width;
    }
  }

  HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitor_info{};
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfoW(monitor, &monitor_info)) {
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &monitor_info.rcWork, 0);
  }
  const RECT work_area = monitor_info.rcWork;
  const int margin = s(kContextMenuScreenMarginDips);
  const bool taskbar_anchor = point.y >= work_area.bottom - s(80) || point.y > work_area.bottom;
  int x = taskbar_anchor ? point.x - width / 2 : point.x;
  int y = taskbar_anchor ? work_area.bottom - s(kContextMenuTaskbarGapDips) - height : point.y;
  x = std::clamp(x,
                 static_cast<int>(work_area.left) + margin,
                 static_cast<int>(work_area.right) - margin - width);
  y = std::clamp(y,
                 static_cast<int>(work_area.top) + margin,
                 static_cast<int>(work_area.bottom) - margin - height);

  context_menu_window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST |
                                             WS_EX_NOACTIVATE,
                                         L"FluentPinyinContextMenuWindow",
                                         L"",
                                         WS_POPUP,
                                         x,
                                         y,
                                         width,
                                         height,
                                         nullptr,
                                         nullptr,
                                         g_module_instance,
                                         this);
  if (context_menu_window_ == nullptr) {
    return;
  }
  hovered_context_menu_row_ = -1;
  context_menu_mouse_tracking_ = false;
  RenderContextMenuLayeredWindow();
  ShowWindow(context_menu_window_, SW_SHOWNOACTIVATE);
  SetWindowPos(context_menu_window_,
               HWND_TOPMOST,
               x,
               y,
               width,
               height,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  SetCapture(context_menu_window_);
  PostMessageW(dpi_window, WM_NULL, 0, 0);
}

void TsfTextService::HideContextMenu() {
  HideContextSubmenu();
  if (context_menu_window_ != nullptr) {
    ShowWindow(context_menu_window_, SW_HIDE);
  }
  hovered_context_menu_row_ = -1;
  context_menu_mouse_tracking_ = false;
}

void TsfTextService::DestroyContextMenu() {
  DestroyContextSubmenu();
  if (context_menu_window_ != nullptr) {
    if (GetCapture() == context_menu_window_) {
      ReleaseCapture();
    }
    DestroyWindow(context_menu_window_);
    context_menu_window_ = nullptr;
  }
  hovered_context_menu_row_ = -1;
  context_menu_mouse_tracking_ = false;
}

void TsfTextService::DrawContextMenu(HDC dc) {
  RECT client{};
  GetClientRect(context_menu_window_, &client);
  const UINT dpi = ReadableDpiForWindow(context_menu_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const int border = FloatingWindowBorderWidth(dpi);
  const int row_height = ScaleHalfDipForDpi(kContextMenuRowHeightHalfDips, dpi);
  const int corner_radius = s(kContextMenuCornerRadiusDips);
  const ToolbarPalette palette = ToolbarPaletteForTheme(theme_mode_, theme_preset_);
  const COLORREF background = palette.context_background;
  const COLORREF edge_color = palette.border;
  const COLORREF separator_color = palette.context_separator;
  const COLORREF text_color = palette.context_text;
  const COLORREF icon_color = text_color;
  const COLORREF hover_color = palette.context_hover;

  RECT body = DrawFloatingWindowSurface(dc, client, corner_radius, dpi, edge_color, background);

  auto separator_count_before = [&](int row) {
    int count = 0;
    for (int previous = 0; previous < row; ++previous) {
      if (ContextMenuHasSeparatorAfter(previous, context_menu_toolbar_mode_)) {
        ++count;
      }
    }
    return count;
  };
  auto row_top = [&](int row) {
    return border + row * row_height + separator_count_before(row) * border;
  };
  auto menu_text = [&](int row) -> std::wstring {
    if (context_menu_toolbar_mode_) {
      switch (row) {
        case kToolbarGearMenuRowCustom:
          return L"\u81EA\u5B9A\u4E49";
        case kToolbarGearMenuRowLayout:
          return L"\u6C34\u5E73/\u5782\u76F4";
        case kToolbarGearMenuRowHide:
          return L"\u9690\u85CF\u8F93\u5165\u6CD5\u5DE5\u5177\u680F";
        case kToolbarGearMenuRowSettings:
          return L"\u8F93\u5165\u6CD5\u8BBE\u7F6E";
        default:
          return {};
      }
    }
    switch (row) {
      case kContextMenuRowShape:
        return full_shape_mode_ ? L"\u5168\u534A\u89D2(\u5168\u89D2)"
                                : L"\u5168\u534A\u89D2(\u534A\u89D2)";
      case kContextMenuRowCharset:
        return simplified_charset_ ? L"\u8F93\u5165\u5B57\u7B26(\u7B80\u4F53)"
                                   : L"\u8F93\u5165\u5B57\u7B26(\u7E41\u4F53)";
      case kContextMenuRowEmoji:
        return L"\u8868\u60C5\u7B26\u53F7\u548C\u7B26\u53F7";
      case kContextMenuRowCustomPhrases:
        return L"\u7528\u6237\u81EA\u5B9A\u4E49\u77ED\u8BED";
      case kContextMenuRowDictionaries:
        return L"\u8BCD\u5E93\u7BA1\u7406";
      case kContextMenuRowKeyConfig:
        return L"\u6309\u952E\u914D\u7F6E";
      case kContextMenuRowToolbar:
        return toolbar_visible() ? L"\u8F93\u5165\u6CD5\u5DE5\u5177\u680F(\u5F00)"
                                 : L"\u8F93\u5165\u6CD5\u5DE5\u5177\u680F(\u5173)";
      case kContextMenuRowSettings:
        return L"\u8BBE\u7F6E";
      case kContextMenuRowRestart:
        return L"\u91CD\u542F";
      default:
        return {};
    }
  };

  const int row_count = ContextMenuRowCount(context_menu_toolbar_mode_);
  if (hovered_context_menu_row_ >= 0 && hovered_context_menu_row_ < row_count) {
    const int top = row_top(hovered_context_menu_row_);
    const int hover_inset_x = s(kContextMenuHoverInsetXDips);
    const int hover_inset_y = ScaleHalfDipForDpi(kContextMenuHoverInsetYHalfDips, dpi);
    RECT hover_rect{body.left + hover_inset_x,
                    top + hover_inset_y,
                    body.right - hover_inset_x,
                    std::min(static_cast<int>(body.bottom),
                             top + row_height - hover_inset_y)};
    FillRoundedRectAntialias(dc, hover_rect, s(kContextMenuHoverRadiusDips), hover_color);
  }

  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, text_color);
  const bool menu_text_traditional = context_menu_toolbar_mode_ ? false : !simplified_charset_;
  HFONT font = CreateLayeredUiFontForDpi(
      kContextMenuTextPointSize,
      dpi,
      FW_NORMAL,
      context_menu_toolbar_mode_ ? ToolbarTextFontFamily() : UiFontFamily(!simplified_charset_));
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  for (int row = 0; row < row_count; ++row) {
    const int top = row_top(row);
    const bool submenu_row = ContextMenuRowHasSubmenu(row, context_menu_toolbar_mode_);
    RECT text_rect{s(ContextMenuTextLeftDips(context_menu_toolbar_mode_)),
                   top,
                   submenu_row
                       ? client.right - s(ContextMenuChevronRightDips(context_menu_toolbar_mode_)) -
                             s(kContextMenuChevronSizeDips)
                       : client.right - s(ContextMenuTextRightDips(context_menu_toolbar_mode_)),
                   top + row_height};
    std::wstring label = menu_text(row);
    DrawTextWithFallback(dc,
                         label,
                         &text_rect,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                         kContextMenuTextPointSize,
                         dpi,
                         FW_NORMAL,
                         menu_text_traditional);

    if (submenu_row) {
      const int chevron_size = s(kContextMenuChevronSizeDips);
      const int chevron_left =
          client.right - s(ContextMenuChevronRightDips(context_menu_toolbar_mode_)) - chevron_size;
      RECT chevron_rect{chevron_left,
                        top + (row_height - chevron_size) / 2,
                        chevron_left + chevron_size,
                        top + (row_height - chevron_size) / 2 + chevron_size};
      DrawFluentPathIcon(dc, chevron_rect, kFluentChevronRight20, 20.0f, icon_color, 0.90f);
    } else if (!context_menu_toolbar_mode_ &&
               (row == kContextMenuRowSettings || row == kContextMenuRowRestart)) {
      const int icon_size = s(kContextMenuIconSizeDips);
      RECT icon_rect{s(kContextMenuIconLeftDips),
                     top + (row_height - icon_size) / 2,
                     s(kContextMenuIconLeftDips) + icon_size,
                     top + (row_height - icon_size) / 2 + icon_size};
      if (row == kContextMenuRowSettings) {
        DrawSettingsIcon(dc, icon_rect, icon_color, s(2));
      } else {
        DrawRestartIcon(dc, icon_rect, icon_color, s(2));
      }
    }

    if (ContextMenuHasSeparatorAfter(row, context_menu_toolbar_mode_)) {
      const int separator_y = top + row_height;
      FillSolidRect(dc, RECT{body.left, separator_y, body.right, separator_y + border}, separator_color);
    }
  }
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
}

void TsfTextService::RenderContextMenuLayeredWindow() {
  if (context_menu_window_ == nullptr) {
    return;
  }
  const UINT dpi = ReadableDpiForWindow(context_menu_window_);
  RenderRoundedLayeredWindow(context_menu_window_,
                             ScaleForDpi(kContextMenuCornerRadiusDips, dpi),
                             ToolbarPaletteForTheme(theme_mode_, theme_preset_).border,
                             [this](HDC dc) { DrawContextMenu(dc); });
}

void TsfTextService::ShowContextSubmenu(int parent_row) {
  if (!ContextMenuRowHasSubmenu(parent_row, context_menu_toolbar_mode_) ||
      context_menu_window_ == nullptr) {
    DestroyContextSubmenu();
    return;
  }
  if (context_submenu_window_ != nullptr && context_submenu_parent_row_ != parent_row) {
    DestroyContextSubmenu();
  }
  if (EnsureContextSubmenuWindowClass() == 0) {
    return;
  }

  const UINT dpi = ReadableDpiForWindow(context_menu_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const int border = FloatingWindowBorderWidth(dpi);
  const int row_height = ScaleHalfDipForDpi(kContextMenuRowHeightHalfDips, dpi);
  auto separator_count_before = [this](int row) {
    int count = 0;
    for (int previous = 0; previous < row; ++previous) {
      if (ContextMenuHasSeparatorAfter(previous, context_menu_toolbar_mode_)) {
        ++count;
      }
    }
    return count;
  };
  const int parent_top = border + parent_row * row_height +
                         separator_count_before(parent_row) * border;
  const int width = s(ContextSubmenuWidthDips(parent_row, context_menu_toolbar_mode_));
  const int submenu_rows = context_menu_toolbar_mode_
                               ? static_cast<int>(kToolbarCustomItemDefinitions.size()) + 1
                               : 2;
  const int height = 2 * border + submenu_rows * row_height;

  RECT main_rect{};
  GetWindowRect(context_menu_window_, &main_rect);
  HMONITOR monitor = MonitorFromWindow(context_menu_window_, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitor_info{};
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfoW(monitor, &monitor_info)) {
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &monitor_info.rcWork, 0);
  }
  const RECT work_area = monitor_info.rcWork;
  const int margin = s(kContextMenuScreenMarginDips);
  const int overlap = s(kContextSubmenuOverlapDips);
  int x = main_rect.right - overlap;
  int y = main_rect.top + parent_top - border;
  if (x + width > work_area.right - margin) {
    x = main_rect.left - width + overlap;
  }
  x = std::clamp(x,
                 static_cast<int>(work_area.left) + margin,
                 static_cast<int>(work_area.right) - margin - width);
  y = std::clamp(y,
                 static_cast<int>(work_area.top) + margin,
                 static_cast<int>(work_area.bottom) - margin - height);

  if (context_submenu_window_ == nullptr) {
    context_submenu_window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                                                  WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                              L"FluentPinyinContextSubmenuWindow",
                                              L"",
                                              WS_POPUP,
                                              x,
                                              y,
                                              width,
                                              height,
                                              context_menu_window_,
                                              nullptr,
                                              g_module_instance,
                                              this);
    if (context_submenu_window_ == nullptr) {
      return;
    }
  }
  context_submenu_parent_row_ = parent_row;
  hovered_context_menu_row_ = parent_row;
  SetWindowPos(context_submenu_window_,
               HWND_TOPMOST,
               x,
               y,
               width,
               height,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  RenderContextSubmenuLayeredWindow();
  ShowWindow(context_submenu_window_, SW_SHOWNOACTIVATE);
}

void TsfTextService::HideContextSubmenu() {
  if (context_submenu_window_ != nullptr) {
    ShowWindow(context_submenu_window_, SW_HIDE);
  }
  context_submenu_parent_row_ = -1;
  hovered_context_submenu_row_ = -1;
  context_submenu_mouse_tracking_ = false;
}

void TsfTextService::DestroyContextSubmenu() {
  if (context_submenu_window_ != nullptr) {
    DestroyWindow(context_submenu_window_);
    context_submenu_window_ = nullptr;
  }
  context_submenu_parent_row_ = -1;
  hovered_context_submenu_row_ = -1;
  context_submenu_mouse_tracking_ = false;
}

void TsfTextService::DrawContextSubmenu(HDC dc) {
  RECT client{};
  GetClientRect(context_submenu_window_, &client);
  const UINT dpi = ReadableDpiForWindow(context_submenu_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const int border = FloatingWindowBorderWidth(dpi);
  const int row_height = ScaleHalfDipForDpi(kContextMenuRowHeightHalfDips, dpi);
  const int corner_radius = s(kContextMenuCornerRadiusDips);
  const ToolbarPalette palette = ToolbarPaletteForTheme(theme_mode_, theme_preset_);
  const COLORREF background = palette.context_background;
  const COLORREF edge_color = palette.border;
  const COLORREF text_color = palette.context_text;
  const COLORREF hover_color = palette.context_hover;
  const int submenu_rows = context_menu_toolbar_mode_
                               ? static_cast<int>(kToolbarCustomItemDefinitions.size()) + 1
                               : 2;

  RECT body = DrawFloatingWindowSurface(dc, client, corner_radius, dpi, edge_color, background);

  if (hovered_context_submenu_row_ >= 0 && hovered_context_submenu_row_ < submenu_rows) {
    const int top = border + hovered_context_submenu_row_ * row_height;
    const int hover_inset_x = s(kContextMenuHoverInsetXDips);
    const int hover_inset_y = ScaleHalfDipForDpi(kContextMenuHoverInsetYHalfDips, dpi);
    RECT hover_rect{body.left + hover_inset_x,
                    top + hover_inset_y,
                    body.right - hover_inset_x,
                    std::min(static_cast<int>(body.bottom),
                             top + row_height - hover_inset_y)};
    FillRoundedRectAntialias(dc, hover_rect, s(kContextMenuHoverRadiusDips), hover_color);
  }

  auto submenu_text = [&](int row) -> std::wstring {
    if (context_menu_toolbar_mode_) {
      if (row >= 0 && row < static_cast<int>(kToolbarCustomItemDefinitions.size())) {
        return ToolbarCustomItemLabel(kToolbarCustomItemDefinitions[static_cast<size_t>(row)].id);
      }
      if (row == static_cast<int>(kToolbarCustomItemDefinitions.size())) {
        return L"\u9ED8\u8BA4\u8BBE\u7F6E";
      }
      return {};
    }
    if (context_submenu_parent_row_ == kContextMenuRowShape) {
      return row == 0 ? L"\u534A\u89D2" : L"\u5168\u89D2";
    }
    if (context_submenu_parent_row_ == kContextMenuRowCharset) {
      return row == 0 ? L"\u7B80\u4F53" : L"\u7E41\u4F53";
    }
    return {};
  };
  auto checked = [&](int row) {
    if (context_menu_toolbar_mode_) {
      if (row >= 0 && row < static_cast<int>(kToolbarCustomItemDefinitions.size())) {
        return ContainsToolbarItem(toolbar_visible_items_,
                                   kToolbarCustomItemDefinitions[static_cast<size_t>(row)].id);
      }
      return false;
    }
    if (context_submenu_parent_row_ == kContextMenuRowShape) {
      return row == 0 ? !full_shape_mode_ : full_shape_mode_;
    }
    if (context_submenu_parent_row_ == kContextMenuRowCharset) {
      return row == 0 ? simplified_charset_ : !simplified_charset_;
    }
    return false;
  };
  auto toolbar_item_for_row = [&](int row) -> int {
    if (!context_menu_toolbar_mode_ || row < 0 ||
        row >= static_cast<int>(kToolbarCustomItemDefinitions.size())) {
      return kToolbarItemNone;
    }
    return kToolbarCustomItemDefinitions[static_cast<size_t>(row)].id;
  };
  auto draw_toolbar_item_icon = [&](int item, RECT icon_rect) {
    auto draw_text_icon = [&](const wchar_t* text) {
      HFONT icon_font = CreateLayeredUiFontForDpi(
          kToolbarMenuTextIconPointSize, dpi, FW_NORMAL, ToolbarTextFontFamily());
      HGDIOBJ old_icon_font = icon_font != nullptr ? SelectObject(dc, icon_font) : nullptr;
      std::wstring icon_text(text);
      DrawTextWithFallback(dc,
                           icon_text,
                           &icon_rect,
                           DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                           kToolbarMenuTextIconPointSize,
                           dpi,
                           FW_NORMAL,
                           false);
      if (old_icon_font != nullptr) {
        SelectObject(dc, old_icon_font);
      }
      if (icon_font != nullptr) {
        DeleteObject(icon_font);
      }
    };
    switch (item) {
      case kToolbarItemInputMode:
        draw_text_icon(ascii_mode_ ? L"\u82F1" : L"\u4E2D");
        break;
      case kToolbarItemShape:
        if (full_shape_mode_) {
          DrawFluentPathIcon(dc, icon_rect, kFluentCircle20Filled, 20.0f, text_color, 0.84f);
        } else {
          DrawFluentPathIcon(dc, icon_rect, kFluentWeatherMoon24, 24.0f, text_color, 0.86f);
        }
        break;
      case kToolbarItemPunctuation:
        DrawStatusPunctuationModeIcon(dc, icon_rect, chinese_punctuation_mode_, text_color, 1);
        break;
      case kToolbarItemCharset:
        draw_text_icon(simplified_charset_ ? L"\u7B80" : L"\u7E41");
        break;
      case kToolbarItemEmoji:
        DrawFluentPathIcon(dc, icon_rect, kFluentEmoji24, 24.0f, text_color, 0.88f);
        break;
      case kToolbarItemNone:
        DrawFluentPathIcon(dc, icon_rect, kFluentArrowRepeatAll24, 24.0f, text_color, 0.88f);
        break;
      default:
        break;
    }
  };

  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, text_color);
  const bool submenu_text_traditional =
      context_menu_toolbar_mode_ ? false : !simplified_charset_;
  HFONT font = CreateLayeredUiFontForDpi(
      kContextMenuTextPointSize,
      dpi,
      FW_NORMAL,
      context_menu_toolbar_mode_ ? ToolbarTextFontFamily() : UiFontFamily(!simplified_charset_));
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  for (int row = 0; row < submenu_rows; ++row) {
    const int top = border + row * row_height;
    if (checked(row)) {
      const int check_slot = s(kContextSubmenuCheckSizeDips);
      const int check_left = s(kContextSubmenuCheckLeftDips);
      RECT check_rect{check_left,
                      top + (row_height - check_slot) / 2,
                      check_left + check_slot,
                      top + (row_height - check_slot) / 2 + check_slot};
      if (context_menu_toolbar_mode_) {
        DrawToolbarMenuCheckmark(dc, check_rect, text_color, dpi);
      } else {
        const int dot_size = s(kContextSubmenuRadioDotSizeDips);
        RECT dot_rect{check_left + (check_slot - dot_size) / 2,
                      top + (row_height - dot_size) / 2,
                      check_left + (check_slot - dot_size) / 2 + dot_size,
                      top + (row_height - dot_size) / 2 + dot_size};
        DrawFluentPathIcon(dc, dot_rect, kFluentCircle20Filled, 20.0f, text_color, 1.0f);
      }
    }
    const int toolbar_item = toolbar_item_for_row(row);
    if (toolbar_item != kToolbarItemNone ||
        (context_menu_toolbar_mode_ &&
         row == static_cast<int>(kToolbarCustomItemDefinitions.size()))) {
      const int icon_size = s(kToolbarCustomSubmenuIconSizeDips);
      RECT icon_rect{s(kToolbarCustomSubmenuIconLeftDips),
                     top + (row_height - icon_size) / 2,
                     s(kToolbarCustomSubmenuIconLeftDips) + icon_size,
                     top + (row_height - icon_size) / 2 + icon_size};
      draw_toolbar_item_icon(toolbar_item, icon_rect);
    }
    RECT text_rect{s(context_menu_toolbar_mode_ ? kToolbarCustomSubmenuTextLeftDips
                                                : kContextSubmenuTextLeftDips),
                   top,
                   client.right - s(context_menu_toolbar_mode_ ? kToolbarCustomSubmenuTextRightDips
                                                               : 16),
                   top + row_height};
    std::wstring label = submenu_text(row);
    DrawTextWithFallback(dc,
                         label,
                         &text_rect,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                         kContextMenuTextPointSize,
                         dpi,
                         FW_NORMAL,
                         submenu_text_traditional);
  }
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
}

void TsfTextService::RenderContextSubmenuLayeredWindow() {
  if (context_submenu_window_ == nullptr) {
    return;
  }
  const UINT dpi = ReadableDpiForWindow(context_submenu_window_);
  RenderRoundedLayeredWindow(context_submenu_window_,
                             ScaleForDpi(kContextMenuCornerRadiusDips, dpi),
                             ToolbarPaletteForTheme(theme_mode_, theme_preset_).border,
                             [this](HDC dc) { DrawContextSubmenu(dc); });
}

ATOM TsfTextService::EnsureContextMenuWindowClass() {
  static ATOM atom = 0;
  if (atom != 0) {
    return atom;
  }

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = StaticContextMenuWindowProc;
  window_class.hInstance = g_module_instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = L"FluentPinyinContextMenuWindow";
  atom = RegisterClassExW(&window_class);
  return atom;
}

ATOM TsfTextService::EnsureContextSubmenuWindowClass() {
  static ATOM atom = 0;
  if (atom != 0) {
    return atom;
  }

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = StaticContextSubmenuWindowProc;
  window_class.hInstance = g_module_instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = L"FluentPinyinContextSubmenuWindow";
  atom = RegisterClassExW(&window_class);
  return atom;
}

LRESULT TsfTextService::ContextMenuWindowProc(HWND window,
                                              UINT message,
                                              WPARAM wparam,
                                              LPARAM lparam) {
  auto row_at_point = [&](int x, int y) {
    const UINT dpi = ReadableDpiForWindow(window);
    const int border = FloatingWindowBorderWidth(dpi);
    const int row_height = ScaleHalfDipForDpi(kContextMenuRowHeightHalfDips, dpi);
    int top = border;
    for (int row = 0; row < ContextMenuRowCount(context_menu_toolbar_mode_); ++row) {
      if (y >= top && y < top + row_height) {
        RECT client{};
        GetClientRect(window, &client);
        return x >= border && x < client.right - border ? row : -1;
      }
      top += row_height;
      if (ContextMenuHasSeparatorAfter(row, context_menu_toolbar_mode_)) {
        top += border;
      }
    }
    return -1;
  };
  auto submenu_row_at_screen_point = [&](POINT screen_point) {
    if (context_submenu_window_ == nullptr || !IsWindowVisible(context_submenu_window_)) {
      return -1;
    }
    RECT submenu_rect{};
    GetWindowRect(context_submenu_window_, &submenu_rect);
    if (screen_point.x < submenu_rect.left || screen_point.x >= submenu_rect.right ||
        screen_point.y < submenu_rect.top || screen_point.y >= submenu_rect.bottom) {
      return -1;
    }
    const UINT dpi = ReadableDpiForWindow(context_submenu_window_);
    const int border = FloatingWindowBorderWidth(dpi);
    const int row_height = ScaleHalfDipForDpi(kContextMenuRowHeightHalfDips, dpi);
    const int submenu_rows = context_menu_toolbar_mode_
                                 ? static_cast<int>(kToolbarCustomItemDefinitions.size()) + 1
                                 : 2;
    const int local_y = screen_point.y - submenu_rect.top;
    const int local_x = screen_point.x - submenu_rect.left;
    RECT client{};
    GetClientRect(context_submenu_window_, &client);
    if (local_x < border || local_x >= client.right - border) {
      return -1;
    }
    for (int row = 0; row < submenu_rows; ++row) {
      const int top = border + row * row_height;
      if (local_y >= top && local_y < top + row_height) {
        return row;
      }
    }
    return -1;
  };

  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_DPICHANGED:
      ApplySuggestedDpiRect(window, lparam);
      if (hovered_context_menu_row_ >= 0 &&
          ContextMenuRowHasSubmenu(hovered_context_menu_row_, context_menu_toolbar_mode_)) {
        ShowContextSubmenu(hovered_context_menu_row_);
      }
      RenderContextMenuLayeredWindow();
      return 0;
    case WM_MOUSEMOVE: {
      const int x = static_cast<short>(LOWORD(lparam));
      const int y = static_cast<short>(HIWORD(lparam));
      POINT screen_point{x, y};
      ClientToScreen(window, &screen_point);
      const int submenu_row = submenu_row_at_screen_point(screen_point);
      if (submenu_row != -1) {
        const int previous_menu_row = hovered_context_menu_row_;
        const int previous_submenu_row = hovered_context_submenu_row_;
        hovered_context_menu_row_ = context_submenu_parent_row_;
        hovered_context_submenu_row_ = submenu_row;
        if (previous_menu_row != hovered_context_menu_row_) {
          RenderContextMenuLayeredWindow();
        }
        if (previous_submenu_row != hovered_context_submenu_row_) {
          RenderContextSubmenuLayeredWindow();
        }
        return 0;
      }

      const int row = row_at_point(x, y);
      if (row != hovered_context_menu_row_) {
        hovered_context_menu_row_ = row;
        RenderContextMenuLayeredWindow();
      }
      hovered_context_submenu_row_ = -1;
      if (ContextMenuRowHasSubmenu(row, context_menu_toolbar_mode_)) {
        ShowContextSubmenu(row);
      } else {
        HideContextSubmenu();
      }
      if (!context_menu_mouse_tracking_) {
        TRACKMOUSEEVENT event{};
        event.cbSize = sizeof(event);
        event.dwFlags = TME_LEAVE;
        event.hwndTrack = window;
        context_menu_mouse_tracking_ = TrackMouseEvent(&event) != FALSE;
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      context_menu_mouse_tracking_ = false;
      {
        POINT cursor{};
        if (GetCursorPos(&cursor) && submenu_row_at_screen_point(cursor) != -1) {
          return 0;
        }
      }
      HideContextSubmenu();
      if (hovered_context_menu_row_ != -1) {
        hovered_context_menu_row_ = -1;
        RenderContextMenuLayeredWindow();
      }
      return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      const int x = static_cast<short>(LOWORD(lparam));
      const int y = static_cast<short>(HIWORD(lparam));
      POINT screen_point{x, y};
      ClientToScreen(window, &screen_point);
      if (row_at_point(x, y) == -1 && submenu_row_at_screen_point(screen_point) == -1) {
        DestroyContextMenu();
      }
      return 0;
    }
    case WM_LBUTTONUP: {
      const int x = static_cast<short>(LOWORD(lparam));
      const int y = static_cast<short>(HIWORD(lparam));
      POINT screen_point{x, y};
      ClientToScreen(window, &screen_point);
      const int submenu_row = submenu_row_at_screen_point(screen_point);
      if (submenu_row != -1) {
        if (context_menu_toolbar_mode_) {
          DestroyContextMenu();
          if (submenu_row >= 0 &&
              submenu_row < static_cast<int>(kToolbarCustomItemDefinitions.size())) {
            ToggleToolbarItemVisibility(
                kToolbarCustomItemDefinitions[static_cast<size_t>(submenu_row)].id);
          } else {
            ToggleToolbarItemVisibility(kToolbarItemNone);
          }
          return 0;
        }
        const UINT command =
            ContextSubmenuCommandForRow(context_submenu_parent_row_, submenu_row, false);
        DestroyContextMenu();
        if (command != 0) {
          HandleLangBarMenuCommand(command);
        }
        return 0;
      }
      const int row = row_at_point(x, y);
      if (ContextMenuRowHasSubmenu(row, context_menu_toolbar_mode_)) {
        hovered_context_menu_row_ = row;
        ShowContextSubmenu(row);
        RenderContextMenuLayeredWindow();
        return 0;
      }
      if (context_menu_toolbar_mode_) {
        DestroyContextMenu();
        switch (row) {
          case kToolbarGearMenuRowLayout:
            ToggleToolbarLayout();
            break;
          case kToolbarGearMenuRowHide:
            toolbar_visible_ = false;
            SaveToolbarSetting();
            RequestInputStateRefresh();
            DestroyToolbarWindow();
            RequestToolbarHostShutdown();
            RequestToolbarHostRefresh();
            break;
          case kToolbarGearMenuRowSettings:
            OpenConfigApp();
            break;
          default:
            break;
        }
        return 0;
      }
      const UINT command = ContextMenuCommandForRow(row, false);
      DestroyContextMenu();
      if (command != 0) {
        HandleLangBarMenuCommand(command);
      }
      return 0;
    }
    case WM_CANCELMODE:
      DestroyContextMenu();
      return 0;
    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE) {
        DestroyContextMenu();
        return 0;
      }
      break;
    case WM_CAPTURECHANGED:
      if (context_menu_window_ == window) {
        HideContextMenu();
      }
      break;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      BeginPaint(window, &paint);
      EndPaint(window, &paint);
      RenderContextMenuLayeredWindow();
      return 0;
    }
    case WM_NCDESTROY:
      if (context_menu_window_ == window) {
        context_menu_window_ = nullptr;
      }
      hovered_context_menu_row_ = -1;
      context_menu_mouse_tracking_ = false;
      break;
    default:
      break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TsfTextService::StaticContextMenuWindowProc(HWND window,
                                                             UINT message,
                                                             WPARAM wparam,
                                                             LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window,
                      GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  auto* service =
      reinterpret_cast<TsfTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (service != nullptr) {
    return service->ContextMenuWindowProc(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT TsfTextService::ContextSubmenuWindowProc(HWND window,
                                                 UINT message,
                                                 WPARAM wparam,
                                                 LPARAM lparam) {
  auto row_at_point = [&](int x, int y) {
    const UINT dpi = ReadableDpiForWindow(window);
    const int border = FloatingWindowBorderWidth(dpi);
    const int row_height = ScaleHalfDipForDpi(kContextMenuRowHeightHalfDips, dpi);
    const int submenu_rows = context_menu_toolbar_mode_
                                 ? static_cast<int>(kToolbarCustomItemDefinitions.size()) + 1
                                 : 2;
    RECT client{};
    GetClientRect(window, &client);
    if (x < border || x >= client.right - border) {
      return -1;
    }
    for (int row = 0; row < submenu_rows; ++row) {
      const int top = border + row * row_height;
      if (y >= top && y < top + row_height) {
        return row;
      }
    }
    return -1;
  };

  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_DPICHANGED:
      ApplySuggestedDpiRect(window, lparam);
      RenderContextSubmenuLayeredWindow();
      return 0;
    case WM_MOUSEMOVE: {
      const int x = static_cast<short>(LOWORD(lparam));
      const int y = static_cast<short>(HIWORD(lparam));
      const int row = row_at_point(x, y);
      if (row != hovered_context_submenu_row_) {
        hovered_context_submenu_row_ = row;
        RenderContextSubmenuLayeredWindow();
      }
      if (!context_submenu_mouse_tracking_) {
        TRACKMOUSEEVENT event{};
        event.cbSize = sizeof(event);
        event.dwFlags = TME_LEAVE;
        event.hwndTrack = window;
        context_submenu_mouse_tracking_ = TrackMouseEvent(&event) != FALSE;
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      context_submenu_mouse_tracking_ = false;
      if (hovered_context_submenu_row_ != -1) {
        hovered_context_submenu_row_ = -1;
        RenderContextSubmenuLayeredWindow();
      }
      return 0;
    case WM_LBUTTONUP: {
      const int x = static_cast<short>(LOWORD(lparam));
      const int y = static_cast<short>(HIWORD(lparam));
      const int row = row_at_point(x, y);
      if (context_menu_toolbar_mode_) {
        DestroyContextMenu();
        if (row >= 0 && row < static_cast<int>(kToolbarCustomItemDefinitions.size())) {
          ToggleToolbarItemVisibility(kToolbarCustomItemDefinitions[static_cast<size_t>(row)].id);
        } else if (row == static_cast<int>(kToolbarCustomItemDefinitions.size())) {
          ToggleToolbarItemVisibility(kToolbarItemNone);
        }
        return 0;
      }
      const UINT command = ContextSubmenuCommandForRow(context_submenu_parent_row_, row, false);
      DestroyContextMenu();
      if (command != 0) {
        HandleLangBarMenuCommand(command);
      }
      return 0;
    }
    case WM_RBUTTONUP:
    case WM_CANCELMODE:
      DestroyContextMenu();
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      BeginPaint(window, &paint);
      EndPaint(window, &paint);
      RenderContextSubmenuLayeredWindow();
      return 0;
    }
    case WM_NCDESTROY:
      if (context_submenu_window_ == window) {
        context_submenu_window_ = nullptr;
      }
      context_submenu_parent_row_ = -1;
      hovered_context_submenu_row_ = -1;
      context_submenu_mouse_tracking_ = false;
      break;
    default:
      break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TsfTextService::StaticContextSubmenuWindowProc(HWND window,
                                                                UINT message,
                                                                WPARAM wparam,
                                                                LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window,
                      GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  auto* service =
      reinterpret_cast<TsfTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (service != nullptr) {
    return service->ContextSubmenuWindowProc(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

void TsfTextService::ToggleAsciiMode() {
  ascii_mode_ = !ascii_mode_;
  caps_lock_ascii_mode_ = false;
  if (status_tip_icon_mode_ == StatusTipIconMode::kChinesePunctuation ||
      status_tip_icon_mode_ == StatusTipIconMode::kEnglishPunctuation) {
    status_tip_icon_mode_ = chinese_punctuation_mode_ ? StatusTipIconMode::kChinesePunctuation
                                                      : StatusTipIconMode::kEnglishPunctuation;
  }
  PersistInputModeDefaults();
  NotifyInputModeChanged();
  RefreshToolbarHostIfVisible(toolbar_visible_);
  if (toolbar_window_ != nullptr) {
    RenderToolbarLayeredWindow();
  }
  ShowStatusTip(active_context_);
}

bool TsfTextService::ToggleAsciiModeFromKey(ITfContext* context, bool caps_lock) {
  if (IsComposing() && !composition_input_.empty()) {
    if (!CommitText(context, AsciiToWide(composition_input_))) {
      return false;
    }
  }
  ascii_mode_ = !ascii_mode_;
  caps_lock_ascii_mode_ = ascii_mode_ && caps_lock;
  if (status_tip_icon_mode_ == StatusTipIconMode::kChinesePunctuation ||
      status_tip_icon_mode_ == StatusTipIconMode::kEnglishPunctuation) {
    status_tip_icon_mode_ = chinese_punctuation_mode_ ? StatusTipIconMode::kChinesePunctuation
                                                      : StatusTipIconMode::kEnglishPunctuation;
  }
  PersistInputModeDefaults();
  NotifyInputModeChanged();
  RefreshToolbarHostIfVisible(toolbar_visible_);
  if (toolbar_window_ != nullptr) {
    RenderToolbarLayeredWindow();
  }
  ShowStatusTip(context);
  return true;
}

void TsfTextService::OpenConfigApp(const wchar_t* page) {
  auto config = InstalledSiblingExecutable(L"fluent-pinyin-settings.exe");
  std::wstring parameters;
  if (page != nullptr && page[0] != L'\0') {
    parameters = L"--page=";
    parameters += page;
  }
  fp::LogInfo(L"tsf", L"Opening settings: " + config.wstring());
  ShellExecuteW(nullptr,
                L"open",
                config.c_str(),
                parameters.empty() ? nullptr : parameters.c_str(),
                nullptr,
                SW_SHOWNORMAL);
}

void TsfTextService::OpenRimeUserDirectory() {
  const auto user_dir = fp::GetFpRoamingDataPath() / L"Rime";
  fp::EnsureDirectory(user_dir);
  ShellExecuteW(nullptr, L"open", user_dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void TsfTextService::RedeployRime() {
  fp::LogInfo(L"tsf", L"Rime redeploy requested.");
  DestroyCandidateWindow();
  HideStatusTip();
  ClearCompositionState();

  std::lock_guard<std::mutex> lock(rime_mutex_);
  if (rime_ == nullptr) {
    rime_ = std::make_unique<fp::core::RimeEngine>();
  }

  rime_ready_ = false;
  const auto status = rime_->Redeploy();
  rime_ready_ = status.initialized;
  if (rime_ready_) {
    fp::LogInfo(L"tsf", L"Rime redeploy completed.");
  } else {
    fp::LogError(L"tsf", L"Rime redeploy failed: " + status.message);
  }
}

void TsfTextService::RestartRimeAndAlgorithmServiceAsync() {
  {
    std::lock_guard<std::mutex> lock(rime_mutex_);
    if (!service_active_ || rime_restart_requested_) {
      return;
    }
    rime_restart_requested_ = true;
  }

  DestroyCandidateWindow();
  HideStatusTip();
  ClearCompositionState();

  AddRef();
  try {
    std::thread([this]() {
      try {
        ReloadRimeAndAlgorithmService();
      } catch (...) {
        fp::LogError(L"tsf", L"Settings input-config apply failed with an unexpected exception.");
      }
      {
        std::lock_guard<std::mutex> lock(rime_mutex_);
        rime_restart_requested_ = false;
      }
      Release();
    }).detach();
  } catch (...) {
    {
      std::lock_guard<std::mutex> lock(rime_mutex_);
      rime_restart_requested_ = false;
    }
    Release();
    fp::LogError(L"tsf", L"Failed to start settings input-config apply thread.");
  }
}

void TsfTextService::ShutdownRimeForUninstall() {
  fp::LogInfo(L"tsf", L"Uninstall requested input core shutdown.");
  DestroyCandidateWindow();
  HideStatusTip();
  ClearCompositionState();

  std::lock_guard<std::mutex> lock(rime_mutex_);
  if (rime_ != nullptr) {
    rime_->Shutdown();
    rime_.reset();
  }
  rime_ready_ = false;
}

void TsfTextService::ReloadRimeAndAlgorithmService() {
  const ULONGLONG reload_start_tick = GetTickCount64();
  fp::LogInfo(L"tsf", L"Settings apply requested: refresh input config and algorithm service.");

  std::lock_guard<std::mutex> lock(rime_mutex_);
  if (rime_ != nullptr) {
    rime_->Shutdown();
  }

  rime_ = std::make_unique<fp::core::RimeEngine>();
  rime_ready_ = false;
  fp::core::RimeEngineOptions options;
  options.force_rebuild_cache = true;
  auto status = rime_->Initialize(options);
  rime_ready_ = status.initialized;
  if (rime_ready_) {
    ApplyRimeOptionsLocked();
    fp::LogInfo(L"tsf",
                L"Settings reload completed in " +
                    std::to_wstring(GetTickCount64() - reload_start_tick) + L" ms.");
  } else {
    fp::LogError(L"tsf", L"Settings reload failed: " + status.message);
  }
}

void TsfTextService::RestartRimeAndAlgorithmService() {
  fp::LogInfo(L"tsf", L"Restart requested: redeploy input data and algorithm service.");
  DestroyCandidateWindow();
  HideStatusTip();
  ClearCompositionState();

  std::lock_guard<std::mutex> lock(rime_mutex_);
  if (rime_ != nullptr) {
    rime_->Shutdown();
  }
  rime_ = std::make_unique<fp::core::RimeEngine>();
  rime_ready_ = false;
  const auto status = rime_->Redeploy();
  rime_ready_ = status.initialized;
  if (rime_ready_) {
    ApplyRimeOptionsLocked();
    fp::LogInfo(L"tsf", L"Restart completed: input data redeployed and algorithm service reloaded.");
  } else {
    fp::LogError(L"tsf", L"Restart failed: " + status.message);
  }
}

void TsfTextService::OpenEmojiPanel() {
  HideCandidateTooltip();
  HideToolbarTooltip();
  INPUT inputs[4]{};
  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = VK_LWIN;
  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = VK_OEM_PERIOD;
  inputs[2].type = INPUT_KEYBOARD;
  inputs[2].ki.wVk = VK_OEM_PERIOD;
  inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
  inputs[3].type = INPUT_KEYBOARD;
  inputs[3].ki.wVk = VK_LWIN;
  inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
  const UINT sent = SendInput(static_cast<UINT>(std::size(inputs)), inputs, sizeof(INPUT));
  if (sent != std::size(inputs)) {
    ShellExecuteW(nullptr, L"open", L"ms-shellactivity:emoji", nullptr, nullptr, SW_SHOWNORMAL);
  }
}

void TsfTextService::OpenFluentPinyinWebsite() {
  if (kFluentPinyinWebsiteUrl.empty()) {
    return;
  }
  ShellExecuteW(nullptr,
                L"open",
                std::wstring(kFluentPinyinWebsiteUrl).c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL);
}

bool TsfTextService::toolbar_visible() const {
  return ReadToolbarVisibleSetting(toolbar_visible_);
}

void TsfTextService::ToggleToolbarWindow() {
  toolbar_visible_ = !ReadToolbarVisibleSetting(toolbar_visible_);
  SaveToolbarSetting();
  RequestInputStateRefresh();
  if (toolbar_visible_) {
    if (ShouldHostToolbarWindow()) {
      ShowToolbarWindow();
    } else {
      DestroyToolbarWindow();
    }
  } else {
    DestroyToolbarWindow();
    RequestToolbarHostShutdown();
  }
}

void TsfTextService::ShowToolbarWindow() {
  toolbar_visible_ = true;
  WriteBoolSetting(L"toolbar_visible", false);
  if (toolbar_window_ != nullptr && !IsWindow(toolbar_window_)) {
    toolbar_window_ = nullptr;
    ReleaseToolbarWindowOwnership();
  }

  if (toolbar_window_ != nullptr) {
    PositionToolbarWindow(false);
    RenderToolbarLayeredWindow();
    KeepToolbarWindowTopmost();
    return;
  }

  if (!ShouldHostToolbarWindow()) {
    fp::LogInfo(L"tsf", L"Toolbar show skipped: current process should not host the toolbar.");
    return;
  }
  if (!TryAcquireToolbarWindowOwnership()) {
    fp::LogInfo(L"tsf", L"Toolbar show deferred: another host owns the toolbar window.");
    return;
  }
  if (EnsureToolbarWindowClass() == 0) {
    fp::LogWarning(L"tsf", L"Toolbar window class registration failed.");
    ReleaseToolbarWindowOwnership();
    return;
  }

  if (toolbar_window_ == nullptr) {
    toolbar_window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                                          WS_EX_LAYERED,
                                      kToolbarWindowClassName,
                                      L"",
                                      WS_POPUP,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      426,
                                      80,
                                      nullptr,
                                      nullptr,
                                      g_module_instance,
                                      this);
    if (toolbar_window_ == nullptr) {
      fp::LogWarning(L"tsf", L"Toolbar window creation failed.");
      ReleaseToolbarWindowOwnership();
      return;
    }
    fp::LogInfo(L"tsf", L"Toolbar window created.");
  }

  PositionToolbarWindow(false);
  SetTimer(toolbar_window_, kToolbarWindowWatchTimer, 600, nullptr);
  RenderToolbarLayeredWindow();
  SetWindowPos(toolbar_window_,
               HWND_TOPMOST,
               0,
               0,
               0,
               0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  fp::LogInfo(L"tsf", L"Toolbar window shown for theme preset: " + theme_preset_);
}

void TsfTextService::RefreshToolbarFromSettings() {
  const bool visible = ReadToolbarVisibleSetting(toolbar_visible_);
  const std::wstring legacy_theme =
      ReadStringSetting(fp::kLegacyThemeSetting, fp::kThemeModeDark);
  theme_mode_ =
      fp::NormalizeThemeModeSetting(ReadStringSetting(fp::kThemeModeSetting, legacy_theme));
  theme_preset_ =
      fp::NormalizeThemePresetSetting(ReadStringSetting(fp::kThemePresetSetting, legacy_theme));
  apps_use_light_theme_ = AppsUseLightTheme();
  system_uses_light_theme_ = SystemUsesLightTheme();
  toolbar_vertical_layout_ = ReadStringSetting(kToolbarLayoutSetting, L"horizontal") == L"vertical";
  toolbar_visible_items_ = ParseToolbarVisibleItems(ReadStringSetting(kToolbarItemsSetting));
  ApplyInputStateFromSettings(true);
  toolbar_visible_ = visible;

  toolbar_position_user_ = ReadBoolSetting(kToolbarPositionUserSetting, false);
  std::optional<POINT> toolbar_point =
      toolbar_position_user_ ? ReadPointSetting(kToolbarPositionSetting) : std::nullopt;
  if (toolbar_point) {
    toolbar_position_ = *toolbar_point;
    has_toolbar_position_ = true;
  } else if (toolbar_position_user_) {
    const bool keep_existing_user_position = has_toolbar_position_;
    toolbar_position_user_ = keep_existing_user_position;
    has_toolbar_position_ = keep_existing_user_position;
  }
  SaveToolbarSetting();

  if (!toolbar_visible_) {
    DestroyToolbarWindow();
    RequestToolbarHostShutdown();
    return;
  }
  if (!ShouldHostToolbarWindow()) {
    DestroyToolbarWindow();
    RequestToolbarHostShutdown();
    return;
  }

  if (toolbar_window_ == nullptr || !IsWindow(toolbar_window_)) {
    toolbar_window_ = nullptr;
    ShowToolbarWindow();
    return;
  }

  PositionToolbarWindowKeepingCenter(false);
  RenderToolbarLayeredWindow();
  KeepToolbarWindowTopmost();
}

void TsfTextService::HideToolbarWindow() {
  if (toolbar_window_ != nullptr) {
    KillTimer(toolbar_window_, kToolbarTooltipTimer);
    hovered_toolbar_item_ = kToolbarItemNone;
    pressed_toolbar_item_ = kToolbarItemNone;
    toolbar_mouse_tracking_ = false;
    HideToolbarTooltip();
    ShowWindow(toolbar_window_, SW_HIDE);
  }
}

void TsfTextService::DestroyToolbarWindow() {
  const bool owned_window = toolbar_window_ != nullptr;
  if (toolbar_window_ != nullptr) {
    DestroyToolbarTooltip();
    KillTimer(toolbar_window_, kToolbarWindowWatchTimer);
    KillTimer(toolbar_window_, kToolbarTooltipTimer);
    DestroyWindow(toolbar_window_);
    toolbar_window_ = nullptr;
  }
  ReleaseToolbarWindowOwnership();
  if (owned_window && toolbar_visible_ && ShouldHostToolbarWindow()) {
    RefreshToolbarHostIfVisible(toolbar_visible_);
  }
}

bool TsfTextService::TryAcquireToolbarWindowOwnership() {
  if (toolbar_owner_mutex_acquired_) {
    return true;
  }
  if (toolbar_owner_mutex_ == nullptr) {
    toolbar_owner_mutex_ = CreateMutexW(nullptr, FALSE, kToolbarWindowOwnerMutexName);
    if (toolbar_owner_mutex_ == nullptr) {
      return false;
    }
  }
  const DWORD wait = WaitForSingleObject(toolbar_owner_mutex_, 0);
  if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) {
    toolbar_owner_mutex_acquired_ = true;
    return true;
  }
  return false;
}

void TsfTextService::ReleaseToolbarWindowOwnership() {
  if (toolbar_owner_mutex_acquired_ && toolbar_owner_mutex_ != nullptr) {
    ReleaseMutex(toolbar_owner_mutex_);
    toolbar_owner_mutex_acquired_ = false;
  }
  if (toolbar_owner_mutex_ != nullptr) {
    CloseHandle(toolbar_owner_mutex_);
    toolbar_owner_mutex_ = nullptr;
  }
}

void TsfTextService::KeepToolbarWindowTopmost() {
  if (toolbar_window_ == nullptr || !IsWindow(toolbar_window_)) {
    return;
  }
  EnsureToolbarWindowTopmost();
  RenderToolbarLayeredWindow();
}

void TsfTextService::EnsureToolbarWindowTopmost() {
  if (toolbar_window_ == nullptr || !IsWindow(toolbar_window_)) {
    return;
  }
  SetWindowPos(toolbar_window_,
               HWND_TOPMOST,
               0,
               0,
               0,
               0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void TsfTextService::PositionToolbarWindow(bool show_window) {
  if (toolbar_window_ == nullptr) {
    return;
  }

  RECT work_area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  const UINT dpi = ReadableDpiForWindow(toolbar_window_);
  const std::vector<int> visible_items = VisibleToolbarItemsWithSettings(toolbar_visible_items_);
  const int width = ToolbarWindowWidthPixels(toolbar_vertical_layout_, visible_items.size(), dpi);
  const int height = ToolbarWindowHeightPixels(toolbar_vertical_layout_, visible_items.size(), dpi);
  const bool use_saved_position = toolbar_position_user_ && has_toolbar_position_;
  int x = use_saved_position
              ? toolbar_position_.x
              : work_area.right - width - ScaleForDpi(kToolbarDefaultRightInsetDips, dpi);
  int y = use_saved_position
              ? toolbar_position_.y
              : work_area.bottom - height - ScaleForDpi(kToolbarDefaultBottomInsetDips, dpi);
  if (x < work_area.left) {
    x = work_area.left;
  } else if (x + width > work_area.right) {
    x = work_area.right - width;
  }
  if (y < work_area.top) {
    y = work_area.top;
  } else if (y + height > work_area.bottom) {
    y = work_area.bottom - height;
  }
  toolbar_position_ = {x, y};
  has_toolbar_position_ = true;

  SetWindowPos(toolbar_window_,
               HWND_TOPMOST,
               x,
               y,
               width,
               height,
               SWP_NOACTIVATE | (show_window ? SWP_SHOWWINDOW : 0));
  SetWindowRgn(toolbar_window_, nullptr, FALSE);
  if (show_window) {
    RenderToolbarLayeredWindow();
  }
}

void TsfTextService::PositionToolbarWindowKeepingCenter(bool show_window) {
  if (toolbar_window_ == nullptr || !toolbar_position_user_ || !has_toolbar_position_ ||
      !IsWindow(toolbar_window_)) {
    PositionToolbarWindow(show_window);
    return;
  }

  RECT rect{};
  if (!GetWindowRect(toolbar_window_, &rect)) {
    PositionToolbarWindow(show_window);
    return;
  }

  const UINT dpi = ReadableDpiForWindow(toolbar_window_);
  const std::vector<int> visible_items = VisibleToolbarItemsWithSettings(toolbar_visible_items_);
  const int new_width = ToolbarWindowWidthPixels(toolbar_vertical_layout_, visible_items.size(), dpi);
  const int new_height =
      ToolbarWindowHeightPixels(toolbar_vertical_layout_, visible_items.size(), dpi);
  toolbar_position_ = {rect.left + (rect.right - rect.left - new_width) / 2,
                       rect.top + (rect.bottom - rect.top - new_height) / 2};
  has_toolbar_position_ = true;
  PositionToolbarWindow(show_window);
  if (toolbar_position_user_) {
    SaveToolbarSetting();
  }
}

void TsfTextService::RenderToolbarLayeredWindow() {
  if (toolbar_window_ == nullptr) {
    return;
  }
  const UINT dpi = ReadableDpiForWindow(toolbar_window_);
  RECT client{};
  GetClientRect(toolbar_window_, &client);
  RECT visible_rect{client.left + kToolbarOuterInsetPixels,
                    client.top + kToolbarOuterInsetPixels,
                    client.right - kToolbarOuterInsetPixels,
                    client.bottom - kToolbarOuterInsetPixels};
  const int radius = ScaleForDpi(kToolbarCornerRadiusDips, dpi);
  RenderToolbarLayeredWindowWithVisibleRect(toolbar_window_,
                                            visible_rect,
                                            radius,
                                            ToolbarWindowPaletteForTheme(theme_mode_, theme_preset_).border,
                                            [this](HDC dc) {
                                              DrawToolbarWindow(dc);
                                            });
}

void TsfTextService::DrawToolbarWindow(HDC dc) {
  RECT client{};
  GetClientRect(toolbar_window_, &client);
  const UINT dpi = ReadableDpiForWindow(toolbar_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const ToolbarPalette palette = ToolbarWindowPaletteForTheme(theme_mode_, theme_preset_);

  RECT toolbar_rect{client.left + kToolbarOuterInsetPixels,
                    client.top + kToolbarOuterInsetPixels,
                    client.right - kToolbarOuterInsetPixels,
                    client.bottom - kToolbarOuterInsetPixels};
  const int corner_radius = s(kToolbarCornerRadiusDips);
  const int border_width = kToolbarBorderPixels;
  RECT inner = DrawRoundedSurface(dc,
                                  toolbar_rect,
                                  corner_radius,
                                  border_width,
                                  palette.border,
                                  palette.background);
  RECT drag_background =
      toolbar_vertical_layout_
          ? RECT{inner.left,
                 inner.top,
                 inner.right,
                 std::min<LONG>(inner.bottom, toolbar_rect.top + s(kToolbarDragWidthDips))}
          : RECT{inner.left,
                 inner.top,
                 std::min<LONG>(inner.right, toolbar_rect.left + s(kToolbarDragWidthDips)),
                 inner.bottom};
  FillRoundedRectSidesAntialias(
      dc,
      drag_background,
      std::max(1, corner_radius - border_width),
      !toolbar_vertical_layout_,
      false,
      palette.drag_background);

  for (auto item : ToolbarItemsForDpi(dpi, toolbar_vertical_layout_, toolbar_visible_items_)) {
    OffsetRect(&item.hit_rect, toolbar_rect.left, toolbar_rect.top);
    OffsetRect(&item.visual_rect, toolbar_rect.left, toolbar_rect.top);
    OffsetRect(&item.feedback_rect, toolbar_rect.left, toolbar_rect.top);
    if (pressed_toolbar_item_ == item.id || hovered_toolbar_item_ == item.id) {
      FillRoundedRectAntialias(dc,
                               item.feedback_rect,
                               s(4),
                               pressed_toolbar_item_ == item.id ? palette.pressed : palette.hover);
    }
  }

  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, palette.text);

  HPEN separator = CreatePen(PS_SOLID, HairlineForDpi(dpi), palette.drag_separator);
  HGDIOBJ old_pen = SelectObject(dc, separator);
  if (toolbar_vertical_layout_) {
    MoveToEx(dc,
             toolbar_rect.left + s(kToolbarSeparatorTopDips),
             toolbar_rect.top + s(kToolbarDragWidthDips),
             nullptr);
    LineTo(dc,
           toolbar_rect.right - s(kToolbarSeparatorBottomDips),
           toolbar_rect.top + s(kToolbarDragWidthDips));
  } else {
    MoveToEx(dc,
             toolbar_rect.left + s(kToolbarDragWidthDips),
             toolbar_rect.top + s(kToolbarSeparatorTopDips),
             nullptr);
    LineTo(dc,
           toolbar_rect.left + s(kToolbarDragWidthDips),
           toolbar_rect.bottom - s(kToolbarSeparatorBottomDips));
  }
  SelectObject(dc, old_pen);
  DeleteObject(separator);

  RECT grip =
      toolbar_vertical_layout_
          ? RECT{toolbar_rect.left + s(kToolbarGripTopDips),
                 toolbar_rect.top + s(kToolbarGripLeftDips),
                 toolbar_rect.left + s(kToolbarGripTopDips + kToolbarGripHeightDips),
                 toolbar_rect.top + s(kToolbarGripLeftDips + kToolbarGripWidthDips)}
          : RECT{toolbar_rect.left + s(kToolbarGripLeftDips),
                 toolbar_rect.top + s(kToolbarGripTopDips),
                 toolbar_rect.left + s(kToolbarGripLeftDips + kToolbarGripWidthDips),
                 toolbar_rect.top + s(kToolbarGripTopDips + kToolbarGripHeightDips)};
  FillRoundedRectAntialias(dc, grip, s(2), palette.muted);

  HFONT text_font =
      CreateLayeredUiFontForDpi(kToolbarTextPointSize, dpi, FW_NORMAL, ToolbarTextFontFamily());
  HGDIOBJ old_font = SelectObject(dc, text_font);

  for (auto item : ToolbarItemsForDpi(dpi, toolbar_vertical_layout_, toolbar_visible_items_)) {
    OffsetRect(&item.hit_rect, toolbar_rect.left, toolbar_rect.top);
    OffsetRect(&item.visual_rect, toolbar_rect.left, toolbar_rect.top);
    OffsetRect(&item.feedback_rect, toolbar_rect.left, toolbar_rect.top);
    RECT rect = item.visual_rect;
    switch (item.id) {
      case kToolbarItemInputMode: {
        OffsetRect(&rect, 0, ScaleHalfDipForDpi(kToolbarTextVerticalOffsetHalfDips, dpi));
        SelectObject(dc, text_font);
        DrawTextW(dc,
                  ascii_mode_ ? L"\u82F1" : L"\u4E2D",
                  -1,
                  &rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        break;
      }
      case kToolbarItemShape:
        DrawToolbarShapeModeIcon(
            dc, ToolbarItemIconRect(item, dpi, 0), full_shape_mode_, palette.text, s(2));
        break;
      case kToolbarItemPunctuation:
        DrawToolbarPunctuationTextIcon(
            dc, ToolbarItemIconRect(item, dpi, 0), chinese_punctuation_mode_, palette.text, dpi);
        break;
      case kToolbarItemCharset:
        OffsetRect(&rect, 0, ScaleHalfDipForDpi(kToolbarTextVerticalOffsetHalfDips, dpi));
        SelectObject(dc, text_font);
        DrawTextW(dc,
                  simplified_charset_ ? L"\u7B80" : L"\u7E41",
                  -1,
                  &rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        break;
      case kToolbarItemEmoji:
        DrawToolbarSmileIcon(dc, ToolbarItemIconRect(item, dpi, 0), palette.text, s(2));
        break;
      case kToolbarItemSettings:
        DrawToolbarSettingsIcon(dc, ToolbarItemIconRect(item, dpi, 0), palette.text, s(2));
        break;
      default:
        break;
    }
  }

  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (text_font != nullptr) {
    DeleteObject(text_font);
  }
  DrawToolbarOuterEdge(dc, toolbar_rect, corner_radius, kToolbarBorderPixels, palette.border);
}

void TsfTextService::ShowToolbarTooltip(int item) {
  if (toolbar_window_ == nullptr || item <= kToolbarItemDrag) {
    HideToolbarTooltip();
    return;
  }
  if (EnsureToolbarTooltipWindowClass() == 0) {
    return;
  }
  if (toolbar_tooltip_window_ == nullptr) {
    toolbar_tooltip_window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                                                  WS_EX_LAYERED,
                                              kToolbarTooltipWindowClassName,
                                              L"",
                                              WS_POPUP,
                                              CW_USEDEFAULT,
                                              CW_USEDEFAULT,
                                              120,
                                              36,
                                              nullptr,
                                              nullptr,
                                              g_module_instance,
                                              this);
    if (toolbar_tooltip_window_ == nullptr) {
      return;
    }
    const DWORD corner_preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(toolbar_tooltip_window_,
                          DWMWA_WINDOW_CORNER_PREFERENCE,
                          &corner_preference,
                          sizeof(corner_preference));
  }
  const bool was_visible = IsWindowVisible(toolbar_tooltip_window_) != FALSE;
  const int previous_item = toolbar_tooltip_item_;
  toolbar_tooltip_item_ = item;
  PositionToolbarTooltip(item);
  RenderToolbarTooltipLayeredWindow();
  if (!was_visible || previous_item != item) {
    ShowWindow(toolbar_tooltip_window_, SW_HIDE);
    if (!AnimateWindow(toolbar_tooltip_window_,
                       kToolbarTooltipAnimationMs,
                       AW_BLEND | AW_SLIDE | AW_VER_POSITIVE)) {
      ShowWindow(toolbar_tooltip_window_, SW_SHOWNOACTIVATE);
    }
  } else {
    ShowWindow(toolbar_tooltip_window_, SW_SHOWNOACTIVATE);
  }
}

void TsfTextService::HideToolbarTooltip() {
  toolbar_tooltip_item_ = kToolbarItemNone;
  has_toolbar_tooltip_anchor_ = false;
  if (toolbar_tooltip_window_ != nullptr) {
    ShowWindow(toolbar_tooltip_window_, SW_HIDE);
  }
}

void TsfTextService::DestroyToolbarTooltip() {
  if (toolbar_tooltip_window_ != nullptr) {
    DestroyWindow(toolbar_tooltip_window_);
    toolbar_tooltip_window_ = nullptr;
  }
  toolbar_tooltip_item_ = kToolbarItemNone;
  has_toolbar_tooltip_anchor_ = false;
}

void TsfTextService::PositionToolbarTooltip(int item) {
  if (toolbar_window_ == nullptr || toolbar_tooltip_window_ == nullptr) {
    return;
  }
  const UINT dpi = ReadableDpiForWindow(toolbar_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  HDC dc = GetDC(toolbar_tooltip_window_);
  HFONT font = CreateLayeredUiFontForDpi(
      kToolbarTooltipFontPointSize, dpi, FW_NORMAL, UiFontFamily(!simplified_charset_));
  HGDIOBJ old_font = font != nullptr && dc != nullptr ? SelectObject(dc, font) : nullptr;
  const std::wstring text = ToolbarTooltipText(item,
                                               ascii_mode_,
                                               full_shape_mode_,
                                               chinese_punctuation_mode_,
                                               simplified_charset_);
  SIZE text_size{};
  if (dc != nullptr) {
    text_size = MeasureTextWithFallback(
        dc, text, kToolbarTooltipFontPointSize, dpi, FW_NORMAL, !simplified_charset_);
  }
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
  if (dc != nullptr) {
    ReleaseDC(toolbar_tooltip_window_, dc);
  }

  const int padding = s(kToolbarTooltipPaddingDips);
  const int width =
      std::max(s(kToolbarTooltipMinWidthDips),
               static_cast<int>(text_size.cx) + padding * 2 +
                   s(kToolbarTooltipOverhangGuardDips));
  const int height = s(kToolbarTooltipHeightDips);
  POINT anchor{};
  if (has_toolbar_tooltip_anchor_) {
    anchor = toolbar_tooltip_anchor_;
  } else {
    GetCursorPos(&anchor);
  }

  int x = anchor.x - width / 2;
  int y = anchor.y - height - s(kToolbarTooltipMouseOffsetYDips);
  RECT work_area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  if (x + width > work_area.right) {
    x = work_area.right - width;
  }
  if (x < work_area.left) {
    x = work_area.left;
  }
  if (y + height > work_area.bottom) {
    y = anchor.y + s(kToolbarTooltipMouseOffsetYDips);
  }
  if (y < work_area.top) {
    y = anchor.y + s(kToolbarTooltipMouseOffsetYDips);
  }
  SetWindowPos(toolbar_tooltip_window_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
  SetWindowRgn(toolbar_tooltip_window_, nullptr, FALSE);
}

void TsfTextService::DrawToolbarTooltip(HDC dc) {
  RECT client{};
  GetClientRect(toolbar_tooltip_window_, &client);
  const UINT dpi = ReadableDpiForWindow(toolbar_tooltip_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const ToolbarPalette palette = ToolbarPaletteForTheme(theme_mode_, theme_preset_);
  DrawFloatingWindowSurface(dc,
                            client,
                            ToolbarTooltipCornerRadius(dpi),
                            dpi,
                            palette.border,
                            palette.tooltip_background);

  HFONT font = CreateLayeredUiFontForDpi(
      kToolbarTooltipFontPointSize, dpi, FW_NORMAL, UiFontFamily(!simplified_charset_));
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, palette.tooltip_text);
  const int padding = s(kToolbarTooltipPaddingDips);
  RECT text_rect{client.left + padding, client.top, client.right - padding, client.bottom - s(1)};
  const std::wstring text = ToolbarTooltipText(toolbar_tooltip_item_,
                                               ascii_mode_,
                                               full_shape_mode_,
                                               chinese_punctuation_mode_,
                                               simplified_charset_);
  DrawTextWithFallback(dc,
                       text,
                       &text_rect,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                       kToolbarTooltipFontPointSize,
                       dpi,
                       FW_NORMAL,
                       !simplified_charset_);
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
}

void TsfTextService::RenderToolbarTooltipLayeredWindow() {
  if (toolbar_tooltip_window_ == nullptr) {
    return;
  }
  const UINT dpi = ReadableDpiForWindow(toolbar_tooltip_window_);
  const int radius = ToolbarTooltipCornerRadius(dpi);
  RenderRoundedLayeredWindow(
      toolbar_tooltip_window_,
      radius,
      ToolbarPaletteForTheme(theme_mode_, theme_preset_).border,
      [this](HDC dc) {
        DrawToolbarTooltip(dc);
      });
}

ATOM TsfTextService::EnsureToolbarTooltipWindowClass() {
  static ATOM atom = 0;
  if (atom != 0) {
    return atom;
  }

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.style = CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc = StaticToolbarTooltipWindowProc;
  window_class.hInstance = g_module_instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = kToolbarTooltipWindowClassName;
  atom = RegisterClassExW(&window_class);
  return atom;
}

LRESULT TsfTextService::ToolbarTooltipWindowProc(HWND window,
                                                 UINT message,
                                                 WPARAM wparam,
                                                 LPARAM lparam) {
  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_DPICHANGED:
      ApplySuggestedDpiRect(window, lparam);
      if (toolbar_tooltip_item_ != kToolbarItemNone) {
        PositionToolbarTooltip(toolbar_tooltip_item_);
      }
      RenderToolbarTooltipLayeredWindow();
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      BeginPaint(window, &paint);
      EndPaint(window, &paint);
      RenderToolbarTooltipLayeredWindow();
      return 0;
    }
    case WM_NCDESTROY:
      if (toolbar_tooltip_window_ == window) {
        toolbar_tooltip_window_ = nullptr;
        toolbar_tooltip_item_ = kToolbarItemNone;
      }
      break;
    default:
      break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TsfTextService::StaticToolbarTooltipWindowProc(HWND window,
                                                                UINT message,
                                                                WPARAM wparam,
                                                                LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window,
                      GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  auto* service =
      reinterpret_cast<TsfTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (service != nullptr) {
    return service->ToolbarTooltipWindowProc(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

ATOM TsfTextService::EnsureToolbarWindowClass() {
  static ATOM atom = 0;
  if (atom != 0) {
    return atom;
  }

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = StaticToolbarWindowProc;
  window_class.hInstance = g_module_instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;
  window_class.cbWndExtra = kCandidateWindowClassExtraLastHeight + sizeof(LONG_PTR);
  window_class.lpszClassName = kToolbarWindowClassName;
  atom = RegisterClassExW(&window_class);
  return atom;
}

LRESULT TsfTextService::ToolbarWindowProc(HWND window,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_DPICHANGED: {
      ApplySuggestedDpiRect(window, lparam);
      if (toolbar_window_ == window) {
        RECT rect{};
        if (GetWindowRect(window, &rect)) {
          toolbar_position_ = {rect.left, rect.top};
          has_toolbar_position_ = true;
        }
        PositionToolbarWindowKeepingCenter(IsWindowVisible(window) != FALSE);
        if (toolbar_tooltip_item_ != kToolbarItemNone) {
          PositionToolbarTooltip(toolbar_tooltip_item_);
        }
      }
      RenderToolbarLayeredWindow();
      return 0;
    }
    case WM_LBUTTONDOWN: {
      const int x = static_cast<short>(LOWORD(lparam));
      const int y = static_cast<short>(HIWORD(lparam));
      const UINT dpi = ReadableDpiForWindow(window);
      const int item =
          ToolbarItemAtPoint(dpi, toolbar_vertical_layout_, toolbar_visible_items_, x, y);
      if (item == kToolbarItemDrag) {
        toolbar_dragging_ = GetCursorPos(&toolbar_drag_start_cursor_) != FALSE;
        RECT rect{};
        GetWindowRect(window, &rect);
        toolbar_drag_start_origin_ = {rect.left, rect.top};
        if (toolbar_dragging_) {
          SetCapture(window);
        }
      } else if (item != kToolbarItemNone) {
        pressed_toolbar_item_ = item;
        hovered_toolbar_item_ = item;
        POINT cursor{};
        has_toolbar_tooltip_anchor_ = GetCursorPos(&cursor) != FALSE;
        if (has_toolbar_tooltip_anchor_) {
          toolbar_tooltip_anchor_ = cursor;
        }
        KillTimer(window, kToolbarTooltipTimer);
        HideToolbarTooltip();
        SetCapture(window);
      }
      RenderToolbarLayeredWindow();
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (toolbar_dragging_) {
        POINT cursor{};
        if (GetCursorPos(&cursor)) {
          const UINT dpi = ReadableDpiForWindow(window);
          RECT work_area{};
          SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
          RECT rect{};
          GetWindowRect(window, &rect);
          const int width = rect.right - rect.left;
          const int height = rect.bottom - rect.top;
          int x = toolbar_drag_start_origin_.x + cursor.x - toolbar_drag_start_cursor_.x;
          int y = toolbar_drag_start_origin_.y + cursor.y - toolbar_drag_start_cursor_.y;
          const int margin = ScaleForDpi(4, dpi);
          x = std::clamp(x,
                         static_cast<int>(work_area.left) + margin - width,
                         static_cast<int>(work_area.right) - margin);
          y = std::clamp(y,
                         static_cast<int>(work_area.top) + margin - height,
                         static_cast<int>(work_area.bottom) - margin);
          toolbar_position_ = {x, y};
          has_toolbar_position_ = true;
          toolbar_position_user_ = true;
          SetWindowPos(window,
                       HWND_TOPMOST,
                       x,
                       y,
                       0,
                       0,
                       SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
          RenderToolbarLayeredWindow();
        }
        return 0;
      }
      const UINT dpi = ReadableDpiForWindow(window);
      const int x = static_cast<short>(LOWORD(lparam));
      const int y = static_cast<short>(HIWORD(lparam));
      const int previous_hovered = hovered_toolbar_item_;
      hovered_toolbar_item_ =
          ToolbarItemAtPoint(dpi, toolbar_vertical_layout_, toolbar_visible_items_, x, y);
      if (hovered_toolbar_item_ == kToolbarItemDrag) {
        hovered_toolbar_item_ = kToolbarItemNone;
      }
      POINT cursor{};
      if (hovered_toolbar_item_ != kToolbarItemNone && GetCursorPos(&cursor)) {
        toolbar_tooltip_anchor_ = cursor;
        has_toolbar_tooltip_anchor_ = true;
      } else {
        has_toolbar_tooltip_anchor_ = false;
      }
      if (!toolbar_mouse_tracking_) {
        TRACKMOUSEEVENT event{};
        event.cbSize = sizeof(event);
        event.dwFlags = TME_LEAVE;
        event.hwndTrack = window;
        toolbar_mouse_tracking_ = TrackMouseEvent(&event) != FALSE;
      }
      if (previous_hovered != hovered_toolbar_item_) {
        KillTimer(window, kToolbarTooltipTimer);
        toolbar_tooltip_item_ = kToolbarItemNone;
        if (toolbar_tooltip_window_ != nullptr) {
          ShowWindow(toolbar_tooltip_window_, SW_HIDE);
        }
        if (hovered_toolbar_item_ != kToolbarItemNone) {
          SetTimer(window, kToolbarTooltipTimer, kToolbarTooltipDelayMs, nullptr);
        } else {
          has_toolbar_tooltip_anchor_ = false;
        }
        RenderToolbarLayeredWindow();
      } else if (hovered_toolbar_item_ != kToolbarItemNone &&
                 toolbar_tooltip_item_ == hovered_toolbar_item_ &&
                 toolbar_tooltip_window_ != nullptr &&
                 IsWindowVisible(toolbar_tooltip_window_)) {
        PositionToolbarTooltip(hovered_toolbar_item_);
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      toolbar_mouse_tracking_ = false;
      KillTimer(window, kToolbarTooltipTimer);
      if (hovered_toolbar_item_ != kToolbarItemNone) {
        hovered_toolbar_item_ = kToolbarItemNone;
        has_toolbar_tooltip_anchor_ = false;
        HideToolbarTooltip();
        RenderToolbarLayeredWindow();
      }
      return 0;
    case WM_LBUTTONUP:
      if (toolbar_dragging_) {
        toolbar_dragging_ = false;
        if (has_toolbar_position_) {
          toolbar_position_user_ = true;
          SaveToolbarSetting();
        }
        ReleaseCapture();
        return 0;
      }
      if (pressed_toolbar_item_ != kToolbarItemNone) {
        const int pressed_item = pressed_toolbar_item_;
        pressed_toolbar_item_ = kToolbarItemNone;
        if (GetCapture() == window) {
          ReleaseCapture();
        }
        const UINT dpi = ReadableDpiForWindow(window);
        const int x = static_cast<short>(LOWORD(lparam));
        const int y = static_cast<short>(HIWORD(lparam));
        const int released_item =
            ToolbarItemAtPoint(dpi, toolbar_vertical_layout_, toolbar_visible_items_, x, y);
        hovered_toolbar_item_ =
            released_item > kToolbarItemDrag ? released_item : kToolbarItemNone;
        POINT cursor{};
        if (hovered_toolbar_item_ != kToolbarItemNone && GetCursorPos(&cursor)) {
          toolbar_tooltip_anchor_ = cursor;
          has_toolbar_tooltip_anchor_ = true;
        } else {
          has_toolbar_tooltip_anchor_ = false;
        }
        KillTimer(window, kToolbarTooltipTimer);
        if (toolbar_tooltip_window_ != nullptr) {
          ShowWindow(toolbar_tooltip_window_, SW_HIDE);
        }
        if (hovered_toolbar_item_ != kToolbarItemNone) {
          SetTimer(window, kToolbarTooltipTimer, kToolbarTooltipDelayMs, nullptr);
        }
        RenderToolbarLayeredWindow();
        if (released_item == pressed_item) {
          switch (pressed_item) {
            case kToolbarItemInputMode:
              ToggleAsciiMode();
              break;
            case kToolbarItemShape:
              HandleLangBarMenuCommand(kMenuFullShape);
              break;
            case kToolbarItemPunctuation:
              HandleLangBarMenuCommand(kMenuPunctuation);
              break;
            case kToolbarItemCharset:
              HandleLangBarMenuCommand(kMenuCharset);
              break;
            case kToolbarItemEmoji:
              HandleLangBarMenuCommand(kMenuEmoji);
              break;
            case kToolbarItemSettings:
              {
                POINT menu_point{x, y};
                ClientToScreen(window, &menu_point);
                ShowToolbarGearMenu(menu_point);
              }
              break;
            default:
              break;
          }
        }
        return 0;
      }
      break;
    case WM_CAPTURECHANGED:
      if (toolbar_dragging_ && has_toolbar_position_) {
        toolbar_position_user_ = true;
        SaveToolbarSetting();
      }
      toolbar_dragging_ = false;
      KillTimer(window, kToolbarTooltipTimer);
      if (pressed_toolbar_item_ != kToolbarItemNone) {
        pressed_toolbar_item_ = kToolbarItemNone;
        RenderToolbarLayeredWindow();
      }
      break;
    case WM_RBUTTONUP: {
      POINT point{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
      const UINT dpi = ReadableDpiForWindow(window);
      const int item =
          ToolbarItemAtPoint(dpi, toolbar_vertical_layout_, toolbar_visible_items_, point.x, point.y);
      if (item == kToolbarItemSettings) {
        ClientToScreen(window, &point);
        ShowToolbarGearMenu(point);
        return 0;
      }
      ClientToScreen(window, &point);
      ShowContextMenu(point);
      return 0;
    }
    case WM_TIMER:
      if (wparam == kToolbarTooltipTimer) {
        KillTimer(window, kToolbarTooltipTimer);
        if (hovered_toolbar_item_ != kToolbarItemNone) {
          ShowToolbarTooltip(hovered_toolbar_item_);
        }
        return 0;
      }
      toolbar_visible_ = ReadToolbarVisibleSetting(toolbar_visible_);
      if (!toolbar_visible_) {
        DestroyToolbarWindow();
      } else {
        EnsureToolbarWindowTopmost();
      }
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      BeginPaint(window, &paint);
      EndPaint(window, &paint);
      RenderToolbarLayeredWindow();
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_NCDESTROY:
      if (toolbar_window_ == window) {
        HideToolbarTooltip();
        toolbar_window_ = nullptr;
        ReleaseToolbarWindowOwnership();
      }
      break;
    default:
      break;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TsfTextService::StaticToolbarWindowProc(HWND window,
                                                         UINT message,
                                                         WPARAM wparam,
                                                         LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window,
                      GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  auto* service =
      reinterpret_cast<TsfTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (service != nullptr) {
    return service->ToolbarWindowProc(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

POINT TsfTextService::CandidateWindowAnchorFromContext(ITfContext* context) {
  HWND context_window = nullptr;
  if (context != nullptr) {
    POINT anchor{};
    context_window = ContextViewWindow(context);
    if (RequestSelectionAnchor(client_id_, context, composition_, &anchor) &&
        IsUsableScreenPoint(anchor)) {
      LogAnchorPoint(L"tsf-range", anchor);
      return anchor;
    }

    if (TryGuiThreadCaretAnchor(context_window, &anchor)) {
      LogAnchorPoint(L"gui-caret", anchor);
      return anchor;
    }
  }

  HWND foreground = GetForegroundWindow();
  const bool foreground_matches_context =
      context_window != nullptr && IsSameRootWindow(context_window, foreground);
  POINT caret{0, 0};
  if (foreground_matches_context && TryGuiThreadCaretAnchor(foreground, &caret)) {
    LogAnchorPoint(L"foreground-caret", caret);
    return caret;
  }

  if (foreground_matches_context && GetCaretPos(&caret) && foreground != nullptr) {
    ClientToScreen(foreground, &caret);
    if (IsUsableScreenPoint(caret)) {
      LogAnchorPoint(L"thread-caret", caret);
      return caret;
    }
  }

  if (TryGuiThreadFocusAnchor(context_window, &caret)) {
    LogAnchorPoint(L"context-focus", caret);
    return caret;
  }

  if (foreground_matches_context && TryGuiThreadFocusAnchor(foreground, &caret)) {
    LogAnchorPoint(L"foreground-focus", caret);
    return caret;
  }

  if (has_last_candidate_anchor_) {
    LogAnchorPoint(L"last-good", last_candidate_anchor_);
    return last_candidate_anchor_;
  }

  RECT work_area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  caret.x = work_area.left + ScaleForDpi(120, DpiForWindow(foreground));
  caret.y = work_area.top + ScaleForDpi(120, DpiForWindow(foreground));
  LogAnchorPoint(L"work-area", caret);
  return caret;
}

void TsfTextService::RememberCandidateAnchor(POINT anchor) {
  if (!IsUsableScreenPoint(anchor)) {
    return;
  }
  last_candidate_anchor_ = anchor;
  has_last_candidate_anchor_ = true;
}

POINT TsfTextService::CandidateWindowAnchor(ITfContext* context) {
  POINT anchor = CandidateWindowAnchorFromContext(context);
  if (!IsUsableScreenPoint(anchor) && has_last_candidate_anchor_) {
    return last_candidate_anchor_;
  }
  RememberCandidateAnchor(anchor);
  return anchor;
}

void TsfTextService::ShowCandidateWindow(ITfContext* context) {
  const ULONGLONG show_start_tick = GetTickCount64();
  if (!IsComposing()) {
    HideCandidateWindow();
    return;
  }
  const bool previous_horizontal_candidate_layout = horizontal_candidate_layout_;
  const int previous_compact_candidate_count = compact_candidate_count_;
  const int previous_candidate_font_size_level = candidate_font_size_level_;
  const std::wstring previous_candidate_font_family = candidate_font_family_;
  ReloadCandidateWindowVisualSettings();
  if (previous_horizontal_candidate_layout != horizontal_candidate_layout_ ||
      previous_compact_candidate_count != compact_candidate_count_ ||
      previous_candidate_font_size_level != candidate_font_size_level_ ||
      previous_candidate_font_family != candidate_font_family_) {
    InvalidateCandidateLayoutCache();
    last_candidate_query_input_.clear();
    last_candidate_query_page_index_ = -1;
    last_candidate_query_page_size_ = 0;
  }
  HideStatusTip();

  if (EnsureCandidateWindowClass() == 0) {
    return;
  }

  if (candidate_window_ == nullptr) {
    candidate_window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                                            WS_EX_LAYERED,
                                        L"FluentPinyinCandidateWindow",
                                        L"",
                                        WS_POPUP,
                                        CW_USEDEFAULT,
                                        CW_USEDEFAULT,
                                        360,
                                        52,
                                        nullptr,
                                        nullptr,
                                        g_module_instance,
                                        this);
    if (candidate_window_ == nullptr) {
      return;
    }
    const DWORD corner_preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(candidate_window_,
                          DWMWA_WINDOW_CORNER_PREFERENCE,
                          &corner_preference,
                          sizeof(corner_preference));
  }
  ApplyCandidateDwmFrame(candidate_window_, theme_mode_, theme_preset_);

  const int requested_candidate_page_size =
      CandidatePageSizeLimit(horizontal_candidate_layout_,
                             expanded_candidate_window_,
                             compact_candidate_count_);
  const bool candidate_page_already_trimmed =
      last_candidate_query_input_ == composition_input_ &&
      last_candidate_query_page_index_ == candidate_page_index_ &&
      last_candidate_query_page_size_ > 0 &&
      last_candidate_query_page_size_ < requested_candidate_page_size &&
      candidates_.size() <= static_cast<size_t>(last_candidate_query_page_size_);
  if (!candidates_.empty() && !candidate_page_already_trimmed) {
    const size_t visible_count =
        CalculateVisibleCandidateCountForWindow(candidate_window_,
                                                horizontal_candidate_layout_,
                                                expanded_candidate_window_,
                                                compact_candidate_count_,
                                                CandidateFontPointSize(),
                                                simplified_charset_,
                                                CandidateFontFamilyFromSetting(candidate_font_family_),
                                                candidates_);
    if (visible_count > 0 && visible_count < candidates_.size() &&
        last_candidate_query_input_ == composition_input_ &&
        last_candidate_query_page_index_ == candidate_page_index_ &&
        last_candidate_query_page_size_ != static_cast<int>(visible_count)) {
      last_candidate_query_input_.clear();
      last_candidate_query_page_index_ = -1;
      last_candidate_query_page_size_ = 0;
      RefreshCandidates();
    }
  }

  POINT caret = CandidateWindowAnchor(context);
  const UINT anchor_dpi = ReadableDpiForPoint(caret);
  const RECT work_area = WorkAreaForPoint(caret);
  UINT dpi = anchor_dpi;
  CandidateLayoutMetrics layout =
      CandidateLayoutForWindowAtDpi(candidate_window_,
                                    anchor_dpi,
                                    work_area,
                                    horizontal_candidate_layout_,
                                    expanded_candidate_window_,
                                    compact_candidate_count_,
                                    CandidateFontPointSize(),
                                    simplified_charset_,
                                    CandidateFontFamilyFromSetting(candidate_font_family_),
                                    candidates_,
                                    &dpi);
  if (layout.width <= 0 || layout.height <= 0) {
    layout.width = ScaleForDpi(300, dpi);
    layout.height = ScaleForDpi(72, dpi);
  }

  const int width = layout.width;
  const int height = layout.height;
  int x = caret.x;
  const int underline_clearance = ScaleHalfDipForDpi(kCandidateAnchorUnderlineClearanceHalfDips, dpi);
  const int y_offset = ScaleHalfDipForDpi(kCandidateAnchorTopOffsetHalfDips, dpi) +
                       underline_clearance;
  int y = caret.y + y_offset;
  if (x + width > work_area.right) {
    x = work_area.right - width;
  }
  if (x < work_area.left) {
    x = work_area.left;
  }
  if (y + height > work_area.bottom) {
    y = caret.y - height - ScaleForDpi(8, dpi);
  }
  if (y < work_area.top) {
    y = work_area.top;
  }
  const bool was_visible = IsWindowVisible(candidate_window_) != FALSE;
  RECT previous_rect{};
  GetWindowRect(candidate_window_, &previous_rect);
  const bool geometry_changed = !was_visible || previous_rect.left != x || previous_rect.top != y ||
                                previous_rect.right - previous_rect.left != width ||
                                previous_rect.bottom - previous_rect.top != height;
  if (geometry_changed) {
    SetWindowPos(candidate_window_,
                 HWND_TOPMOST,
                 x,
                 y,
                 width,
                 height,
                 SWP_NOACTIVATE | (was_visible ? 0 : SWP_SHOWWINDOW));
  } else if (!was_visible) {
    ShowWindow(candidate_window_, SW_SHOWNOACTIVATE);
  }
  if (candidate_tooltip_tool_ != kCandidateToolNone) {
    PositionCandidateTooltip(candidate_tooltip_tool_);
  }
  SetTimer(candidate_window_, kCandidateWindowWatchTimer, 250, nullptr);
  {
    CandidateRenderLayoutScope layout_scope(&layout);
    RenderCandidateLayeredWindow();
  }
  LogTsfPerfIfSlow(L"ShowCandidateWindow",
                   GetTickCount64() - show_start_tick,
                   20,
                   L"width=" + std::to_wstring(width) +
                       L", height=" + std::to_wstring(height) +
                       L", candidates=" + std::to_wstring(candidates_.size()) +
                       L", geometry_changed=" +
                       (geometry_changed ? std::wstring(L"yes") : std::wstring(L"no")) +
                       L", expanded=" + (expanded_candidate_window_ ? std::wstring(L"yes")
                                                                     : std::wstring(L"no")));
}

void TsfTextService::HideCandidateWindow() {
  if (candidate_window_ != nullptr) {
    KillTimer(candidate_window_, kCandidateWindowWatchTimer);
    KillTimer(candidate_window_, kCandidateToolTooltipTimer);
    hovered_candidate_tool_ = kCandidateToolNone;
    pressed_candidate_tool_ = kCandidateToolNone;
    candidate_mouse_tracking_ = false;
    has_candidate_tooltip_anchor_ = false;
    HideCandidateTooltip();
    ShowWindow(candidate_window_, SW_HIDE);
  }
}

void TsfTextService::DestroyCandidateWindow() {
  if (candidate_window_ != nullptr) {
    HideCandidateTooltip();
    KillTimer(candidate_window_, kCandidateWindowWatchTimer);
    KillTimer(candidate_window_, kCandidateToolTooltipTimer);
    DestroyWindow(candidate_window_);
    candidate_window_ = nullptr;
  }
}

void TsfTextService::OpenCandidateSettingsMenu() {
  HideCandidateTooltip();
  DestroyContextMenu();
  OpenConfigApp();
}

bool TsfTextService::ShouldKeepCandidateWindow() const {
  if (!IsComposing() || !has_input_focus_) {
    return false;
  }
  if (!IsCurrentKeyboardProfile()) {
    return false;
  }
  HWND context_window = ContextViewWindow(active_context_);
  if (context_window == nullptr || !IsWindow(context_window)) {
    return false;
  }
  HWND foreground = GetForegroundWindow();
  if (foreground == nullptr || !IsWindow(foreground)) {
    return false;
  }
  HWND context_root = GetAncestor(context_window, GA_ROOT);
  HWND foreground_root = GetAncestor(foreground, GA_ROOT);
  if (context_root != nullptr && foreground_root != nullptr) {
    return context_root == foreground_root;
  }
  DWORD context_process_id = 0;
  GetWindowThreadProcessId(context_window, &context_process_id);
  DWORD foreground_process_id = 0;
  GetWindowThreadProcessId(foreground, &foreground_process_id);
  return context_process_id == 0 || foreground_process_id == 0 ||
         context_process_id == foreground_process_id;
}

void TsfTextService::ShowCandidateTooltip(int tool) {
  if (candidate_window_ == nullptr ||
      !IsCandidateToolEnabled(tool, has_previous_candidate_page_, has_next_candidate_page_)) {
    HideCandidateTooltip();
    return;
  }
  if (EnsureCandidateTooltipWindowClass() == 0) {
    return;
  }
  if (candidate_tooltip_window_ == nullptr) {
    candidate_tooltip_window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                                                    WS_EX_LAYERED,
                                                L"FluentPinyinCandidateTooltipWindow",
                                                L"",
                                                WS_POPUP,
                                                CW_USEDEFAULT,
                                                CW_USEDEFAULT,
                                                120,
                                                36,
                                                nullptr,
                                                nullptr,
                                                g_module_instance,
                                                this);
    if (candidate_tooltip_window_ == nullptr) {
      return;
    }
    const DWORD corner_preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(candidate_tooltip_window_,
                          DWMWA_WINDOW_CORNER_PREFERENCE,
                          &corner_preference,
                          sizeof(corner_preference));
  }
  const bool was_visible = IsWindowVisible(candidate_tooltip_window_) != FALSE;
  const int previous_tool = candidate_tooltip_tool_;
  candidate_tooltip_tool_ = tool;
  PositionCandidateTooltip(tool);
  RenderCandidateTooltipLayeredWindow();
  if (!was_visible || previous_tool != tool) {
    ShowWindow(candidate_tooltip_window_, SW_HIDE);
    if (!AnimateWindow(candidate_tooltip_window_,
                       kCandidateToolTooltipAnimationMs,
                       AW_BLEND | AW_SLIDE | AW_VER_POSITIVE)) {
      ShowWindow(candidate_tooltip_window_, SW_SHOWNOACTIVATE);
    }
  } else {
    ShowWindow(candidate_tooltip_window_, SW_SHOWNOACTIVATE);
  }
}

void TsfTextService::HideCandidateTooltip() {
  candidate_tooltip_tool_ = kCandidateToolNone;
  has_candidate_tooltip_anchor_ = false;
  if (candidate_tooltip_window_ != nullptr) {
    ShowWindow(candidate_tooltip_window_, SW_HIDE);
  }
}

void TsfTextService::PositionCandidateTooltip(int tool) {
  if (candidate_window_ == nullptr || candidate_tooltip_window_ == nullptr) {
    return;
  }
  const UINT dpi = ReadableDpiForWindow(candidate_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  HDC dc = GetDC(candidate_tooltip_window_);
  HFONT font = CreateLayeredUiFontForDpi(
      kCandidateToolTooltipFontPointSize, dpi, FW_NORMAL, UiFontFamily(!simplified_charset_));
  HGDIOBJ old_font = font != nullptr && dc != nullptr ? SelectObject(dc, font) : nullptr;
  const std::wstring text = CandidateToolTooltipText(tool, expanded_candidate_window_);
  SIZE text_size{};
  if (dc != nullptr) {
    text_size = MeasureTextWithFallback(
        dc, text, kCandidateToolTooltipFontPointSize, dpi, FW_NORMAL, !simplified_charset_);
  }
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
  if (dc != nullptr) {
    ReleaseDC(candidate_tooltip_window_, dc);
  }
  const int horizontal_padding = CandidateToolTooltipHorizontalPadding(dpi);
  const int text_overhang_guard = s(kCandidateToolTooltipOverhangGuardDips);
  const int width =
      std::max(s(54),
               static_cast<int>(text_size.cx) + horizontal_padding * 2 + text_overhang_guard);
  const int height = CandidateToolTooltipHeight(dpi);

  const CandidateLayoutMetrics layout =
      CandidateLayoutForWindow(candidate_window_,
                               horizontal_candidate_layout_,
                               expanded_candidate_window_,
                               compact_candidate_count_,
                               CandidateFontPointSize(),
                               simplified_charset_,
                               CandidateFontFamilyFromSetting(candidate_font_family_),
                               candidates_);

  RECT owner_rect{};
  GetWindowRect(candidate_window_, &owner_rect);
  const RECT tool_rect = CandidateToolRect(layout, tool);
  POINT anchor{};
  if (has_candidate_tooltip_anchor_) {
    anchor = candidate_tooltip_anchor_;
  } else {
    anchor = {owner_rect.left + tool_rect.left + (tool_rect.right - tool_rect.left) / 2,
              owner_rect.top + tool_rect.bottom};
  }
  int x = anchor.x + s(4);
  int y = anchor.y + s(7);
  RECT work_area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  if (x + width > work_area.right) {
    x = anchor.x - width - s(4);
  }
  if (x < work_area.left) {
    x = work_area.left;
  }
  if (y + height > work_area.bottom) {
    y = anchor.y - height - s(7);
  }
  SetWindowPos(candidate_tooltip_window_,
               HWND_TOPMOST,
               x,
               y,
               width,
               height,
               SWP_NOACTIVATE);
  SetWindowRgn(candidate_tooltip_window_, nullptr, FALSE);
}

void TsfTextService::DrawCandidateTooltip(HDC dc) {
  RECT client{};
  GetClientRect(candidate_tooltip_window_, &client);
  const UINT dpi = ReadableDpiForWindow(candidate_tooltip_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const ToolbarPalette palette = ToolbarPaletteForTheme(theme_mode_, theme_preset_);
  DrawFloatingWindowSurface(dc,
                            client,
                            CandidateToolTooltipCornerRadius(dpi),
                            dpi,
                            palette.border,
                            palette.tooltip_background);

  HFONT font = CreateLayeredUiFontForDpi(
      kCandidateToolTooltipFontPointSize, dpi, FW_NORMAL, UiFontFamily(!simplified_charset_));
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, palette.tooltip_text);
  const int horizontal_padding = CandidateToolTooltipHorizontalPadding(dpi);
  RECT text_rect{client.left + horizontal_padding,
                 client.top,
                 client.right - horizontal_padding,
                 client.bottom - s(1)};
  const std::wstring text =
      candidate_tooltip_tool_ != kCandidateToolNone
          ? CandidateToolTooltipText(candidate_tooltip_tool_, expanded_candidate_window_)
          : L"";
  DrawTextWithFallback(
      dc,
      text,
      &text_rect,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
      kCandidateToolTooltipFontPointSize,
      dpi,
      FW_NORMAL,
      !simplified_charset_);
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
}

void TsfTextService::RenderCandidateTooltipLayeredWindow() {
  if (candidate_tooltip_window_ == nullptr) {
    return;
  }
  const UINT dpi = ReadableDpiForWindow(candidate_tooltip_window_);
  RenderRoundedLayeredWindow(candidate_tooltip_window_,
                             CandidateToolTooltipCornerRadius(dpi),
                             ToolbarPaletteForTheme(theme_mode_, theme_preset_).border,
                             [this](HDC dc) { DrawCandidateTooltip(dc); });
}

ATOM TsfTextService::EnsureCandidateTooltipWindowClass() {
  static ATOM atom = 0;
  if (atom != 0) {
    return atom;
  }

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.style = CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc = StaticCandidateTooltipWindowProc;
  window_class.hInstance = g_module_instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = L"FluentPinyinCandidateTooltipWindow";
  atom = RegisterClassExW(&window_class);
  return atom;
}

LRESULT TsfTextService::CandidateTooltipWindowProc(HWND window,
                                                   UINT message,
                                                   WPARAM wparam,
                                                   LPARAM lparam) {
  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_DPICHANGED:
      ApplySuggestedDpiRect(window, lparam);
      if (candidate_tooltip_tool_ != kCandidateToolNone) {
        PositionCandidateTooltip(candidate_tooltip_tool_);
      }
      RenderCandidateTooltipLayeredWindow();
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      BeginPaint(window, &paint);
      EndPaint(window, &paint);
      RenderCandidateTooltipLayeredWindow();
      return 0;
    }
    case WM_NCDESTROY:
      if (candidate_tooltip_window_ == window) {
        candidate_tooltip_window_ = nullptr;
        candidate_tooltip_tool_ = kCandidateToolNone;
      }
      break;
    default:
      break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TsfTextService::StaticCandidateTooltipWindowProc(HWND window,
                                                                  UINT message,
                                                                  WPARAM wparam,
                                                                  LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window,
                      GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  auto* service =
      reinterpret_cast<TsfTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (service != nullptr) {
    return service->CandidateTooltipWindowProc(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

std::wstring TsfTextService::StatusTipText() const {
  if (ascii_mode_) {
    return caps_lock_ascii_mode_ ? L"En" : L"en";
  }
  return simplified_charset_ ? L"\u7B80" : L"\u7E41";
}

bool TsfTextService::IsStatusTipAllowed(ITfContext* context) const {
  if (!status_tip_enabled_ || !has_input_focus_) {
    return false;
  }

  ITfContext* status_context = context != nullptr ? context : active_context_;
  HWND context_window = ContextViewWindow(status_context);
  HWND foreground = GetForegroundWindow();
  const bool foreground_matches_context =
      context_window != nullptr && IsSameRootWindow(context_window, foreground);

  POINT anchor{};
  bool has_input_caret = false;
  if (status_context != nullptr &&
      (foreground_matches_context || context_window == nullptr)) {
    has_input_caret =
        RequestSelectionAnchor(client_id_, status_context, composition_, &anchor) &&
        IsUsableScreenPoint(anchor);
  }
  if (!has_input_caret && foreground_matches_context) {
    has_input_caret = TryGuiThreadCaretAnchor(context_window, &anchor) ||
                      TryGuiThreadCaretAnchor(foreground, &anchor);
  }
  if (!has_input_caret && status_context == nullptr) {
    has_input_caret = TryGuiThreadCaretAnchor(foreground, &anchor);
  }
  if (!has_input_caret) {
    return false;
  }

  std::wstring process_name;
  if (status_context != nullptr) {
    process_name = ProcessNameFromWindow(context_window);
  }
  if (process_name.empty() && foreground_matches_context) {
    process_name = ProcessNameFromWindow(foreground);
  }
  if (process_name.empty()) {
    process_name = ForegroundProcessName();
  }
  if (process_name.empty()) {
    return true;
  }
  return !ProcessNameInList(process_name, status_tip_blacklist_);
}

void TsfTextService::ShowStatusTip(ITfContext* context, StatusTipDetail detail) {
  LoadUserSettings();
  if (IsComposing() || !IsStatusTipAllowed(context)) {
    HideStatusTip();
    return;
  }
  if (EnsureStatusTipWindowClass() == 0) {
    return;
  }

  if (status_tip_window_ == nullptr) {
    status_tip_window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                                             WS_EX_LAYERED,
                                         L"FluentPinyinStatusTipWindow",
                                         L"",
                                         WS_POPUP,
                                         CW_USEDEFAULT,
                                         CW_USEDEFAULT,
                                         96,
                                         30,
                                         nullptr,
                                         nullptr,
                                         g_module_instance,
                                         this);
    if (status_tip_window_ == nullptr) {
      return;
    }
    const DWORD corner_preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(status_tip_window_,
                          DWMWA_WINDOW_CORNER_PREFERENCE,
                          &corner_preference,
                          sizeof(corner_preference));
  }
  ApplyCandidateDwmFrame(status_tip_window_, theme_mode_, theme_preset_);

  if (detail != StatusTipDetail::kNone) {
    switch (detail) {
      case StatusTipDetail::kChinesePunctuation:
        status_tip_icon_mode_ = StatusTipIconMode::kChinesePunctuation;
        break;
      case StatusTipDetail::kEnglishPunctuation:
        status_tip_icon_mode_ = StatusTipIconMode::kEnglishPunctuation;
        break;
      case StatusTipDetail::kFullShape:
        status_tip_icon_mode_ = StatusTipIconMode::kFullShape;
        break;
      case StatusTipDetail::kHalfShape:
        status_tip_icon_mode_ = StatusTipIconMode::kHalfShape;
        break;
      case StatusTipDetail::kNone:
        break;
    }
  }
  status_tip_detail_ = detail;
  PositionStatusTip(context);
  SetTimer(status_tip_window_, kStatusTipHideTimer, kStatusTipHideDelayMs, nullptr);
  RenderStatusTipLayeredWindow();
}

void TsfTextService::HideStatusTip() {
  if (status_tip_window_ != nullptr) {
    KillTimer(status_tip_window_, kStatusTipHideTimer);
    ShowWindow(status_tip_window_, SW_HIDE);
  }
  status_tip_detail_ = StatusTipDetail::kNone;
}

void TsfTextService::DestroyStatusTip() {
  if (status_tip_window_ != nullptr) {
    KillTimer(status_tip_window_, kStatusTipHideTimer);
    DestroyWindow(status_tip_window_);
    status_tip_window_ = nullptr;
  }
}

void TsfTextService::PositionStatusTip(ITfContext* context) {
  (void)context;
  if (status_tip_window_ == nullptr) {
    return;
  }

  const UINT dpi = ReadableDpiForWindow(status_tip_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const int height = CandidateToolTooltipHeight(dpi);
  const int horizontal_padding = CandidateToolTooltipHorizontalPadding(dpi);
  const int text_icon_gap = s(kStatusTipTextIconGapDips);
  const int icon_size = StatusTipDetailIconSize(dpi);
  int width = s(54);

  HDC dc = GetDC(status_tip_window_);
  HFONT font = CreateLayeredUiFontForDpi(
      kCandidateToolTooltipFontPointSize, dpi, FW_NORMAL, UiFontFamily(!simplified_charset_));
  HGDIOBJ old_font = font != nullptr && dc != nullptr ? SelectObject(dc, font) : nullptr;
  if (dc != nullptr) {
    const std::wstring text = StatusTipText();
    const SIZE text_size =
        MeasureTextWithFallback(
            dc, text, kCandidateToolTooltipFontPointSize, dpi, FW_NORMAL, !simplified_charset_);
    width = std::max(s(54),
                     static_cast<int>(text_size.cx) + horizontal_padding * 2 +
                         text_icon_gap + icon_size);
  }
  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
  if (dc != nullptr) {
    ReleaseDC(status_tip_window_, dc);
  }

  POINT anchor{};
  if (!GetCursorPos(&anchor) || !IsUsableScreenPoint(anchor)) {
    RECT work_area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    anchor.x = work_area.left + s(120);
    anchor.y = work_area.top + s(120);
  }
  RECT work_area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);

  int x = anchor.x + s(kStatusTipMouseOffsetXDips);
  int y = anchor.y + s(kStatusTipMouseOffsetYDips);
  if (x + width > work_area.right) {
    x = anchor.x - width - s(kStatusTipMouseOffsetXDips);
  }
  if (x < work_area.left) {
    x = work_area.left;
  }
  if (y + height > work_area.bottom) {
    y = anchor.y - height - s(kStatusTipMouseOffsetYDips);
  }
  if (y < work_area.top) {
    y = work_area.top;
  }

  SetWindowPos(status_tip_window_,
               HWND_TOPMOST,
               x,
               y,
               width,
               height,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  SetWindowRgn(status_tip_window_, nullptr, FALSE);
}

void TsfTextService::DrawStatusTip(HDC dc) {
  RECT client{};
  GetClientRect(status_tip_window_, &client);
  const UINT dpi = ReadableDpiForWindow(status_tip_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const int corner_radius = CandidateToolTooltipCornerRadius(dpi);
  const ToolbarPalette palette = ToolbarPaletteForTheme(theme_mode_, theme_preset_);
  DrawFloatingWindowSurface(dc,
                            client,
                            corner_radius,
                            dpi,
                            palette.border,
                            palette.tooltip_background);

  HFONT font = CreateLayeredUiFontForDpi(
      kCandidateToolTooltipFontPointSize, dpi, FW_NORMAL, UiFontFamily(!simplified_charset_));
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, palette.tooltip_text);
  const int horizontal_padding = CandidateToolTooltipHorizontalPadding(dpi);
  const int text_icon_gap = s(kStatusTipTextIconGapDips);
  const int icon_size = StatusTipDetailIconSize(dpi);
  const int center_y = client.top + (client.bottom - client.top) / 2;
  RECT detail_rect{client.right - horizontal_padding - icon_size,
                   center_y - icon_size / 2,
                   client.right - horizontal_padding,
                   center_y - icon_size / 2 + icon_size};
  RECT text_rect{client.left + horizontal_padding,
                 client.top,
                 detail_rect.left - text_icon_gap,
                 client.bottom - s(1)};
  const std::wstring text = StatusTipText();
  DrawTextWithFallback(
      dc,
      text,
      &text_rect,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
      kCandidateToolTooltipFontPointSize,
      dpi,
      FW_NORMAL,
      !simplified_charset_);

  StatusTipIconMode effective_icon_mode = status_tip_icon_mode_;
  if (effective_icon_mode == StatusTipIconMode::kChinesePunctuation ||
      effective_icon_mode == StatusTipIconMode::kEnglishPunctuation) {
    effective_icon_mode = (ascii_mode_ || !chinese_punctuation_mode_)
                              ? StatusTipIconMode::kEnglishPunctuation
                              : StatusTipIconMode::kChinesePunctuation;
  }
  switch (effective_icon_mode) {
    case StatusTipIconMode::kChinesePunctuation:
    case StatusTipIconMode::kEnglishPunctuation:
      DrawStatusPunctuationModeIcon(
          dc,
          detail_rect,
          effective_icon_mode == StatusTipIconMode::kChinesePunctuation,
          palette.tooltip_text,
          s(1));
      break;
    case StatusTipIconMode::kFullShape:
    case StatusTipIconMode::kHalfShape:
      DrawShapeModeIcon(dc,
                        detail_rect,
                        effective_icon_mode == StatusTipIconMode::kFullShape,
                        palette.tooltip_text,
                        s(1));
      break;
  }

  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
}

void TsfTextService::RenderStatusTipLayeredWindow() {
  if (status_tip_window_ == nullptr) {
    return;
  }
  const UINT dpi = ReadableDpiForWindow(status_tip_window_);
  RenderRoundedLayeredWindow(status_tip_window_,
                             CandidateToolTooltipCornerRadius(dpi),
                             ToolbarPaletteForTheme(theme_mode_, theme_preset_).border,
                             [this](HDC dc) { DrawStatusTip(dc); });
}

ATOM TsfTextService::EnsureStatusTipWindowClass() {
  static ATOM atom = 0;
  if (atom != 0) {
    return atom;
  }

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = StaticStatusTipWindowProc;
  window_class.hInstance = g_module_instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = L"FluentPinyinStatusTipWindow";
  atom = RegisterClassExW(&window_class);
  return atom;
}

LRESULT TsfTextService::StatusTipWindowProc(HWND window,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam) {
  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_DPICHANGED:
      ApplySuggestedDpiRect(window, lparam);
      PositionStatusTip(active_context_);
      RenderStatusTipLayeredWindow();
      return 0;
    case WM_TIMER:
      if (wparam == kStatusTipHideTimer) {
        HideStatusTip();
        return 0;
      }
      break;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      BeginPaint(window, &paint);
      EndPaint(window, &paint);
      RenderStatusTipLayeredWindow();
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_NCDESTROY:
      if (status_tip_window_ == window) {
        status_tip_window_ = nullptr;
      }
      break;
    default:
      break;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TsfTextService::StaticStatusTipWindowProc(HWND window,
                                                           UINT message,
                                                           WPARAM wparam,
                                                           LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window,
                      GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  auto* service =
      reinterpret_cast<TsfTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (service != nullptr) {
    return service->StatusTipWindowProc(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

void TsfTextService::DrawCandidateWindow(HDC dc) {
  RECT client{};
  GetClientRect(candidate_window_, &client);
  const UINT dpi = ReadableDpiForWindow(candidate_window_);
  auto s = [dpi](int value) { return ScaleForDpi(value, dpi); };
  const int candidate_font_point_size = CandidateFontPointSize();
  const CandidateWindowPalette palette = CandidatePalette(theme_mode_, theme_preset_);
  HFONT font =
      CreateUiFontForDpi(candidate_font_point_size,
                         dpi,
                         FW_NORMAL,
                         CandidateUiFontFamily(CandidateFontFamilyFromSetting(candidate_font_family_),
                                               !simplified_charset_));
  HGDIOBJ old_font = font != nullptr ? SelectObject(dc, font) : nullptr;
  const CandidateLayoutMetrics calculated_layout =
      g_candidate_render_layout == nullptr
          ? CalculateCandidateLayout(candidate_window_,
                                     dc,
                                     dpi,
                                     horizontal_candidate_layout_,
                                     expanded_candidate_window_,
                                     compact_candidate_count_,
                                     CandidateFontPointSize(),
                                     !simplified_charset_,
                                     candidates_)
          : CandidateLayoutMetrics{};
  const CandidateLayoutMetrics& layout =
      g_candidate_render_layout != nullptr ? *g_candidate_render_layout : calculated_layout;

  RECT edge{client.left, client.top, client.right, client.bottom};
  const int corner_radius = s(8);
  DrawFloatingWindowSurface(dc,
                            edge,
                            corner_radius,
                            dpi,
                            palette.border,
                            palette.background);

  SetBkMode(dc, TRANSPARENT);

  const size_t count = std::min(candidates_.size(), layout.candidate_rects.size());
  if (count == 0) {
    SetTextColor(dc, palette.muted);
    RECT empty_rect{s(16), layout.candidate_top, client.right - s(16), client.bottom - s(10)};
    DrawTextWithFallback(dc,
                         L"\u65E0\u5019\u9009",
                         &empty_rect,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                         candidate_font_point_size,
                         dpi,
                         FW_NORMAL,
                         !simplified_charset_);
  } else {
    for (size_t index = 0; index < count; ++index) {
      RECT item_rect = layout.candidate_rects[index];
      if (item_rect.right <= item_rect.left) {
        continue;
      }
      if (index == selected_candidate_index_) {
        const bool compact_horizontal =
            horizontal_candidate_layout_ &&
            (!expanded_candidate_window_ ||
             (index < layout.candidate_rows.size() && layout.candidate_rows[index] == 0));
        const bool compact_vertical =
            !horizontal_candidate_layout_ && !expanded_candidate_window_;
        const int highlight_top_inset =
            (compact_horizontal || compact_vertical) ? ScaleHalfDipForDpi(3, dpi) : s(1);
        const int highlight_bottom_inset =
            (compact_horizontal || compact_vertical) ? ScaleHalfDipForDpi(7, dpi) : s(1);
        RECT highlight_rect{item_rect.left,
                            item_rect.top + highlight_top_inset,
                            item_rect.right,
                            item_rect.bottom - highlight_bottom_inset};
        FillRoundedRectAntialias(dc, highlight_rect, s(4), palette.highlight);
        const int mark_width = ScaleHalfDipForDpi(7, dpi);
        const int item_height =
            std::max(1, static_cast<int>(item_rect.bottom - item_rect.top));
        const int highlight_height =
            std::max(1, static_cast<int>(highlight_rect.bottom - highlight_rect.top));
        const int mark_min_height = ScaleHalfDipForDpi(18, dpi);
        const int mark_max_height =
            std::max(mark_min_height, highlight_height - ScaleHalfDipForDpi(3, dpi));
        const int mark_height =
            std::clamp(ScaleCandidateSelectionMark(ScaleHalfDipForDpi(31, dpi), item_height, dpi),
                       mark_min_height,
                       mark_max_height);
        const int mark_left =
            item_rect.left +
            ((compact_horizontal || compact_vertical) ? ScaleHalfDipForDpi(1, dpi) : s(1));
        const int mark_top =
            highlight_rect.top +
            std::max(0, (highlight_height - mark_height) / 2);
        RECT mark_rect{mark_left,
                       mark_top,
                       mark_left + mark_width,
                       mark_top + mark_height};
        FillRoundedRectAntialias(dc, mark_rect, mark_width, palette.accent);
      }

      SetTextColor(dc, palette.candidate_number);
      int number_base = static_cast<int>(index) % kMaxCompactCandidateCount;
      if (expanded_candidate_window_) {
        if (!horizontal_candidate_layout_ && index < layout.candidate_rows.size()) {
          number_base = layout.candidate_rows[index];
        } else if (index < layout.candidate_columns.size()) {
          number_base = layout.candidate_columns[index];
        } else {
          number_base = static_cast<int>(index) % std::max(1, layout.expanded_columns);
        }
      }
      std::wstring number = std::to_wstring(number_base + 1);
      const bool expanded_tail_candidate =
          expanded_candidate_window_ &&
          horizontal_candidate_layout_ &&
          (index >= layout.candidate_rows.size() ||
           layout.candidate_rows[index] != 0);
      const bool show_number = number_base >= 0 && number_base < kMaxCompactCandidateCount;
      const bool compact_vertical =
          !horizontal_candidate_layout_;
      const int number_left =
          item_rect.left +
          (expanded_tail_candidate
               ? s(8)
               : (compact_vertical
                      ? s(13)
                      : (index == selected_candidate_index_ ? s(13) : s(9))));
      const int number_width =
          show_number ? std::max(s(9),
                                  static_cast<int>(MeasureTextWithFallback(dc,
                                                                           number,
                                                                           candidate_font_point_size,
                                                                           dpi,
                                                                           FW_NORMAL,
                                                                           !simplified_charset_)
                                                       .cx))
                      : 0;
      const int number_right = number_left + number_width;
      if (show_number) {
        RECT number_rect{number_left,
                         item_rect.top + s(3),
                         number_right,
                         item_rect.bottom - s(3)};
        DrawTextWithFallback(dc,
                             number,
                             &number_rect,
                             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                             candidate_font_point_size,
                             dpi,
                             FW_NORMAL,
                             !simplified_charset_);
      }

      SetTextColor(dc, palette.text);
      const bool compact_horizontal_text =
          horizontal_candidate_layout_ &&
          (!expanded_candidate_window_ ||
           (index < layout.candidate_rows.size() && layout.candidate_rows[index] == 0));
      const int text_left =
          show_number ? number_right +
                            (expanded_tail_candidate
                                 ? s(5)
                                 : ((compact_horizontal_text || compact_vertical) ? s(2) : s(5)))
                      : item_rect.left + s(8);
      const int text_right_inset =
          expanded_tail_candidate || horizontal_candidate_layout_ ? s(6) : s(12);
      RECT text_rect{text_left,
                     item_rect.top + s(3),
                     item_rect.right - text_right_inset,
                     item_rect.bottom - s(3)};
      const int available_text_width =
          std::max(0, static_cast<int>(text_rect.right - text_rect.left));
      const int measured_text_width =
          static_cast<int>(MeasureTextWithFallback(dc,
                                                   candidates_[index].text,
                                                   candidate_font_point_size,
                                                   dpi,
                                                   FW_NORMAL,
                                                   !simplified_charset_)
                               .cx);
      const UINT text_format =
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
          (measured_text_width > available_text_width ? DT_END_ELLIPSIS : 0);
      DrawTextWithFallback(dc,
                           candidates_[index].text,
                           &text_rect,
                           text_format,
                           candidate_font_point_size,
                           dpi,
                           FW_NORMAL,
                           !simplified_charset_);

      if (!horizontal_candidate_layout_ && !expanded_candidate_window_ &&
          !candidates_[index].comment.empty()) {
        SetTextColor(dc, palette.muted);
        RECT comment_rect{item_rect.right - s(108),
                          item_rect.top + s(4),
                          item_rect.right - s(12),
                          item_rect.bottom - s(4)};
        DrawTextWithFallback(dc,
                             candidates_[index].comment,
                             &comment_rect,
                             DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS,
                             candidate_font_point_size,
                             dpi,
                             FW_NORMAL,
                             !simplified_charset_);
      }
    }

  }

  const COLORREF separator_color = palette.separator;
  FillSolidRect(dc, layout.tool_separator_rect, separator_color);
  if (!horizontal_candidate_layout_) {
    for (const RECT& separator_rect : layout.tool_row_separator_rects) {
      FillSolidRect(dc, separator_rect, separator_color);
    }
    if (expanded_candidate_window_) {
      const COLORREF expanded_header_separator_color = palette.expanded_separator;
      FillSolidRect(dc, layout.expanded_header_separator_rect, expanded_header_separator_color);
      for (const RECT& separator_rect : layout.expanded_vertical_separator_rects) {
        FillSolidRect(dc, separator_rect, separator_color);
      }
      FillSolidRect(dc, layout.expanded_footer_separator_rect, separator_color);
    }
  } else {
    FillSolidRect(dc, layout.emoji_separator_rect, separator_color);
    FillSolidRect(dc, layout.expand_separator_rect, separator_color);
    const COLORREF expanded_header_separator_color = palette.expanded_separator;
    FillSolidRect(dc, layout.expanded_header_separator_rect, expanded_header_separator_color);
    for (const RECT& separator_rect : layout.expanded_horizontal_separator_rects) {
      FillSolidRect(dc, separator_rect, separator_color);
    }
    FillSolidRect(dc, layout.expanded_footer_separator_rect, separator_color);
  }
  DrawCandidateToolFeedback(dc,
                             layout,
                             dpi,
                             hovered_candidate_tool_,
                             pressed_candidate_tool_,
                             !horizontal_candidate_layout_,
                             palette.tool_hover,
                             palette.tool_pressed);
  const COLORREF page_active_color = palette.page_active;
  const COLORREF page_disabled_color = palette.page_disabled;
  const COLORREF tool_icon_color = palette.tool_icon;
  const bool center_page_icons = true;
  DrawCandidatePageTriangle(dc,
                            CandidateToolIconRect(layout, kCandidateToolPrevious),
                            false,
                            has_previous_candidate_page_ ? page_active_color : page_disabled_color,
                            center_page_icons,
                            horizontal_candidate_layout_,
                            !horizontal_candidate_layout_);
  DrawCandidatePageTriangle(dc,
                            CandidateToolIconRect(layout, kCandidateToolNext),
                            true,
                            has_next_candidate_page_ ? page_active_color : page_disabled_color,
                            center_page_icons,
                            horizontal_candidate_layout_,
                            !horizontal_candidate_layout_);
  DrawCandidateBoardHeartIcon(dc,
                              CandidateToolIconRect(layout, kCandidateToolEmoji),
                              tool_icon_color,
                              s(2));
  DrawExpandChevron(dc,
                    CandidateToolIconRect(layout, kCandidateToolExpand),
                    expanded_candidate_window_,
                    !horizontal_candidate_layout_ ? (expanded_candidate_window_ ? -1 : 1) : 0,
                    tool_icon_color,
                    s(2));
  if (expanded_candidate_window_ && layout.brand_rect.right > layout.brand_rect.left) {
    const bool brand_light_icon =
        fp::EffectiveThemePreset(theme_mode_, theme_preset_, AppsUseLightTheme()) ==
        fp::kThemePresetDefaultLight;
    HICON brand_icon =
        CachedFluentPinyinIcon(brand_light_icon,
                               layout.brand_icon_rect.right - layout.brand_icon_rect.left,
                               layout.brand_icon_rect.bottom - layout.brand_icon_rect.top);
    if (brand_icon != nullptr) {
      DrawIconEx(dc,
                 layout.brand_icon_rect.left,
                 layout.brand_icon_rect.top,
                 brand_icon,
                 layout.brand_icon_rect.right - layout.brand_icon_rect.left,
                 layout.brand_icon_rect.bottom - layout.brand_icon_rect.top,
                 0,
                 nullptr,
                 DI_NORMAL);
    }
    SetTextColor(dc, palette.text);
    if (!horizontal_candidate_layout_) {
      std::wstring vertical_brand;
      const std::wstring brand_text = L"\u6D41\u7545\u62FC\u97F3";
      for (wchar_t ch : brand_text) {
        if (!vertical_brand.empty()) {
          vertical_brand += L"\n";
        }
        vertical_brand += ch;
      }
      RECT brand_text_rect = layout.brand_text_rect;
      DrawTextWithFallback(dc,
                           vertical_brand,
                           &brand_text_rect,
                           DT_CENTER | DT_TOP | DT_NOPREFIX,
                           kHorizontalExpandedBrandTextPointSize,
                           dpi,
                           FW_NORMAL,
                           !simplified_charset_);
    } else {
      RECT brand_text_rect = layout.brand_text_rect;
      DrawTextWithFallback(dc,
                           L"\u6D41\u7545\u62FC\u97F3",
                           &brand_text_rect,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS,
                           kHorizontalExpandedBrandTextPointSize,
                           dpi,
                           FW_NORMAL,
                           !simplified_charset_);
    }
  }
  if (layout.settings_rect.right > layout.settings_rect.left) {
    DrawSettingsIcon(dc, layout.settings_rect, tool_icon_color, s(2));
  }

  if (old_font != nullptr) {
    SelectObject(dc, old_font);
  }
  if (font != nullptr) {
    DeleteObject(font);
  }
}

void TsfTextService::RenderCandidateLayeredWindow() {
  if (candidate_window_ == nullptr) {
    return;
  }
  const ULONGLONG render_start_tick = GetTickCount64();
  const UINT dpi = ReadableDpiForWindow(candidate_window_);
  RenderRoundedLayeredWindow(candidate_window_,
                             ScaleForDpi(8, dpi),
                             CandidatePalette(theme_mode_, theme_preset_).border,
                             [this](HDC dc) { DrawCandidateWindow(dc); });
  LogTsfPerfIfSlow(L"RenderCandidateLayeredWindow",
                   GetTickCount64() - render_start_tick,
                   16,
                   L"candidates=" + std::to_wstring(candidates_.size()) +
                       L", expanded=" + (expanded_candidate_window_ ? std::wstring(L"yes")
                                                                     : std::wstring(L"no")));
}

ATOM TsfTextService::EnsureCandidateWindowClass() {
  static ATOM atom = 0;
  if (atom != 0) {
    return atom;
  }

  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.style = 0;
  window_class.cbWndExtra = kCandidateWindowClassExtraLastHeight + sizeof(LONG_PTR);
  window_class.lpfnWndProc = StaticCandidateWindowProc;
  window_class.hInstance = g_module_instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = L"FluentPinyinCandidateWindow";
  atom = RegisterClassExW(&window_class);
  return atom;
}

LRESULT TsfTextService::CandidateWindowProc(HWND window,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam) {
  switch (message) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_DPICHANGED:
      InvalidateCandidateLayoutCache();
      ApplySuggestedDpiRect(window, lparam);
      if (candidate_window_ == window && IsWindowVisible(window) && IsComposing()) {
        ShowCandidateWindow(active_context_);
      } else {
        RenderCandidateLayeredWindow();
      }
      return 0;
    case WM_LBUTTONDOWN: {
      const int y = static_cast<short>(HIWORD(lparam));
      const int x = static_cast<short>(LOWORD(lparam));
      const CandidateLayoutMetrics layout =
          CandidateLayoutForWindow(window,
                                   horizontal_candidate_layout_,
                                   expanded_candidate_window_,
                                   compact_candidate_count_,
                                   CandidateFontPointSize(),
                                   simplified_charset_,
                                   CandidateFontFamilyFromSetting(candidate_font_family_),
                                   candidates_);

      const int tool = CandidateToolAtPoint(layout, x, y);
      if (tool != kCandidateToolNone &&
          IsCandidateToolEnabled(tool, has_previous_candidate_page_, has_next_candidate_page_)) {
        pressed_candidate_tool_ = tool;
        hovered_candidate_tool_ = tool;
        POINT cursor{};
        has_candidate_tooltip_anchor_ = GetCursorPos(&cursor) != FALSE;
        if (has_candidate_tooltip_anchor_) {
          candidate_tooltip_anchor_ = cursor;
        }
        KillTimer(window, kCandidateToolTooltipTimer);
        candidate_tooltip_tool_ = kCandidateToolNone;
        if (candidate_tooltip_window_ != nullptr) {
          ShowWindow(candidate_tooltip_window_, SW_HIDE);
        }
        SetCapture(window);
        RenderCandidateLayeredWindow();
        return 0;
      }

      for (size_t index = 0; index < layout.candidate_rects.size(); ++index) {
        const RECT& rect = layout.candidate_rects[index];
        if (index < candidates_.size() && IsSelectableCandidateRect(rect) &&
            PtInRectInclusive(rect, x, y)) {
          selected_candidate_index_ = index;
          CommitCandidateFromMouse(index);
          break;
        }
      }
      return 0;
    }
    case WM_MOUSEMOVE: {
      const int y = static_cast<short>(HIWORD(lparam));
      const int x = static_cast<short>(LOWORD(lparam));
      const CandidateLayoutMetrics layout =
          CandidateLayoutForWindow(window,
                                   horizontal_candidate_layout_,
                                   expanded_candidate_window_,
                                   compact_candidate_count_,
                                   CandidateFontPointSize(),
                                   simplified_charset_,
                                   CandidateFontFamilyFromSetting(candidate_font_family_),
                                   candidates_);

      const int previous_hovered = hovered_candidate_tool_;
      const int hit_tool = CandidateToolAtPoint(layout, x, y);
      const int tool =
          IsCandidateToolEnabled(hit_tool, has_previous_candidate_page_, has_next_candidate_page_)
              ? hit_tool
              : kCandidateToolNone;
      hovered_candidate_tool_ = tool;
      POINT cursor{};
      if (hovered_candidate_tool_ != kCandidateToolNone && GetCursorPos(&cursor)) {
        candidate_tooltip_anchor_ = cursor;
        has_candidate_tooltip_anchor_ = true;
      } else if (hovered_candidate_tool_ == kCandidateToolNone) {
        has_candidate_tooltip_anchor_ = false;
      }
      if (!candidate_mouse_tracking_) {
        TRACKMOUSEEVENT event{};
        event.cbSize = sizeof(event);
        event.dwFlags = TME_LEAVE;
        event.hwndTrack = window;
        candidate_mouse_tracking_ = TrackMouseEvent(&event) != FALSE;
      }
      if (previous_hovered != hovered_candidate_tool_) {
        KillTimer(window, kCandidateToolTooltipTimer);
        candidate_tooltip_tool_ = kCandidateToolNone;
        if (candidate_tooltip_window_ != nullptr) {
          ShowWindow(candidate_tooltip_window_, SW_HIDE);
        }
        if (hovered_candidate_tool_ != kCandidateToolNone) {
          SetTimer(window, kCandidateToolTooltipTimer, kCandidateToolTooltipDelayMs, nullptr);
        } else {
          has_candidate_tooltip_anchor_ = false;
        }
        RenderCandidateLayeredWindow();
      } else if (hovered_candidate_tool_ != kCandidateToolNone &&
                 candidate_tooltip_tool_ == hovered_candidate_tool_ &&
                 candidate_tooltip_window_ != nullptr &&
                 IsWindowVisible(candidate_tooltip_window_)) {
        PositionCandidateTooltip(hovered_candidate_tool_);
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      candidate_mouse_tracking_ = false;
      KillTimer(window, kCandidateToolTooltipTimer);
      if (hovered_candidate_tool_ != kCandidateToolNone) {
        hovered_candidate_tool_ = kCandidateToolNone;
        has_candidate_tooltip_anchor_ = false;
        HideCandidateTooltip();
        RenderCandidateLayeredWindow();
      }
      return 0;
    case WM_LBUTTONUP: {
      if (pressed_candidate_tool_ != kCandidateToolNone) {
        const int pressed_tool = pressed_candidate_tool_;
        pressed_candidate_tool_ = kCandidateToolNone;
        if (GetCapture() == window) {
          ReleaseCapture();
        }

        const int y = static_cast<short>(HIWORD(lparam));
        const int x = static_cast<short>(LOWORD(lparam));
        const CandidateLayoutMetrics layout =
            CandidateLayoutForWindow(window,
                                     horizontal_candidate_layout_,
                                     expanded_candidate_window_,
                                     compact_candidate_count_,
                                     CandidateFontPointSize(),
                                     simplified_charset_,
                                     CandidateFontFamilyFromSetting(candidate_font_family_),
                                     candidates_);

        const int hit_tool = CandidateToolAtPoint(layout, x, y);
        const int released_tool =
            IsCandidateToolEnabled(hit_tool, has_previous_candidate_page_, has_next_candidate_page_)
                ? hit_tool
                : kCandidateToolNone;
        hovered_candidate_tool_ = released_tool;
        POINT cursor{};
        if (hovered_candidate_tool_ != kCandidateToolNone && GetCursorPos(&cursor)) {
          candidate_tooltip_anchor_ = cursor;
          has_candidate_tooltip_anchor_ = true;
        } else if (hovered_candidate_tool_ == kCandidateToolNone) {
          has_candidate_tooltip_anchor_ = false;
        }
        KillTimer(window, kCandidateToolTooltipTimer);
        candidate_tooltip_tool_ = kCandidateToolNone;
        if (candidate_tooltip_window_ != nullptr) {
          ShowWindow(candidate_tooltip_window_, SW_HIDE);
        }
        if (hovered_candidate_tool_ != kCandidateToolNone) {
          SetTimer(window, kCandidateToolTooltipTimer, kCandidateToolTooltipDelayMs, nullptr);
        } else {
          has_candidate_tooltip_anchor_ = false;
        }
        RenderCandidateLayeredWindow();
        if (released_tool == pressed_tool) {
          if (pressed_tool == kCandidateToolEmoji) {
            OpenEmojiPanel();
          } else if (pressed_tool == kCandidateToolExpand) {
            SetCandidateExpansion(active_context_, !expanded_candidate_window_);
          } else if (pressed_tool == kCandidateToolPrevious && has_previous_candidate_page_) {
            ChangeCandidatePage(active_context_, -1);
          } else if (pressed_tool == kCandidateToolNext && has_next_candidate_page_) {
            ChangeCandidatePage(active_context_, 1);
          } else if (pressed_tool == kCandidateToolSettings) {
            OpenCandidateSettingsMenu();
          } else if (pressed_tool == kCandidateToolBrand) {
            OpenFluentPinyinWebsite();
          }
        }
        return 0;
      }
      break;
    }
    case WM_CAPTURECHANGED:
      KillTimer(window, kCandidateToolTooltipTimer);
      if (pressed_candidate_tool_ != kCandidateToolNone) {
        pressed_candidate_tool_ = kCandidateToolNone;
        RenderCandidateLayeredWindow();
      }
      break;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      BeginPaint(window, &paint);
      EndPaint(window, &paint);
      RenderCandidateLayeredWindow();
      return 0;
    }
    case WM_TIMER:
      if (wparam == kCandidateToolTooltipTimer) {
        KillTimer(window, kCandidateToolTooltipTimer);
        if (hovered_candidate_tool_ != kCandidateToolNone &&
            IsCandidateToolEnabled(hovered_candidate_tool_,
                                   has_previous_candidate_page_,
                                   has_next_candidate_page_)) {
          ShowCandidateTooltip(hovered_candidate_tool_);
        }
        return 0;
      }
      if (wparam == kCandidateWindowWatchTimer) {
        if (!ShouldKeepCandidateWindow()) {
          HideCandidateWindow();
        }
        return 0;
      }
      break;
    case WM_ERASEBKGND:
      return 1;
    case WM_NCDESTROY:
      if (candidate_window_ == window) {
        candidate_window_ = nullptr;
      }
      break;
    default:
      break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK TsfTextService::StaticCandidateWindowProc(HWND window,
                                                           UINT message,
                                                           WPARAM wparam,
                                                           LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window,
                      GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  auto* service =
      reinterpret_cast<TsfTextService*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (service != nullptr) {
    return service->CandidateWindowProc(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace fp::tsf
