#include "common/core_ipc_protocol.h"
#include "tsf/rime_core_client.h"

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

bool WaitForPipeGone(const std::wstring& pipe_name, std::chrono::milliseconds timeout) {
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

class ScopedHandle {
 public:
  ScopedHandle() = default;
  explicit ScopedHandle(HANDLE handle) : handle_(handle) {}
  ScopedHandle(const ScopedHandle&) = delete;
  ScopedHandle& operator=(const ScopedHandle&) = delete;
  ~ScopedHandle() {
    reset();
  }

  [[nodiscard]] HANDLE get() const noexcept { return handle_; }
  [[nodiscard]] bool valid() const noexcept {
    return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
  }
  void reset(HANDLE handle = nullptr) noexcept {
    if (valid()) {
      CloseHandle(handle_);
    }
    handle_ = handle;
  }

 private:
  HANDLE handle_ = nullptr;
};

class ScopedAttributeList {
 public:
  explicit ScopedAttributeList(DWORD attribute_count) {
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, attribute_count, 0, &bytes);
    if (bytes == 0) {
      return;
    }
    storage_.resize(bytes);
    list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
    if (!InitializeProcThreadAttributeList(list_, attribute_count, 0, &bytes)) {
      list_ = nullptr;
      storage_.clear();
    }
  }
  ScopedAttributeList(const ScopedAttributeList&) = delete;
  ScopedAttributeList& operator=(const ScopedAttributeList&) = delete;
  ~ScopedAttributeList() {
    if (list_ != nullptr) {
      DeleteProcThreadAttributeList(list_);
    }
  }

  [[nodiscard]] LPPROC_THREAD_ATTRIBUTE_LIST get() const noexcept { return list_; }
  [[nodiscard]] bool valid() const noexcept { return list_ != nullptr; }

 private:
  std::vector<unsigned char> storage_;
  LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
};

bool CreateChildSecretPipe(ScopedHandle* read_handle, ScopedHandle* write_handle) {
  SECURITY_ATTRIBUTES attributes{};
  attributes.nLength = sizeof(attributes);
  attributes.bInheritHandle = TRUE;
  HANDLE read = nullptr;
  HANDLE write = nullptr;
  if (!CreatePipe(&read, &write, &attributes, 0)) {
    return false;
  }
  read_handle->reset(read);
  write_handle->reset(write);
  return true;
}

bool WriteChildSecret(HANDLE pipe, const std::string& secret) {
  if (pipe == nullptr || pipe == INVALID_HANDLE_VALUE) {
    return false;
  }
  const auto size = static_cast<std::uint32_t>(secret.size());
  DWORD written = 0;
  if (!WriteFile(pipe, &size, sizeof(size), &written, nullptr) ||
      written != sizeof(size)) {
    return false;
  }
  if (size == 0) {
    return true;
  }
  return WriteFile(pipe, secret.data(), size, &written, nullptr) &&
         written == size;
}

bool WaitForPipeReady(const std::wstring& pipe_name, std::chrono::milliseconds timeout) {
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
    const std::filesystem::path& temp_root,
    const std::wstring& pipe_suffix,
    const std::string& ipc_secret) {
  const auto user_dir = temp_root / L"user";
  const auto staging_dir = temp_root / L"build";
  const auto log_dir = temp_root / L"log";
  ScopedHandle secret_read;
  ScopedHandle secret_write;
  if (!CreateChildSecretPipe(&secret_read, &secret_write)) {
    std::wcerr << L"Failed to create isolated corehost secret pipe: "
               << GetLastError() << L"\n";
    return std::nullopt;
  }
  std::wstring command_line =
      Quote(corehost_path.wstring()) + L" --user-data-dir " + Quote(user_dir.wstring()) +
      L" --staging-dir " + Quote(staging_dir.wstring()) +
      L" --log-dir " + Quote(log_dir.wstring()) +
      L" --pipe-suffix " + Quote(pipe_suffix) +
      L" --ipc-secret-handle " +
      std::to_wstring(reinterpret_cast<std::uintptr_t>(secret_read.get())) +
      L" --parent-pid " + std::to_wstring(GetCurrentProcessId());
  ScopedAttributeList attributes(1);
  if (!attributes.valid()) {
    std::wcerr << L"Failed to allocate isolated corehost startup attributes.\n";
    return std::nullopt;
  }
  HANDLE inherited_handles[] = {secret_read.get()};
  if (!UpdateProcThreadAttribute(attributes.get(),
                                 0,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                 inherited_handles,
                                 sizeof(inherited_handles),
                                 nullptr,
                                 nullptr)) {
    std::wcerr << L"Failed to configure isolated corehost inherited handle list: "
               << GetLastError() << L"\n";
    return std::nullopt;
  }
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
  startup.StartupInfo.wShowWindow = SW_HIDE;
  startup.lpAttributeList = attributes.get();
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessW(corehost_path.c_str(),
                                      command_line.data(),
                                      nullptr,
                                      nullptr,
                                      TRUE,
                                      CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
                                      nullptr,
                                      corehost_path.parent_path().c_str(),
                                      &startup.StartupInfo,
                                      &process);
  if (!created) {
    std::wcerr << L"Failed to start isolated corehost: " << GetLastError() << L"\n";
    return std::nullopt;
  }
  secret_read.reset();
  const bool secret_written = WriteChildSecret(secret_write.get(), ipc_secret);
  const DWORD secret_write_error = secret_written ? ERROR_SUCCESS : GetLastError();
  secret_write.reset();
  CloseHandle(process.hThread);
  CoreHostProcess corehost;
  corehost.process = process.hProcess;
  corehost.process_id = process.dwProcessId;
  if (!secret_written) {
    std::wcerr << L"Failed to send isolated corehost IPC secret: "
               << secret_write_error << L"\n";
    corehost.TerminateIfRunning();
    return std::nullopt;
  }
  if (!WaitForPipeReady(fp::coreipc::PipeNameForSuffix(pipe_suffix),
                        std::chrono::seconds(5))) {
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
  const std::string ipc_secret = "runtime-smoke-secret-" +
                                 std::to_string(GetCurrentProcessId());
  fp::tsf::RimeCoreClient cleanup_client(corehost_path, pipe_suffix, ipc_secret);
  cleanup_client.ShutdownSharedEngine();
  WaitForPipeGone(fp::coreipc::PipeNameForSuffix(pipe_suffix), std::chrono::seconds(5));

  const auto temp_root =
      std::filesystem::temp_directory_path() / L"FluentPinyin-corehost-runtime-smoke";
  std::error_code error;
  std::filesystem::remove_all(temp_root, error);
  std::filesystem::create_directories(temp_root, error);
  UseIsolatedAppData(temp_root);
  auto corehost = StartIsolatedCoreHost(corehost_path, temp_root, pipe_suffix, ipc_secret);
  if (!Expect(corehost.has_value(), "isolated corehost starts")) {
    return 1;
  }

  fp::tsf::RimeCoreClient client(corehost_path, pipe_suffix, ipc_secret);
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
  ok &= Expect(WaitForPipeGone(fp::coreipc::PipeNameForSuffix(pipe_suffix),
                               std::chrono::seconds(8)),
               "corehost exits after shutdown request");
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
