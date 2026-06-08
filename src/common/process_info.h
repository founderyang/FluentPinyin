#pragma once

#include <string>
#include <string_view>

namespace fp {

std::wstring GetCurrentProcessCommandLine();
std::wstring GetCurrentProcessImageName();
bool CurrentProcessCommandLineContains(std::wstring_view needle);

}  // namespace fp
