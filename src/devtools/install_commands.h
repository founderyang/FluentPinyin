#pragma once

#include <filesystem>

namespace fp::devtools {

std::filesystem::path DefaultInstallDir();
int PrepareInstall();
int FinalizeInstall();
int CleanupInstall(const std::filesystem::path& install_dir,
                   bool skip_install_dir,
                   bool keep_user_data,
                   bool restart_text_services);

}  // namespace fp::devtools
