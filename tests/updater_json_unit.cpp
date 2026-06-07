#include "updater/release_json.h"

#include <iostream>
#include <string>
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

void TestReleaseJsonSuccess() {
  const std::string json = R"({
    "assets": [
      {"name": "symbols.zip", "browser_download_url": "https://example.invalid/wrong.msi"},
      {"browser_download_url": "https://example.invalid/FluentPinyin.msi?x=quote%5C%22",
       "name": "FluentPinyin.msi",
       "extra": [true, false, null, {"escaped": "\u6d41\u7545"}]}
    ],
    "tag_name": "v00.00.06",
    "nested": {"tag_name": "v99.99.99"}
  })";
  const auto release = fp::updater::ParseReleaseInfo(json, "FluentPinyin.msi");
  Expect(release.has_value(), "ParseReleaseInfo accepts GitHub release JSON");
  if (!release) {
    return;
  }
  Expect(release->tag == "v00.00.06", "ParseReleaseInfo reads root tag_name");
  Expect(release->asset_name == "FluentPinyin.msi", "ParseReleaseInfo preserves requested asset");
  Expect(release->asset_url == "https://example.invalid/FluentPinyin.msi?x=quote%5C%22",
         "ParseReleaseInfo reads matching asset download URL");
}

void TestUtf8BomAndEscapes() {
  const std::string json =
      std::string("\xEF\xBB\xBF") +
      R"({"tag_name":"v1","assets":[{"name":"FluentPinyin.msi","browser_download_url":"https://example.invalid/\u0046luentPinyin.msi"}]})";
  const auto release = fp::updater::ParseReleaseInfo(json, "FluentPinyin.msi");
  Expect(release.has_value(), "ParseReleaseInfo accepts UTF-8 BOM and unicode escapes");
  if (release) {
    Expect(release->asset_url == "https://example.invalid/FluentPinyin.msi",
           "ParseReleaseInfo decodes unicode escapes in strings");
  }
}

void TestMalformedJson() {
  Expect(!fp::updater::ParseReleaseInfo(R"({"tag_name":"v1","assets":[)",
                                        "FluentPinyin.msi"),
         "ParseReleaseInfo rejects malformed JSON without crashing");
  const auto diagnostics =
      fp::updater::InspectReleaseJson(R"({"tag_name":"v1","assets":[]})", "FluentPinyin.msi");
  Expect(diagnostics.root_object, "InspectReleaseJson reports a root object");
  Expect(diagnostics.tag, "InspectReleaseJson reports tag presence");
  Expect(!diagnostics.asset, "InspectReleaseJson reports missing asset");
}

}  // namespace

int main() {
  TestReleaseJsonSuccess();
  TestUtf8BomAndEscapes();
  TestMalformedJson();
  if (g_failures != 0) {
    std::cerr << g_failures << " updater JSON unit test failure(s)\n";
    return 1;
  }
  std::cout << "Updater JSON unit tests passed\n";
  return 0;
}
