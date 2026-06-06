#pragma once

#include <cstdint>
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

namespace detail {

[[nodiscard]] constexpr bool ShouldDeployRimeWorkspace(bool allow_deploy,
                                                       bool force_rebuild_cache,
                                                       bool user_config_changed,
                                                       bool has_fresh_build_cache) noexcept {
  return force_rebuild_cache ||
         (allow_deploy && (!has_fresh_build_cache || user_config_changed));
}

[[nodiscard]] FP_CORE_API std::string BuildWanxiangCustomPatchForTesting(
    bool pro,
    bool use_imported_dictionary);
[[nodiscard]] FP_CORE_API std::string BuildWanxiangSettingFingerprintForTesting(bool pro);

}  // namespace detail

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

class FP_CORE_API RimeEngine {
 public:
  RimeEngine() = default;
  RimeEngine(const RimeEngine&) = delete;
  RimeEngine& operator=(const RimeEngine&) = delete;
  ~RimeEngine();

  RimeEngineStatus Initialize(const RimeEngineOptions& options = {});
  void Shutdown();
  RimeEngineStatus Redeploy();
  static RimeEngineStatus RedeployDefaultUserData();
  void ResetComposition();
  bool SetOption(const std::string& option_name, bool enabled);
  [[nodiscard]] bool GetOption(const std::string& option_name);
  [[nodiscard]] bool HasBuiltSchema() const;

  [[nodiscard]] std::vector<RimeCandidateView> GetCandidatesForInput(
      const std::string& input,
      int max_candidates = 8);
  [[nodiscard]] RimeCandidatePage GetCandidatePageForInput(const std::string& input,
                                                           int page_index,
                                                           int page_size = 8);
  [[nodiscard]] RimeCandidateCommit SelectCandidateForInput(const std::string& input,
                                                            int page_index,
                                                            int page_size,
                                                            size_t candidate_index);
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
  bool EnsureSession();
  bool SyncSessionInput(const std::string& input, bool reset_page = false);
  void DestroySession();
  [[nodiscard]] std::wstring ConvertOutputText(const char* utf8_text);
  bool EnsureOpenCcS2TConverter();
  void CloseOpenCcConverters();

  bool initialized_ = false;
  std::filesystem::path shared_data_dir_;
  std::filesystem::path user_data_dir_;
  std::filesystem::path log_dir_;
  std::filesystem::path staging_dir_;
  std::filesystem::path prebuilt_data_dir_;
  std::string schema_id_ = "wanxiang";
  std::string rime_version_;
  std::uintptr_t session_id_ = 0;
  std::string session_input_;
  int session_page_index_ = 0;
  void* opencc_s2t_ = nullptr;
  bool opencc_s2t_failed_ = false;
  bool output_traditional_ = false;
  bool user_config_changed_ = false;
  bool schema_id_overridden_ = false;
};

}  // namespace fp::core
