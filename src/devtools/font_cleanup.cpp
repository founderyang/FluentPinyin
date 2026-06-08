#include "devtools/font_cleanup.h"

#include "common/bundled_fonts.h"
#include "common/logging.h"
#include "common/path_utils.h"
#include "devtools/cleanup_utils.h"
#include "devtools/registry_utils.h"

#include <windows.h>

#include <string>

namespace fp::devtools {
namespace {

void BroadcastFontChange() {
  SendMessageTimeoutW(HWND_BROADCAST,
                      WM_FONTCHANGE,
                      0,
                      0,
                      SMTO_ABORTIFHUNG | SMTO_NORMAL,
                      3000,
                      nullptr);
}

}  // namespace

FontCleanupSummary RemoveFontFilesAndRegistry() {
  constexpr wchar_t kFontsRegistry[] =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
  const auto target_dir = fp::GetLocalAppDataPath() / L"Microsoft" / L"Windows" / L"Fonts";
  FontCleanupSummary summary;

  for (const auto& font : fp::kBundledFontEntries) {
    const auto target = target_dir / std::wstring(font.file);
    for (int attempt = 0; attempt < 4; ++attempt) {
      RemoveFontResourceExW(target.c_str(), 0, nullptr);
      ++summary.resource_remove_attempts;
    }
    if (fp::devtools::DeleteRegistryValue(
            HKEY_CURRENT_USER,
            kFontsRegistry,
            fp::BundledFontRegistryValueName(font, fp::kTrueTypeFontRegistryKind))) {
      ++summary.registry_values_deleted;
    }
    if (fp::devtools::DeleteRegistryValue(
            HKEY_CURRENT_USER,
            kFontsRegistry,
            fp::BundledFontRegistryValueName(font, fp::kOpenTypeFontRegistryKind))) {
      ++summary.registry_values_deleted;
    }
    fp::devtools::RemoveFileNow(target);
    ++summary.file_delete_requests;
  }
  BroadcastFontChange();
  fp::LogInfo(L"installer",
              L"font cleanup: resource_remove_attempts=" +
                  std::to_wstring(summary.resource_remove_attempts) +
                  L", registry_values_deleted=" +
                  std::to_wstring(summary.registry_values_deleted) +
                  L", file_delete_requests=" +
                  std::to_wstring(summary.file_delete_requests) + L".");
  return summary;
}

}  // namespace fp::devtools
