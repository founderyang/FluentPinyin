#pragma once

#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32) && !defined(FP_CORE_STATIC)
#if defined(FP_CORE_EXPORTS)
#define FP_CORE_API __declspec(dllexport)
#else
#define FP_CORE_API __declspec(dllimport)
#endif
#else
#define FP_CORE_API
#endif

namespace fp::core {

struct RimeEngineOptions {
  std::filesystem::path shared_data_dir;
  std::filesystem::path user_data_dir;
  std::filesystem::path log_dir;
  std::filesystem::path staging_dir;
  std::filesystem::path prebuilt_data_dir;
  std::string schema_id = "rime_frost";
  bool deploy = true;
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

class FP_CORE_API RimeEngine {
 public:
  RimeEngine() = default;
  RimeEngine(const RimeEngine&) = delete;
  RimeEngine& operator=(const RimeEngine&) = delete;
  ~RimeEngine();

  RimeEngineStatus Initialize(const RimeEngineOptions& options = {});
  void Shutdown();

  [[nodiscard]] std::vector<RimeCandidateView> GetCandidatesForInput(
      const std::string& input,
      int max_candidates = 8);
  [[nodiscard]] RimeCandidatePage GetCandidatePageForInput(const std::string& input,
                                                           int page_index,
                                                           int page_size = 8);
  [[nodiscard]] const std::filesystem::path& shared_data_dir() const noexcept {
    return shared_data_dir_;
  }
  [[nodiscard]] const std::filesystem::path& user_data_dir() const noexcept {
    return user_data_dir_;
  }
  [[nodiscard]] const std::string& rime_version() const noexcept { return rime_version_; }
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }

 private:
  bool EnsureUserConfig();

  bool initialized_ = false;
  std::filesystem::path shared_data_dir_;
  std::filesystem::path user_data_dir_;
  std::filesystem::path log_dir_;
  std::filesystem::path staging_dir_;
  std::filesystem::path prebuilt_data_dir_;
  std::string schema_id_ = "rime_frost";
  std::string rime_version_;
};

}  // namespace fp::core
