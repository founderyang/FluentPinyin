#include "tsf/context_menu_model.h"

#include <iostream>

namespace {

int g_failures = 0;

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

}  // namespace

int main() {
  Expect(fp::tsf::ContextMenuRowCount(false) == fp::tsf::kContextMenuRowCount,
         "context menu row count is stable");
  Expect(fp::tsf::ContextMenuRowCount(true) == fp::tsf::kToolbarGearMenuRowCount,
         "toolbar gear menu row count is stable");
  Expect(fp::tsf::ContextMenuHasSeparatorAfter(fp::tsf::kContextMenuRowEmoji),
         "emoji row has separator in context menu");
  Expect(!fp::tsf::ContextMenuHasSeparatorAfter(fp::tsf::kContextMenuRowEmoji, true),
         "toolbar gear menu has no separators");
  Expect(fp::tsf::ContextMenuRowHasSubmenu(fp::tsf::kContextMenuRowShape),
         "shape row opens submenu");
  Expect(fp::tsf::ContextMenuRowHasSubmenu(fp::tsf::kToolbarGearMenuRowCustom, true),
         "custom toolbar row opens submenu");
  Expect(fp::tsf::ContextMenuCommandForRow(fp::tsf::kContextMenuRowEmoji) ==
             fp::tsf::kMenuEmoji,
         "emoji row command is stable");
  Expect(fp::tsf::ContextMenuCommandForRow(fp::tsf::kContextMenuRowShape) == 0,
         "shape row command is handled by submenu");
  Expect(fp::tsf::ContextSubmenuCommandForRow(fp::tsf::kContextMenuRowShape, 0) ==
             fp::tsf::kMenuFullShapeHalf,
         "shape first submenu command is half shape");
  Expect(fp::tsf::ContextSubmenuCommandForRow(fp::tsf::kContextMenuRowCharset, 1) ==
             fp::tsf::kMenuCharsetTraditional,
         "charset second submenu command is traditional");
  Expect(fp::tsf::ContextSubmenuWidthDips(fp::tsf::kContextMenuRowCharset) ==
             fp::tsf::kContextSubmenuCharsetWidthDips,
         "charset submenu width is stable");

  return g_failures == 0 ? 0 : 1;
}
