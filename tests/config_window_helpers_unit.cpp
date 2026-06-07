#include "config_winui/window_helpers.h"

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

void TestScaleSizeForDpi() {
  namespace window = fp::config_winui;

  const winrt::Windows::Graphics::SizeInt32 base{860, 480};
  const auto normal = window::ScaleSizeForDpi(base, 96);
  Expect(normal.Width == 860 && normal.Height == 480, "96 DPI keeps logical size");

  const auto scaled = window::ScaleSizeForDpi(base, 144);
  Expect(scaled.Width == 1290 && scaled.Height == 720, "144 DPI scales by 150 percent");

  const auto tiny = window::ScaleSizeForDpi({1, 1}, 120);
  Expect(tiny.Width == 1 && tiny.Height == 1, "MulDiv preserves small positive sizes");
}

void TestActivateMissingWindow() {
  namespace window = fp::config_winui;
  Expect(!window::ActivateWindowByTitle(L"FluentPinyinMissingWindowForUnitTest", 1),
         "missing windows are not activated");
}

}  // namespace

int main() {
  TestScaleSizeForDpi();
  TestActivateMissingWindow();
  if (g_failures != 0) {
    std::cerr << g_failures << " config window helper failure(s)\n";
    return 1;
  }
  std::cout << "Config window helper tests passed\n";
  return 0;
}
