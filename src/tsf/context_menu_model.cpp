#include "tsf/context_menu_model.h"

namespace fp::tsf {

bool ContextMenuHasSeparatorAfter(int row, bool toolbar_mode) {
  if (toolbar_mode) {
    return false;
  }
  return row == kContextMenuRowCharset || row == kContextMenuRowEmoji ||
         row == kContextMenuRowDictionaries || row == kContextMenuRowKeyConfig ||
         row == kContextMenuRowToolbar;
}

bool ContextMenuRowHasSubmenu(int row, bool toolbar_mode) {
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

int ContextSubmenuWidthDips(int parent_row, bool toolbar_mode) {
  if (toolbar_mode) {
    return kToolbarCustomSubmenuWidthDips;
  }
  return parent_row == kContextMenuRowCharset ? kContextSubmenuCharsetWidthDips
                                              : kContextSubmenuShapeWidthDips;
}

UINT ContextMenuCommandForRow(int row, bool toolbar_mode) {
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

UINT ContextSubmenuCommandForRow(int parent_row, int row, bool toolbar_mode) {
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

}  // namespace fp::tsf
