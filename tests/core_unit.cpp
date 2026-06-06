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

}  // namespace

int main() {
  TestRimeDeployDecision();
  if (g_failures != 0) {
    std::cerr << g_failures << " core unit test failure(s)\n";
    return 1;
  }
  std::cout << "Core unit tests passed\n";
  return 0;
}
