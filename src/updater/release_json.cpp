#include "updater/release_json.h"

#include "common/encoding.h"

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/base.h>

namespace fp::updater {
namespace {

std::wstring JsonTextToWide(std::string_view json) {
  if (json.size() >= 3 && static_cast<unsigned char>(json[0]) == 0xEF &&
      static_cast<unsigned char>(json[1]) == 0xBB &&
      static_cast<unsigned char>(json[2]) == 0xBF) {
    json.remove_prefix(3);
  }
  return fp::Utf8ToWideStrict(json);
}

std::optional<winrt::Windows::Data::Json::JsonObject> TryParseRoot(std::string_view json) {
  const std::wstring wide = JsonTextToWide(json);
  if (wide.empty() && !json.empty()) {
    return std::nullopt;
  }
  try {
    return winrt::Windows::Data::Json::JsonObject::Parse(winrt::hstring(wide));
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::string> JsonStringMember(
    const winrt::Windows::Data::Json::JsonObject& object,
    winrt::hstring const& key) {
  try {
    const auto value = object.GetNamedValue(key);
    if (value.ValueType() != winrt::Windows::Data::Json::JsonValueType::String) {
      return std::nullopt;
    }
    return fp::WideToUtf8(std::wstring_view(value.GetString()));
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::string> FindAssetUrl(
    const winrt::Windows::Data::Json::JsonObject& root,
    std::string_view expected_asset_name) {
  try {
    const auto assets = root.GetNamedArray(L"assets");
    for (uint32_t index = 0; index < assets.Size(); ++index) {
      const auto value = assets.GetAt(index);
      if (value.ValueType() != winrt::Windows::Data::Json::JsonValueType::Object) {
        continue;
      }
      const auto object = value.GetObject();
      const auto name = JsonStringMember(object, L"name");
      if (!name || *name != expected_asset_name) {
        continue;
      }
      return JsonStringMember(object, L"browser_download_url");
    }
  } catch (...) {
    return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace

std::optional<ReleaseInfo> ParseReleaseInfo(std::string_view json,
                                            std::string_view asset_name) {
  const auto root = TryParseRoot(json);
  if (!root) {
    return std::nullopt;
  }
  const auto tag = JsonStringMember(*root, L"tag_name");
  const auto asset_url = FindAssetUrl(*root, asset_name);
  if (!tag || !asset_url) {
    return std::nullopt;
  }
  return ReleaseInfo{.tag = *tag, .asset_name = std::string(asset_name), .asset_url = *asset_url};
}

ReleaseJsonDiagnostics InspectReleaseJson(std::string_view json,
                                          std::string_view asset_name) {
  ReleaseJsonDiagnostics diagnostics;
  const auto root = TryParseRoot(json);
  diagnostics.root_object = root.has_value();
  if (!root) {
    return diagnostics;
  }
  diagnostics.tag = JsonStringMember(*root, L"tag_name").has_value();
  diagnostics.asset = FindAssetUrl(*root, asset_name).has_value();
  return diagnostics;
}

}  // namespace fp::updater
