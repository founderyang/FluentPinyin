#pragma once

namespace fp::devtools {

struct FontCleanupSummary {
  int resource_remove_attempts = 0;
  int registry_values_deleted = 0;
  int file_delete_requests = 0;
};

FontCleanupSummary RemoveFontFilesAndRegistry();

}  // namespace fp::devtools
