#include "common/constants.h"
#include "config_winui/wanxiang_modes.h"

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

}  // namespace

int main() {
  const auto& modes = fp::config_winui::WanxiangModeDefinitions();
  Expect(modes.size() == 14, "exposes all Wanxiang mode toggles");
  Expect(std::wstring_view(modes.front().setting_key) ==
             fp::kWanxiangReverseLookupEnabledSetting,
         "keeps reverse lookup first");
  Expect(std::wstring_view(modes.front().legacy_key) == fp::kLegacyUModeSetting,
         "keeps reverse lookup migrated from legacy u mode");
  Expect(std::wstring_view(modes[4].setting_key) ==
             fp::kWanxiangCalculatorEnabledSetting,
         "keeps calculator mode in the V-mode migration group");
  Expect(std::wstring_view(modes[4].legacy_key) == fp::kLegacyVModeSetting,
         "keeps calculator migrated from legacy v mode");
  Expect(!modes[6].default_value, "keeps quick symbols disabled by default");
  Expect(!modes.back().default_value, "keeps schema shortcuts disabled by default");

  if (g_failures != 0) {
    std::cerr << g_failures << " Wanxiang mode failure(s)\n";
    return 1;
  }
  std::cout << "Wanxiang mode tests passed\n";
  return 0;
}
