#pragma once

#include <string>
#include <string_view>

namespace fp {

std::string WideToUtf8(std::wstring_view value);
std::wstring Utf8ToWide(std::string_view value);
std::wstring Utf8ToWideStrict(std::string_view value);

}  // namespace fp
