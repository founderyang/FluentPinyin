#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fp::core {

struct RimeEngineOptions {
  std::filesystem::path shared_data_dir;
  std::filesystem::path user_data_dir;
  std::filesystem::path log_dir;
  std::filesystem::path staging_dir;
  std::filesystem::path prebuilt_data_dir;
  std::string schema_id;
  // Allows Rime deploy only when cache is missing, stale, or changed.
  bool deploy = true;
  bool force_rebuild_cache = false;
};

struct RimeEngineStatus {
  bool initialized = false;
  std::wstring message;
};

struct RimeCandidateView {
  std::wstring text;
  std::wstring comment;
};

struct RimeCandidatePage {
  std::wstring composition;
  std::vector<RimeCandidateView> candidates;
  bool has_previous_page = false;
  bool has_next_page = false;
};

struct RimeCandidateCommit {
  bool handled = false;
  std::wstring text;
  std::string remaining_input;
  std::wstring remaining_composition;
};

}  // namespace fp::core
