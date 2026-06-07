#include "common/core_ipc_protocol.h"
#include "common/encoding.h"
#include "common/logging.h"
#include "core/rime_engine.h"

#include <windows.h>
#include <shellapi.h>
#include <sddl.h>

#include <array>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr DWORD kPipeBufferSize = 1024 * 1024;
constexpr DWORD kPipeDefaultTimeoutMs = 120000;

bool ReadExact(HANDLE pipe, void* buffer, DWORD bytes) {
  auto* cursor = static_cast<unsigned char*>(buffer);
  DWORD remaining = bytes;
  while (remaining > 0) {
    DWORD read = 0;
    if (!ReadFile(pipe, cursor, remaining, &read, nullptr) || read == 0) {
      return false;
    }
    cursor += read;
    remaining -= read;
  }
  return true;
}

bool WriteExact(HANDLE pipe, const void* buffer, DWORD bytes) {
  const auto* cursor = static_cast<const unsigned char*>(buffer);
  DWORD remaining = bytes;
  while (remaining > 0) {
    DWORD written = 0;
    if (!WriteFile(pipe, cursor, remaining, &written, nullptr) || written == 0) {
      return false;
    }
    cursor += written;
    remaining -= written;
  }
  return true;
}

bool ReadMessage(HANDLE pipe, std::string* payload) {
  std::uint32_t size = 0;
  if (payload == nullptr || !ReadExact(pipe, &size, sizeof(size)) || size > kPipeBufferSize) {
    return false;
  }
  payload->assign(size, '\0');
  return size == 0 || ReadExact(pipe, payload->data(), size);
}

bool WriteMessage(HANDLE pipe, std::string_view payload) {
  if (payload.size() > kPipeBufferSize) {
    return false;
  }
  const auto size = static_cast<std::uint32_t>(payload.size());
  return WriteExact(pipe, &size, sizeof(size)) &&
         (size == 0 || WriteExact(pipe, payload.data(), size));
}

class CoreHostState {
 public:
  explicit CoreHostState(fp::core::RimeEngineOptions options) : options_(std::move(options)) {}

  std::string Handle(std::string_view request) {
    fp::coreipc::Command command{};
    std::vector<std::string> fields;
    if (!fp::coreipc::DecodeCommand(request, &command, &fields)) {
      return fp::coreipc::EncodeErrorResponse(L"Invalid core host request.");
    }

    std::lock_guard lock(mutex_);
    switch (command) {
      case fp::coreipc::Command::kInitialize:
        return fp::coreipc::EncodeStatusResponse(EnsureInitialized());
      case fp::coreipc::Command::kShutdown:
        shutdown_requested_ = true;
        // The core host is the process boundary for librime/OpenCC.  On a
        // shutdown request, return the IPC response and let process exit reclaim
        // the engine instead of running teardown code in a host application.
        engine_.release();
        return fp::coreipc::EncodeStatusResponse({true, L"Core host shut down engine."});
      case fp::coreipc::Command::kResetComposition:
        if (const auto status = EnsureInitialized(); !status.initialized) {
          return fp::coreipc::EncodeStatusResponse(status);
        }
        engine_->ResetComposition();
        return fp::coreipc::EncodeStatusResponse({true, L"Composition reset."});
      case fp::coreipc::Command::kSetOption:
        return HandleSetOption(fields);
      case fp::coreipc::Command::kGetCandidatePage:
        return HandleGetCandidatePage(fields);
      case fp::coreipc::Command::kSelectCandidate:
        return HandleSelectCandidate(fields);
      case fp::coreipc::Command::kRedeploy:
        return HandleRedeploy();
      case fp::coreipc::Command::kHandshake:
        if (fields.size() != 1) {
          return fp::coreipc::EncodeErrorResponse(L"Invalid handshake request.");
        }
        return fp::coreipc::EncodeHandshakeResponse(fields[0]);
      default:
        return fp::coreipc::EncodeErrorResponse(L"Unknown core host command.");
    }
  }

  [[nodiscard]] bool shutdown_requested() const noexcept { return shutdown_requested_; }

 private:
  fp::core::RimeEngineStatus EnsureInitialized() {
    if (engine_ != nullptr && engine_->initialized()) {
      return {true, L"Core host engine already initialized."};
    }
    try {
      auto engine = std::make_unique<fp::core::RimeEngine>();
      auto status = engine->Initialize(options_);
      if (status.initialized) {
        engine_ = std::move(engine);
      }
      return status;
    } catch (const std::exception& error) {
      std::wstring message = L"Core host initialization exception: ";
      message += fp::Utf8ToWide(error.what());
      fp::LogError(L"corehost", message);
      return {false, message};
    } catch (...) {
      const std::wstring message = L"Core host initialization failed unexpectedly.";
      fp::LogError(L"corehost", message);
      return {false, message};
    }
  }

  std::string HandleSetOption(const std::vector<std::string>& fields) {
    if (fields.size() != 2) {
      return fp::coreipc::EncodeErrorResponse(L"Invalid set-option request.");
    }
    if (const auto status = EnsureInitialized(); !status.initialized) {
      return fp::coreipc::EncodeStatusResponse(status);
    }
    const bool enabled = fields[1] == "1" || fields[1] == "true";
    return fp::coreipc::EncodeStatusResponse(
        {engine_->SetOption(fields[0], enabled), L"Set option."});
  }

  std::string HandleGetCandidatePage(const std::vector<std::string>& fields) {
    if (fields.size() != 3) {
      return fp::coreipc::EncodeErrorResponse(L"Invalid candidate-page request.");
    }
    int page_index = 0;
    int page_size = 0;
    if (!ParseInt(fields[1], &page_index) || !ParseInt(fields[2], &page_size)) {
      return fp::coreipc::EncodeErrorResponse(L"Invalid candidate-page numeric field.");
    }
    if (const auto status = EnsureInitialized(); !status.initialized) {
      return fp::coreipc::EncodeStatusResponse(status);
    }
    const auto page = engine_->GetCandidatePageForInput(fields[0], page_index, page_size);
    return fp::coreipc::EncodeCandidatePageResponse(page);
  }

  std::string HandleSelectCandidate(const std::vector<std::string>& fields) {
    if (fields.size() != 4) {
      return fp::coreipc::EncodeErrorResponse(L"Invalid select-candidate request.");
    }
    int page_index = 0;
    int page_size = 0;
    size_t candidate_index = 0;
    if (!ParseInt(fields[1], &page_index) || !ParseInt(fields[2], &page_size) ||
        !ParseSize(fields[3], &candidate_index)) {
      return fp::coreipc::EncodeErrorResponse(L"Invalid select-candidate numeric field.");
    }
    if (const auto status = EnsureInitialized(); !status.initialized) {
      return fp::coreipc::EncodeStatusResponse(status);
    }
    return fp::coreipc::EncodeCandidateCommitResponse(
        engine_->SelectCandidateForInput(fields[0], page_index, page_size, candidate_index));
  }

  std::string HandleRedeploy() {
    if (engine_ == nullptr) {
      engine_ = std::make_unique<fp::core::RimeEngine>();
    }
    return fp::coreipc::EncodeStatusResponse(engine_->Redeploy());
  }

  static bool ParseInt(std::string_view text, int* value) {
    try {
      *value = std::stoi(std::string(text));
      return true;
    } catch (...) {
      return false;
    }
  }

  static bool ParseSize(std::string_view text, size_t* value) {
    try {
      *value = static_cast<size_t>(std::stoull(std::string(text)));
      return true;
    } catch (...) {
      return false;
    }
  }

  std::mutex mutex_;
  fp::core::RimeEngineOptions options_;
  std::unique_ptr<fp::core::RimeEngine> engine_;
  bool shutdown_requested_ = false;
};

void ServePipe(CoreHostState* state, HANDLE pipe) {
  std::string request;
  std::string response = fp::coreipc::EncodeErrorResponse(L"Core host internal error.");
  if (ReadMessage(pipe, &request)) {
    try {
      response = state->Handle(request);
    } catch (...) {
      response = fp::coreipc::EncodeErrorResponse(L"Core host command failed unexpectedly.");
    }
  }
  WriteMessage(pipe, response);
  FlushFileBuffers(pipe);
  DisconnectNamedPipe(pipe);
  CloseHandle(pipe);
  if (state->shutdown_requested()) {
    fp::FlushLogs();
    TerminateProcess(GetCurrentProcess(), 0);
  }
}

std::optional<std::wstring> CurrentUserSidString() {
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    fp::LogError(L"corehost", L"OpenProcessToken failed while building pipe ACL.");
    return std::nullopt;
  }

  DWORD required = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &required);
  if (required == 0) {
    CloseHandle(token);
    fp::LogError(L"corehost", L"GetTokenInformation failed while sizing pipe ACL.");
    return std::nullopt;
  }

  std::vector<std::byte> buffer(required);
  if (!GetTokenInformation(token, TokenUser, buffer.data(), required, &required)) {
    const DWORD error = GetLastError();
    CloseHandle(token);
    fp::LogError(L"corehost",
                 L"GetTokenInformation failed while building pipe ACL: " +
                     std::to_wstring(error));
    return std::nullopt;
  }
  CloseHandle(token);

  const auto* user = reinterpret_cast<const TOKEN_USER*>(buffer.data());
  LPWSTR sid_text = nullptr;
  if (!ConvertSidToStringSidW(user->User.Sid, &sid_text) || sid_text == nullptr) {
    fp::LogError(L"corehost", L"ConvertSidToStringSid failed while building pipe ACL.");
    return std::nullopt;
  }
  std::wstring result(sid_text);
  LocalFree(sid_text);
  return result;
}

class LocalSecurityDescriptor {
 public:
  LocalSecurityDescriptor() {
    const auto user_sid = CurrentUserSidString();
    if (!user_sid) {
      return;
    }
    const std::wstring sddl =
        L"D:P"
        L"(A;;GA;;;SY)"
        L"(A;;GA;;;BA)"
        L"(A;;GA;;;" +
        *user_sid + L")";
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),
                                                              SDDL_REVISION_1,
                                                              &descriptor_,
                                                              nullptr)) {
      const DWORD error = GetLastError();
      fp::LogError(L"corehost",
                   L"ConvertStringSecurityDescriptorToSecurityDescriptor failed: " +
                       std::to_wstring(error));
    }
  }
  LocalSecurityDescriptor(const LocalSecurityDescriptor&) = delete;
  LocalSecurityDescriptor& operator=(const LocalSecurityDescriptor&) = delete;
  ~LocalSecurityDescriptor() {
    if (descriptor_ != nullptr) {
      LocalFree(descriptor_);
    }
  }

  SECURITY_ATTRIBUTES* attributes() {
    if (descriptor_ == nullptr) {
      return nullptr;
    }
    attributes_.nLength = sizeof(attributes_);
    attributes_.lpSecurityDescriptor = descriptor_;
    attributes_.bInheritHandle = FALSE;
    return &attributes_;
  }

  [[nodiscard]] bool valid() const noexcept {
    return descriptor_ != nullptr;
  }

 private:
  PSECURITY_DESCRIPTOR descriptor_ = nullptr;
  SECURITY_ATTRIBUTES attributes_{};
};

fp::core::RimeEngineOptions ParseOptions() {
  fp::core::RimeEngineOptions options;
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr) {
    return options;
  }
  for (int index = 1; index < argc; ++index) {
    const std::wstring_view arg = argv[index];
    if (arg == L"--user-data-dir" && index + 1 < argc) {
      options.user_data_dir = argv[++index];
    } else if (arg == L"--staging-dir" && index + 1 < argc) {
      options.staging_dir = argv[++index];
    } else if (arg == L"--log-dir" && index + 1 < argc) {
      options.log_dir = argv[++index];
    } else if (arg == L"--no-deploy") {
      options.deploy = false;
    }
  }
  LocalFree(argv);
  return options;
}

int RunCoreHost() {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

  const std::wstring mutex_name = fp::coreipc::CoreHostMutexName();
  HANDLE mutex = CreateMutexW(nullptr, TRUE, mutex_name.c_str());
  if (mutex == nullptr) {
    fp::LogError(L"corehost", L"Failed to create core host mutex.");
    return 1;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    fp::LogInfo(L"corehost", L"Another FluentPinyin core host is already running.");
    CloseHandle(mutex);
    return 0;
  }

  fp::LogInfo(L"corehost", L"FluentPinyin core host started.");
  CoreHostState state(ParseOptions());
  LocalSecurityDescriptor pipe_security;
  if (!pipe_security.valid()) {
    fp::LogError(L"corehost", L"Core host pipe security descriptor is invalid.");
    CloseHandle(mutex);
    return 1;
  }
  for (;;) {
    const std::wstring pipe_name = fp::coreipc::PipeName();
    HANDLE pipe = CreateNamedPipeW(pipe_name.c_str(),
                                   PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                   PIPE_UNLIMITED_INSTANCES,
                                   kPipeBufferSize,
                                   kPipeBufferSize,
                                   kPipeDefaultTimeoutMs,
                                   pipe_security.attributes());
    if (pipe == INVALID_HANDLE_VALUE) {
      const DWORD error = GetLastError();
      fp::LogError(L"corehost", L"CreateNamedPipe failed: " + std::to_wstring(error));
      return 1;
    }

    const BOOL connected = ConnectNamedPipe(pipe, nullptr);
    const DWORD connect_error = connected ? ERROR_SUCCESS : GetLastError();
    const BOOL usable = connected ? TRUE : (connect_error == ERROR_PIPE_CONNECTED);
    if (!usable) {
      fp::LogWarning(L"corehost",
                     L"ConnectNamedPipe failed: " + std::to_wstring(connect_error));
      CloseHandle(pipe);
      continue;
    }
    ServePipe(&state, pipe);
    if (state.shutdown_requested()) {
      fp::LogInfo(L"corehost", L"FluentPinyin core host shutdown requested.");
      break;
    }
  }
  ReleaseMutex(mutex);
  CloseHandle(mutex);
  fp::FlushLogs();
  TerminateProcess(GetCurrentProcess(), 0);
  return 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  return RunCoreHost();
}

int wmain() {
  return RunCoreHost();
}
