#include "devtools/rime_warmup.h"

#include "common/logging.h"
#include "common/path_utils.h"
#include "core/rime_engine.h"

#include <windows.h>

#include <iostream>
#include <string>
#include <vector>

namespace fp::devtools {

int StartRimeWarmupProcess() {
  const ULONGLONG start_tick = GetTickCount64();
  const auto executable = fp::GetModuleExecutablePath();
  if (executable.empty()) {
    fp::LogWarning(L"installer", L"Failed to resolve devtools path for Rime warmup.");
    return 1;
  }

  const auto working_dir = executable.parent_path();
  std::wstring command_line = L"\"" + executable.wstring() + L"\" warmup-rime";
  std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
  mutable_command.push_back(L'\0');

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESHOWWINDOW;
  startup.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessW(executable.c_str(),
                                      mutable_command.data(),
                                      nullptr,
                                      nullptr,
                                      FALSE,
                                      CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS,
                                      nullptr,
                                      working_dir.c_str(),
                                      &startup,
                                      &process);
  if (!created) {
    const DWORD error = GetLastError();
    fp::LogWarning(L"installer",
                   L"Failed to start Rime warmup process: " + std::to_wstring(error));
    std::wcerr << L"Failed to start Rime warmup process: " << error << L"\n";
    return 1;
  }

  const DWORD process_id = process.dwProcessId;
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  const ULONGLONG elapsed_ms = GetTickCount64() - start_tick;
  fp::LogInfo(L"installer",
              L"Started Rime warmup process " + std::to_wstring(process_id) + L" in " +
                  std::to_wstring(elapsed_ms) + L" ms.");
  std::cout << "Rime warmup started in " << elapsed_ms << " ms.\n";
  return 0;
}

int WarmupRime() {
  const ULONGLONG start_tick = GetTickCount64();
  SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
  const BOOL background_mode = SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);

  fp::core::RimeEngine engine;
  fp::core::RimeEngineOptions options;
  auto status = engine.Initialize(options);

  const bool built_schema = status.initialized && engine.HasBuiltSchema();
  if (background_mode) {
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
  }

  const ULONGLONG elapsed_ms = GetTickCount64() - start_tick;
  if (!status.initialized) {
    fp::LogError(L"installer",
                 L"Rime warmup failed in " + std::to_wstring(elapsed_ms) + L" ms: " +
                     status.message);
    std::wcerr << L"Rime warmup failed: " << status.message << L"\n";
    return 1;
  }

  fp::LogInfo(L"installer",
              L"Rime warmup completed in " + std::to_wstring(elapsed_ms) +
                  L" ms; built_schema=" +
                  (built_schema ? std::wstring(L"yes") : std::wstring(L"no")) + L".");
  std::cout << "Rime warmup completed in " << elapsed_ms << " ms.\n";
  return 0;
}

}  // namespace fp::devtools
