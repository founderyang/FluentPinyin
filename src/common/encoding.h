#pragma once

#include <string>
#include <string_view>

namespace fp {

std::string WideToUtf8(std::wstring_view value);
std::wstring Utf8ToWide(std::string_view value);
std::wstring Utf8ToWideStrict(std::string_view value);
std::wstring ToLowerInvariant(std::wstring value);
std::wstring TrimWhitespace(std::wstring_view value);
bool EqualsInsensitive(std::wstring_view left, std::wstring_view right);
bool ContainsInsensitive(std::wstring_view value, std::wstring_view needle);

}  // namespace fp
