#pragma once

#include <windows.h>

namespace fp::tsf {

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

inline constexpr int kContextMenuWidthDips = 202;
inline constexpr int kContextMenuRowHeightHalfDips = 67;
inline constexpr int kContextMenuCornerRadiusDips = 7;
inline constexpr int kContextMenuTextPointSize = 11;
inline constexpr int kContextMenuTextLeftDips = 72;
inline constexpr int kContextMenuIconLeftDips = 42;
inline constexpr int kContextMenuIconSizeDips = 18;
inline constexpr int kContextMenuChevronRightDips = 17;
inline constexpr int kContextMenuChevronSizeDips = 16;
inline constexpr int kContextMenuTaskbarGapDips = 8;
inline constexpr int kContextMenuScreenMarginDips = 6;
inline constexpr int kContextMenuHoverInsetXDips = 3;
inline constexpr int kContextMenuHoverInsetYHalfDips = 3;
inline constexpr int kContextMenuHoverRadiusDips = 4;
inline constexpr int kContextSubmenuShapeWidthDips = 102;
inline constexpr int kContextSubmenuCharsetWidthDips = 118;
inline constexpr int kToolbarCustomSubmenuWidthDips = 222;
inline constexpr int kToolbarGearMenuWidthDips = 128;
inline constexpr int kToolbarGearMenuTextLeftDips = 15;
inline constexpr int kToolbarGearMenuTextRightDips = 12;
inline constexpr int kToolbarGearMenuChevronRightDips = 10;
inline constexpr int kToolbarCustomSubmenuIconLeftDips = 42;
inline constexpr int kToolbarCustomSubmenuIconSizeDips = 18;
inline constexpr int kToolbarCustomSubmenuTextLeftDips = 72;
inline constexpr int kToolbarCustomSubmenuTextRightDips = 12;
inline constexpr int kToolbarMenuTextIconPointSize = 11;
inline constexpr int kContextSubmenuTextLeftDips = 46;
inline constexpr int kContextSubmenuCheckLeftDips = 18;
inline constexpr int kContextSubmenuCheckSizeDips = 16;
inline constexpr int kContextSubmenuRadioDotSizeDips = 6;
inline constexpr int kContextSubmenuOverlapDips = 2;

bool ContextMenuHasSeparatorAfter(int row, bool toolbar_mode = false);
bool ContextMenuRowHasSubmenu(int row, bool toolbar_mode = false);
int ContextMenuRowCount(bool toolbar_mode);
int ContextMenuWidthDips(bool toolbar_mode);
int ContextMenuTextLeftDips(bool toolbar_mode);
int ContextMenuTextRightDips(bool toolbar_mode);
int ContextMenuChevronRightDips(bool toolbar_mode);
int ContextSubmenuWidthDips(int parent_row, bool toolbar_mode = false);
UINT ContextMenuCommandForRow(int row, bool toolbar_mode = false);
UINT ContextSubmenuCommandForRow(int parent_row, int row, bool toolbar_mode = false);

}  // namespace fp::tsf
