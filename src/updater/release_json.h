#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace fp::updater {

struct ReleaseInfo {
  std::string tag;
  std::string asset_name;
  std::string asset_url;
};

struct ReleaseJsonDiagnostics {
  bool root_object = false;
  bool tag = false;
  bool asset = false;
};

std::optional<ReleaseInfo> ParseReleaseInfo(std::string_view json,
                                            std::string_view asset_name);
ReleaseJsonDiagnostics InspectReleaseJson(std::string_view json,
                                          std::string_view asset_name);

}  // namespace fp::updater
