#include "core/rime_engine.h"

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
}

}  // namespace

int main() {
  TestRimeDeployDecision();
  TestWanxiangTranslatorTuningPatch();
  if (g_failures != 0) {
    std::cerr << g_failures << " core unit test failure(s)\n";
    return 1;
  }
  std::cout << "Core unit tests passed\n";
  return 0;
}
