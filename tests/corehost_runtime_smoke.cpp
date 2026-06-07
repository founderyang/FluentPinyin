#include "common/core_ipc_protocol.h"
#include "tsf/rime_core_client.h"

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <thread>

namespace {

bool WaitForPipeGone(std::chrono::milliseconds timeout) {
  const std::wstring pipe_name = fp::coreipc::PipeName();
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (!WaitNamedPipeW(pipe_name.c_str(), 50)) {
      const DWORD error = GetLastError();
      if (error == ERROR_FILE_NOT_FOUND) {
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return !WaitNamedPipeW(pipe_name.c_str(), 50);
}

bool Expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

std::wstring RuntimeSmokePipeSuffix() {
  return L"runtime-smoke-" + std::to_wstring(GetCurrentProcessId());
}

void UseIsolatedAppData(const std::filesystem::path& temp_root) {
  const auto local = temp_root / L"appdata" / L"Local";
  const auto roaming = temp_root / L"appdata" / L"Roaming";
  const auto program_data = temp_root / L"appdata" / L"ProgramData";
  std::error_code error;
  std::filesystem::create_directories(local, error);
  std::filesystem::create_directories(roaming, error);
  std::filesystem::create_directories(program_data, error);
  SetEnvironmentVariableW(L"LOCALAPPDATA", local.c_str());
  SetEnvironmentVariableW(L"APPDATA", roaming.c_str());
  SetEnvironmentVariableW(L"PROGRAMDATA", program_data.c_str());
}

void PrintDirectoryTree(const std::filesystem::path& root) {
  std::error_code error;
  if (!std::filesystem::exists(root, error)) {
    std::wcerr << L"Missing diagnostic directory: " << root.wstring() << L"\n";
    return;
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error)) {
    if (error) {
      break;
    }
    std::wcerr << L"  " << entry.path().wstring();
    if (entry.is_regular_file(error)) {
      std::wcerr << L" (" << entry.file_size(error) << L" bytes)";
    }
    std::wcerr << L"\n";
  }
}

std::wstring Quote(std::wstring_view value) {
  std::wstring result = L"\"";
  for (wchar_t ch : value) {
    if (ch == L'"') {
      result += L"\\\"";
    } else {
      result.push_back(ch);
    }
  }
  result += L"\"";
  return result;
}

struct CoreHostProcess {
  HANDLE process = nullptr;
  DWORD process_id = 0;

  CoreHostProcess() = default;
  CoreHostProcess(const CoreHostProcess&) = delete;
  CoreHostProcess& operator=(const CoreHostProcess&) = delete;

  CoreHostProcess(CoreHostProcess&& other) noexcept
      : process(other.process), process_id(other.process_id) {
    other.process = nullptr;
    other.process_id = 0;
  }

  CoreHostProcess& operator=(CoreHostProcess&& other) noexcept {
    if (this != &other) {
      Close();
      process = other.process;
      process_id = other.process_id;
      other.process = nullptr;
      other.process_id = 0;
    }
    return *this;
  }

  ~CoreHostProcess() {
    Close();
  }

  void Close() {
    if (process != nullptr) {
      CloseHandle(process);
      process = nullptr;
    }
    process_id = 0;
  }

  bool IsRunning() const {
    if (process == nullptr) {
      return false;
    }
    return WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
  }

  bool WaitForExit(std::chrono::milliseconds timeout) const {
    if (process == nullptr) {
      return true;
    }
    const DWORD wait = WaitForSingleObject(process, static_cast<DWORD>(timeout.count()));
    return wait == WAIT_OBJECT_0;
  }

  void TerminateIfRunning() {
    if (IsRunning()) {
      TerminateProcess(process, 1);
      WaitForSingleObject(process, 3000);
    }
  }
};

bool WaitForPipeReady(std::chrono::milliseconds timeout) {
  const std::wstring pipe_name = fp::coreipc::PipeName();
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (WaitNamedPipeW(pipe_name.c_str(), 50)) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return WaitNamedPipeW(pipe_name.c_str(), 50) != FALSE;
}

std::optional<CoreHostProcess> StartIsolatedCoreHost(
    const std::filesystem::path& corehost_path,
    const std::filesystem::path& temp_root) {
  const auto user_dir = temp_root / L"user";
  const auto staging_dir = temp_root / L"build";
  const auto log_dir = temp_root / L"log";
  std::wstring command_line =
      Quote(corehost_path.wstring()) + L" --user-data-dir " + Quote(user_dir.wstring()) +
      L" --staging-dir " + Quote(staging_dir.wstring()) +
      L" --log-dir " + Quote(log_dir.wstring());
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESHOWWINDOW;
  startup.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessW(corehost_path.c_str(),
                                      command_line.data(),
                                      nullptr,
                                      nullptr,
                                      FALSE,
                                      CREATE_NO_WINDOW,
                                      nullptr,
                                      corehost_path.parent_path().c_str(),
                                      &startup,
                                      &process);
  if (!created) {
    std::wcerr << L"Failed to start isolated corehost: " << GetLastError() << L"\n";
    return std::nullopt;
  }
  CloseHandle(process.hThread);
  CoreHostProcess corehost;
  corehost.process = process.hProcess;
  corehost.process_id = process.dwProcessId;
  if (!WaitForPipeReady(std::chrono::seconds(5))) {
    DWORD exit_code = STILL_ACTIVE;
    GetExitCodeProcess(corehost.process, &exit_code);
    std::wcerr << L"Isolated corehost pipe was not ready. pid=" << corehost.process_id
               << L" exit_code=" << exit_code << L"\n";
    corehost.TerminateIfRunning();
    return std::nullopt;
  }
  return std::move(corehost);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 2) {
    std::cerr << "Usage: corehost_runtime_smoke <fluent-pinyin-corehost.exe>\n";
    return 2;
  }

  const std::filesystem::path corehost_path = std::filesystem::absolute(argv[1]);
  if (!std::filesystem::exists(corehost_path)) {
    std::wcerr << L"Missing corehost executable: " << corehost_path.wstring() << L"\n";
    return 2;
  }

  const std::wstring pipe_suffix = RuntimeSmokePipeSuffix();
  SetEnvironmentVariableW(L"FLUENT_PINYIN_COREHOST_PIPE_SUFFIX", pipe_suffix.c_str());
  fp::tsf::RimeCoreClient cleanup_client(corehost_path);
  cleanup_client.ShutdownSharedEngine();
  WaitForPipeGone(std::chrono::seconds(5));

  const auto temp_root =
      std::filesystem::temp_directory_path() / L"FluentPinyin-corehost-runtime-smoke";
  std::error_code error;
  std::filesystem::remove_all(temp_root, error);
  std::filesystem::create_directories(temp_root, error);
  UseIsolatedAppData(temp_root);
  auto corehost = StartIsolatedCoreHost(corehost_path, temp_root);
  if (!Expect(corehost.has_value(), "isolated corehost starts")) {
    return 1;
  }

  fp::tsf::RimeCoreClient client(corehost_path);
  const auto status = client.Initialize();
  if (!Expect(status.initialized, "corehost initializes the shared Rime engine")) {
    std::wcerr << L"Rime status: " << status.message << L"\n";
    DWORD exit_code = STILL_ACTIVE;
    if (corehost->process != nullptr) {
      GetExitCodeProcess(corehost->process, &exit_code);
    }
    std::wcerr << L"Corehost pid=" << corehost->process_id
               << L" exit_code=" << exit_code << L"\n";
    PrintDirectoryTree(temp_root);
    const bool shutdown_sent = client.ShutdownSharedEngine();
    std::cerr << "Shutdown after init failure sent=" << (shutdown_sent ? "true" : "false")
              << "\n";
    corehost->TerminateIfRunning();
    return 1;
  }
  client.SetOption("ascii_punct", false);
  client.SetOption("full_shape", false);
  client.SetOption("s2s", false);
  client.SetOption("s2t", false);
  client.SetOption("s2hk", false);
  client.SetOption("s2tw", false);
  client.SetOption("s2s", true);
  client.SetOption("abbrev", true);
  client.ResetComposition();

  const auto page = client.GetCandidatePageForInput("nihao", 0, 5);
  bool ok = true;
  ok &= Expect(!page.composition.empty(), "corehost returns a composition for pinyin input");
  ok &= Expect(!page.candidates.empty(), "corehost returns candidates for pinyin input");

  const bool shutdown_sent = client.ShutdownSharedEngine();
  ok &= Expect(shutdown_sent, "corehost accepts shutdown request");
  ok &= Expect(WaitForPipeGone(std::chrono::seconds(8)), "corehost exits after shutdown request");
  if (!corehost->WaitForExit(std::chrono::seconds(8))) {
    std::wcerr << L"Corehost still running after shutdown request. pid="
               << corehost->process_id << L"\n";
    corehost->TerminateIfRunning();
    ok = false;
  }
  std::filesystem::remove_all(temp_root, error);

  if (!ok) {
    return 1;
  }
  std::cout << "Corehost runtime smoke passed\n";
  return 0;
}
