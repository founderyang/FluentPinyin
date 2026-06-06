#include "updater/release_json.h"

#include <cctype>
#include <cstdint>

namespace fp::updater {
namespace {

struct JsonValueSpan {
  size_t start = 0;
  size_t end = 0;
};

bool IsJsonWhitespace(char value) {
  return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

size_t SkipJsonWhitespace(std::string_view json, size_t position) {
  while (position < json.size() && IsJsonWhitespace(json[position])) {
    ++position;
  }
  return position;
}

size_t RootObjectStart(std::string_view json) {
  size_t root = 0;
  if (json.size() >= 3 && static_cast<unsigned char>(json[0]) == 0xEF &&
      static_cast<unsigned char>(json[1]) == 0xBB &&
      static_cast<unsigned char>(json[2]) == 0xBF) {
    root = 3;
  }
  return SkipJsonWhitespace(json, root);
}

int JsonHexDigit(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return 10 + value - 'a';
  }
  if (value >= 'A' && value <= 'F') {
    return 10 + value - 'A';
  }
  return -1;
}

bool ReadJsonHexQuad(std::string_view json, size_t* position, std::uint32_t* value) {
  if (*position + 4 > json.size()) {
    return false;
  }
  std::uint32_t result = 0;
  for (int count = 0; count < 4; ++count) {
    const int digit = JsonHexDigit(json[*position + count]);
    if (digit < 0) {
      return false;
    }
    result = (result << 4) | static_cast<std::uint32_t>(digit);
  }
  *position += 4;
  *value = result;
  return true;
}

bool AppendUtf8CodePoint(std::uint32_t code_point, std::string* output) {
  if (code_point <= 0x7F) {
    output->push_back(static_cast<char>(code_point));
    return true;
  }
  if (code_point <= 0x7FF) {
    output->push_back(static_cast<char>(0xC0 | (code_point >> 6)));
    output->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    return true;
  }
  if (code_point >= 0xD800 && code_point <= 0xDFFF) {
    return false;
  }
  if (code_point <= 0xFFFF) {
    output->push_back(static_cast<char>(0xE0 | (code_point >> 12)));
    output->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    output->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    return true;
  }
  if (code_point <= 0x10FFFF) {
    output->push_back(static_cast<char>(0xF0 | (code_point >> 18)));
    output->push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
    output->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
    output->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    return true;
  }
  return false;
}

bool ParseJsonString(std::string_view json, size_t* position, std::string* value) {
  if (*position >= json.size() || json[*position] != '"') {
    return false;
  }
  value->clear();
  ++*position;

  while (*position < json.size()) {
    const unsigned char ch = static_cast<unsigned char>(json[(*position)++]);
    if (ch == '"') {
      return true;
    }
    if (ch < 0x20) {
      return false;
    }
    if (ch != '\\') {
      value->push_back(static_cast<char>(ch));
      continue;
    }
    if (*position >= json.size()) {
      return false;
    }

    const char escape = json[(*position)++];
    switch (escape) {
      case '"':
      case '\\':
      case '/':
        value->push_back(escape);
        break;
      case 'b':
        value->push_back('\b');
        break;
      case 'f':
        value->push_back('\f');
        break;
      case 'n':
        value->push_back('\n');
        break;
      case 'r':
        value->push_back('\r');
        break;
      case 't':
        value->push_back('\t');
        break;
      case 'u': {
        std::uint32_t code_unit = 0;
        if (!ReadJsonHexQuad(json, position, &code_unit)) {
          return false;
        }
        if (code_unit >= 0xD800 && code_unit <= 0xDBFF) {
          if (*position + 2 > json.size() || json[*position] != '\\' ||
              json[*position + 1] != 'u') {
            return false;
          }
          *position += 2;
          std::uint32_t low = 0;
          if (!ReadJsonHexQuad(json, position, &low) || low < 0xDC00 || low > 0xDFFF) {
            return false;
          }
          const std::uint32_t high_offset = code_unit - 0xD800;
          const std::uint32_t low_offset = low - 0xDC00;
          code_unit = 0x10000 + ((high_offset << 10) | low_offset);
        }
        if (!AppendUtf8CodePoint(code_unit, value)) {
          return false;
        }
        break;
      }
      default:
        return false;
    }
  }
  return false;
}

bool SkipJsonValue(std::string_view json, size_t* position);

bool SkipJsonLiteral(std::string_view json, size_t* position, std::string_view literal) {
  if (json.substr(*position, literal.size()) != literal) {
    return false;
  }
  *position += literal.size();
  return true;
}

bool SkipJsonNumber(std::string_view json, size_t* position) {
  size_t cursor = *position;
  if (cursor < json.size() && json[cursor] == '-') {
    ++cursor;
  }
  if (cursor >= json.size()) {
    return false;
  }
  if (json[cursor] == '0') {
    ++cursor;
  } else if (std::isdigit(static_cast<unsigned char>(json[cursor])) != 0) {
    while (cursor < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[cursor])) != 0) {
      ++cursor;
    }
  } else {
    return false;
  }
  if (cursor < json.size() && json[cursor] == '.') {
    ++cursor;
    const size_t fraction_start = cursor;
    while (cursor < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[cursor])) != 0) {
      ++cursor;
    }
    if (cursor == fraction_start) {
      return false;
    }
  }
  if (cursor < json.size() && (json[cursor] == 'e' || json[cursor] == 'E')) {
    ++cursor;
    if (cursor < json.size() && (json[cursor] == '+' || json[cursor] == '-')) {
      ++cursor;
    }
    const size_t exponent_start = cursor;
    while (cursor < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[cursor])) != 0) {
      ++cursor;
    }
    if (cursor == exponent_start) {
      return false;
    }
  }
  *position = cursor;
  return true;
}

bool SkipJsonArray(std::string_view json, size_t* position) {
  if (*position >= json.size() || json[*position] != '[') {
    return false;
  }
  ++*position;
  *position = SkipJsonWhitespace(json, *position);
  if (*position < json.size() && json[*position] == ']') {
    ++*position;
    return true;
  }
  while (*position < json.size()) {
    if (!SkipJsonValue(json, position)) {
      return false;
    }
    *position = SkipJsonWhitespace(json, *position);
    if (*position < json.size() && json[*position] == ',') {
      ++*position;
      *position = SkipJsonWhitespace(json, *position);
      continue;
    }
    if (*position < json.size() && json[*position] == ']') {
      ++*position;
      return true;
    }
    return false;
  }
  return false;
}

bool SkipJsonObject(std::string_view json, size_t* position) {
  if (*position >= json.size() || json[*position] != '{') {
    return false;
  }
  ++*position;
  *position = SkipJsonWhitespace(json, *position);
  if (*position < json.size() && json[*position] == '}') {
    ++*position;
    return true;
  }
  while (*position < json.size()) {
    std::string key;
    if (!ParseJsonString(json, position, &key)) {
      return false;
    }
    *position = SkipJsonWhitespace(json, *position);
    if (*position >= json.size() || json[*position] != ':') {
      return false;
    }
    ++*position;
    if (!SkipJsonValue(json, position)) {
      return false;
    }
    *position = SkipJsonWhitespace(json, *position);
    if (*position < json.size() && json[*position] == ',') {
      ++*position;
      *position = SkipJsonWhitespace(json, *position);
      continue;
    }
    if (*position < json.size() && json[*position] == '}') {
      ++*position;
      return true;
    }
    return false;
  }
  return false;
}

bool SkipJsonValue(std::string_view json, size_t* position) {
  *position = SkipJsonWhitespace(json, *position);
  if (*position >= json.size()) {
    return false;
  }
  switch (json[*position]) {
    case '"': {
      std::string ignored;
      return ParseJsonString(json, position, &ignored);
    }
    case '{':
      return SkipJsonObject(json, position);
    case '[':
      return SkipJsonArray(json, position);
    case 't':
      return SkipJsonLiteral(json, position, "true");
    case 'f':
      return SkipJsonLiteral(json, position, "false");
    case 'n':
      return SkipJsonLiteral(json, position, "null");
    default:
      return json[*position] == '-' ||
                     std::isdigit(static_cast<unsigned char>(json[*position])) != 0
                 ? SkipJsonNumber(json, position)
                 : false;
  }
}

std::optional<JsonValueSpan> FindJsonObjectMember(std::string_view json,
                                                  size_t object_start,
                                                  std::string_view key) {
  size_t position = SkipJsonWhitespace(json, object_start);
  if (position >= json.size() || json[position] != '{') {
    return std::nullopt;
  }
  ++position;
  position = SkipJsonWhitespace(json, position);
  while (position < json.size() && json[position] != '}') {
    std::string member_key;
    if (!ParseJsonString(json, &position, &member_key)) {
      return std::nullopt;
    }
    position = SkipJsonWhitespace(json, position);
    if (position >= json.size() || json[position] != ':') {
      return std::nullopt;
    }
    ++position;
    position = SkipJsonWhitespace(json, position);
    const size_t value_start = position;
    size_t value_end = position;
    if (!SkipJsonValue(json, &value_end)) {
      return std::nullopt;
    }
    if (member_key == key) {
      return JsonValueSpan{.start = value_start, .end = value_end};
    }
    position = SkipJsonWhitespace(json, value_end);
    if (position < json.size() && json[position] == ',') {
      ++position;
      position = SkipJsonWhitespace(json, position);
      continue;
    }
    if (position < json.size() && json[position] == '}') {
      break;
    }
    return std::nullopt;
  }
  return std::nullopt;
}

std::optional<std::string> ExtractJsonString(std::string_view json,
                                             size_t object_start,
                                             std::string_view key) {
  const auto span = FindJsonObjectMember(json, object_start, key);
  if (!span) {
    return std::nullopt;
  }
  size_t position = SkipJsonWhitespace(json, span->start);
  std::string value;
  if (!ParseJsonString(json, &position, &value)) {
    return std::nullopt;
  }
  position = SkipJsonWhitespace(json, position);
  return position == span->end ? std::optional<std::string>(std::move(value)) : std::nullopt;
}

std::optional<std::string> ExtractAssetDownloadUrl(std::string_view json,
                                                   size_t object_start,
                                                   std::string_view asset_name) {
  size_t position = SkipJsonWhitespace(json, object_start);
  if (position >= json.size() || json[position] != '{') {
    return std::nullopt;
  }
  ++position;
  position = SkipJsonWhitespace(json, position);

  std::optional<std::string> name;
  std::optional<std::string> download_url;
  while (position < json.size() && json[position] != '}') {
    std::string member_key;
    if (!ParseJsonString(json, &position, &member_key)) {
      return std::nullopt;
    }
    position = SkipJsonWhitespace(json, position);
    if (position >= json.size() || json[position] != ':') {
      return std::nullopt;
    }
    ++position;
    position = SkipJsonWhitespace(json, position);

    if (member_key == "name" || member_key == "browser_download_url") {
      std::string value;
      if (!ParseJsonString(json, &position, &value)) {
        return std::nullopt;
      }
      if (member_key == "name") {
        name = std::move(value);
      } else {
        download_url = std::move(value);
      }
    } else if (!SkipJsonValue(json, &position)) {
      return std::nullopt;
    }

    position = SkipJsonWhitespace(json, position);
    if (position < json.size() && json[position] == ',') {
      ++position;
      position = SkipJsonWhitespace(json, position);
      continue;
    }
    if (position < json.size() && json[position] == '}') {
      break;
    }
    return std::nullopt;
  }

  if (name && *name == asset_name && download_url) {
    return download_url;
  }
  return std::nullopt;
}

std::optional<std::string> FindAssetUrl(std::string_view json,
                                        size_t root_object_start,
                                        std::string_view asset_name) {
  const auto assets = FindJsonObjectMember(json, root_object_start, "assets");
  if (!assets) {
    return std::nullopt;
  }
  size_t position = SkipJsonWhitespace(json, assets->start);
  if (position >= json.size() || json[position] != '[') {
    return std::nullopt;
  }
  ++position;
  position = SkipJsonWhitespace(json, position);
  while (position < json.size() && json[position] != ']') {
    const size_t item_start = position;
    size_t item_end = position;
    if (!SkipJsonValue(json, &item_end)) {
      return std::nullopt;
    }
    if (item_start < json.size() && json[item_start] == '{') {
      if (const auto url = ExtractAssetDownloadUrl(json, item_start, asset_name)) {
        return url;
      }
    }
    position = SkipJsonWhitespace(json, item_end);
    if (position < json.size() && json[position] == ',') {
      ++position;
      position = SkipJsonWhitespace(json, position);
      continue;
    }
    if (position < json.size() && json[position] == ']') {
      break;
    }
    return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace

std::optional<ReleaseInfo> ParseReleaseInfo(std::string_view json,
                                            std::string_view asset_name) {
  const size_t root = RootObjectStart(json);
  if (root >= json.size() || json[root] != '{') {
    return std::nullopt;
  }
  const auto tag = ExtractJsonString(json, root, "tag_name");
  const auto asset_url = FindAssetUrl(json, root, asset_name);
  if (!tag || !asset_url) {
    return std::nullopt;
  }
  return ReleaseInfo{.tag = *tag, .asset_name = std::string(asset_name), .asset_url = *asset_url};
}

ReleaseJsonDiagnostics InspectReleaseJson(std::string_view json,
                                          std::string_view asset_name) {
  ReleaseJsonDiagnostics diagnostics;
  const size_t root = RootObjectStart(json);
  diagnostics.root_object = root < json.size() && json[root] == '{';
  if (!diagnostics.root_object) {
    return diagnostics;
  }
  diagnostics.tag = ExtractJsonString(json, root, "tag_name").has_value();
  diagnostics.asset = FindAssetUrl(json, root, asset_name).has_value();
  return diagnostics;
}

}  // namespace fp::updater
