#include "common/process_info.h"

#include "common/path_utils.h"

#include <windows.h>

namespace fp {

std::wstring GetCurrentProcessCommandLine() {
  const wchar_t* command_line = GetCommandLineW();
  return command_line != nullptr ? std::wstring(command_line) : std::wstring();
}

std::wstring GetCurrentProcessImageName() {
  const auto executable = GetModuleExecutablePath();
  return executable.empty() ? std::wstring() : executable.filename().wstring();
}

bool CurrentProcessCommandLineContains(std::wstring_view needle) {
  return GetCurrentProcessCommandLine().find(std::wstring(needle)) != std::wstring::npos;
}

}  // namespace fp
