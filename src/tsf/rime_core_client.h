#pragma once

#include "common/rime_types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace fp::tsf {

class RimeCoreClient {
 public:
  explicit RimeCoreClient(std::filesystem::path corehost_path);
  RimeCoreClient(std::filesystem::path corehost_path,
                 std::wstring pipe_suffix,
                 std::string ipc_secret);

  fp::core::RimeEngineStatus Initialize();
  void Shutdown();
  bool ShutdownSharedEngine();
  void ResetComposition();
  bool SetOption(const std::string& option_name, bool enabled);
  fp::core::RimeCandidatePage GetCandidatePageForInput(const std::string& input,
                                                       int page_index,
                                                       int page_size);
  fp::core::RimeCandidateCommit SelectCandidateForInput(const std::string& input,
                                                        int page_index,
                                                        int page_size,
                                                        size_t candidate_index);
  fp::core::RimeEngineStatus Redeploy();

 private:
  bool EnsureHostRunning();
  std::optional<std::string> SendRequest(std::string_view request,
                                         bool allow_start = true,
                                         unsigned long wait_timeout_ms = 2500);

  std::filesystem::path corehost_path_;
  std::wstring pipe_suffix_;
  std::string ipc_secret_;
  bool initialized_ = false;
};

}  // namespace fp::tsf
