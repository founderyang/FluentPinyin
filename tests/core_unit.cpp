#include "core/rime_engine.h"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

int g_failures = 0;

void Expect(bool condition, std::string_view message) {
  if (condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAIL: " << message << "\n";
}

void TestRimeDeployDecision() {
  using fp::core::detail::ShouldDeployRimeWorkspace;

  Expect(!ShouldDeployRimeWorkspace(true, false, false, true),
         "fresh cache skips deploy even when deploy is allowed");
  Expect(ShouldDeployRimeWorkspace(true, false, false, false),
         "missing cache deploys when deploy is allowed");
  Expect(ShouldDeployRimeWorkspace(true, false, true, true),
         "changed user config deploys when deploy is allowed");
  Expect(!ShouldDeployRimeWorkspace(false, false, true, false),
         "disabled deploy does not deploy for missing cache");
  Expect(ShouldDeployRimeWorkspace(false, true, false, true),
         "forced rebuild deploys even when regular deploy is disabled");
}

void TestWanxiangTranslatorTuningPatch() {
  const std::string base_patch =
      fp::core::detail::BuildWanxiangCustomPatchForTesting(false, false);
  const std::string pro_patch =
      fp::core::detail::BuildWanxiangCustomPatchForTesting(true, false);

  Expect(base_patch.find("translator/enable_user_dict: true") != std::string::npos,
         "Wanxiang base keeps main translator user dictionary tuning enabled");
  Expect(pro_patch.find("translator/enable_user_dict: false") != std::string::npos,
         "Wanxiang pro keeps main translator user dictionary tuning disabled");
  Expect(base_patch.find("grammar/language: wanxiang-lts-zh-hans") != std::string::npos,
         "Wanxiang base enables the LTS grammar model by default");
  Expect(base_patch.find("user_predict/enable_post_predict: true") != std::string::npos,
         "Wanxiang base enables post-commit prediction by default");
  Expect(base_patch.find("user_predict/enable_context_reorder: true") != std::string::npos,
         "Wanxiang base enables input-time word order learning by default");
  Expect(base_patch.find("user_predict/enable_fallback_reorder: true") != std::string::npos,
         "Wanxiang base enables fallback word order learning by default");
  Expect(base_patch.find("add_user_dict/enable_auto_phrase: true") != std::string::npos,
         "Wanxiang base enables automatic phrase learning by default");

  const std::string base_fingerprint =
      fp::core::detail::BuildWanxiangSettingFingerprintForTesting(false);
  const std::string pro_fingerprint =
      fp::core::detail::BuildWanxiangSettingFingerprintForTesting(true);
  Expect(base_fingerprint.find("main_translator_user_dict_enabled=1") !=
             std::string::npos,
         "Wanxiang base fingerprint includes tuning enabled");
  Expect(pro_fingerprint.find("main_translator_user_dict_enabled=0") !=
             std::string::npos,
         "Wanxiang pro fingerprint includes tuning disabled");
  Expect(base_fingerprint.find("wanxiang_large_model_enabled=1") !=
             std::string::npos,
         "Wanxiang fingerprint includes large model enabled");
  Expect(base_fingerprint.find("wanxiang_auto_word_order_enabled=1") !=
             std::string::npos,
         "Wanxiang fingerprint includes automatic word order learning enabled");
}

void TestRimeCandidateSmoke() {
  fp::core::RimeEngine engine;
  fp::core::RimeEngineOptions options;
  const auto temp_root =
      std::filesystem::temp_directory_path() / L"FluentPinyin-core-unit-rime";
  std::error_code error;
  std::filesystem::remove_all(temp_root, error);
  std::filesystem::create_directories(temp_root, error);
  options.user_data_dir = temp_root / L"user";
  options.log_dir = temp_root / L"log";
  options.staging_dir = temp_root / L"build";
  options.deploy = true;
  const auto status = engine.Initialize(options);
  Expect(status.initialized, "RimeEngine initializes for candidate smoke");
  if (!status.initialized) {
    std::wcerr << L"Rime status: " << status.message << L"\n";
    return;
  }

  const auto page = engine.GetCandidatePageForInput("nihao", 0, 5);
  Expect(!page.composition.empty(), "RimeEngine returns composition for pinyin input");
  Expect(!page.candidates.empty(), "RimeEngine returns candidates for pinyin input");
  engine.Shutdown();
  std::filesystem::remove_all(temp_root, error);
}

}  // namespace

int main() {
  TestRimeDeployDecision();
  TestWanxiangTranslatorTuningPatch();
  TestRimeCandidateSmoke();
  if (g_failures != 0) {
    std::cerr << g_failures << " core unit test failure(s)\n";
    return 1;
  }
  std::cout << "Core unit tests passed\n";
  return 0;
}
