#pragma once

#include <string_view>

namespace fp {

void LogInfo(std::wstring_view component, std::wstring_view message);
void LogWarning(std::wstring_view component, std::wstring_view message);
void LogError(std::wstring_view component, std::wstring_view message);

}  // namespace fp
