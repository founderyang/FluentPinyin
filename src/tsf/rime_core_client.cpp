#include "tsf/rime_core_client.h"

#include "common/core_ipc_protocol.h"
#include "common/logging.h"

#include <windows.h>
#include <bcrypt.h>
#include <psapi.h>
#include <softpub.h>
#include <wintrust.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwctype>
#include <cstdio>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fp::tsf {
namespace {

constexpr DWORD kCoreHostConnectTimeoutMs = 2500;
constexpr DWORD kCoreHostInitializeTimeoutMs = 120000;
constexpr DWORD kCoreHostPipeRetryIntervalMs = 25;
constexpr DWORD kCoreHostSecretLimit = 512;

class ScopedHandle {
 public:
  ScopedHandle() = default;
  explicit ScopedHandle(HANDLE handle) : handle_(handle) {}
  ScopedHandle(const ScopedHandle&) = delete;
  ScopedHandle& operator=(const ScopedHandle&) = delete;
  ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.release()) {}
  ScopedHandle& operator=(ScopedHandle&& other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }
  ~ScopedHandle() {
    reset();
  }

  [[nodiscard]] HANDLE get() const noexcept { return handle_; }
  [[nodiscard]] bool valid() const noexcept {
    return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
  }
  HANDLE release() noexcept {
    HANDLE handle = handle_;
    handle_ = nullptr;
    return handle;
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

std::wstring HexTokenWide() {
  std::array<unsigned char, 16> bytes{};
  if (BCryptGenRandom(nullptr,
                      bytes.data(),
                      static_cast<ULONG>(bytes.size()),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
    const ULONGLONG tick = GetTickCount64();
    const DWORD process_id = GetCurrentProcessId();
    for (size_t index = 0; index < bytes.size(); ++index) {
      bytes[index] = static_cast<unsigned char>((tick >> ((index % 8) * 8)) ^
                                                (process_id >> ((index % 4) * 8)) ^
                                                (index * 29));
    }
  }
  constexpr wchar_t hex[] = L"0123456789ABCDEF";
  std::wstring token;
  token.reserve(bytes.size() * 2);
  for (const unsigned char byte : bytes) {
    token.push_back(hex[(byte >> 4) & 0x0F]);
    token.push_back(hex[byte & 0x0F]);
  }
  return token;
}

std::string NarrowAscii(const std::wstring& text) {
  return std::string(text.begin(), text.end());
}

std::string SecretForSuffix(const std::wstring& suffix) {
  return NarrowAscii(suffix) + "." + NarrowAscii(HexTokenWide());
}

const std::wstring& ProcessPipeSuffix() {
  static const std::wstring suffix = HexTokenWide();
  return suffix;
}

const std::string& ProcessIpcSecret() {
  static const std::string secret = SecretForSuffix(ProcessPipeSuffix());
  return secret;
}

std::wstring QuoteCommandLineArg(std::wstring_view value) {
  std::wstring result;
  result.reserve(value.size() + 2);
  result.push_back(L'"');
  size_t backslashes = 0;
  for (wchar_t ch : value) {
    if (ch == L'\\') {
      ++backslashes;
      continue;
    }
    if (ch == L'"') {
      result.append(backslashes * 2 + 1, L'\\');
      result.push_back(ch);
    } else {
      result.append(backslashes, L'\\');
      result.push_back(ch);
    }
    backslashes = 0;
  }
  result.append(backslashes * 2, L'\\');
  result.push_back(L'"');
  return result;
}

std::wstring NormalizePathForCompare(const std::filesystem::path& path) {
  std::error_code error;
  auto weak = std::filesystem::weakly_canonical(path, error);
  if (error) {
    weak = std::filesystem::absolute(path, error);
  }
  std::wstring text = (error ? path : weak).wstring();
  std::transform(text.begin(), text.end(), text.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  return text;
}

std::wstring LastErrorMessage(const wchar_t* prefix, DWORD error) {
  std::wstring message(prefix);
  message += L" Win32 error ";
  message += std::to_wstring(error);
  message += L".";
  return message;
}

bool IpcTraceEnabled() {
  return GetEnvironmentVariableW(L"FLUENT_PINYIN_IPC_TRACE", nullptr, 0) > 0;
}

void IpcTrace(const wchar_t* message, DWORD value = ERROR_SUCCESS) {
  if (!IpcTraceEnabled()) {
    return;
  }
  if (value == ERROR_SUCCESS) {
    fwprintf(stderr, L"[core-ipc] %ls\n", message);
  } else {
    fwprintf(stderr, L"[core-ipc] %ls: %lu\n", message, static_cast<unsigned long>(value));
  }
  fflush(stderr);
}

bool WaitForCoreHostPipe(const std::wstring& pipe_name,
                         DWORD timeout_ms,
                         DWORD* last_error = nullptr) {
  IpcTrace(L"waiting for pipe");
  const ULONGLONG start = GetTickCount64();
  DWORD error = ERROR_FILE_NOT_FOUND;
  for (;;) {
    const ULONGLONG elapsed = GetTickCount64() - start;
    if (elapsed >= timeout_ms) {
      if (last_error != nullptr) {
        *last_error = error;
      }
      return false;
    }
    const DWORD remaining = timeout_ms - static_cast<DWORD>(elapsed);
    const DWORD slice = std::min<DWORD>(remaining, kCoreHostPipeRetryIntervalMs);
    if (WaitNamedPipeW(pipe_name.c_str(), slice)) {
      IpcTrace(L"pipe ready");
      if (last_error != nullptr) {
        *last_error = ERROR_SUCCESS;
      }
      return true;
    }
    error = GetLastError();
    IpcTrace(L"WaitNamedPipe failed", error);
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_SEM_TIMEOUT &&
        error != ERROR_PIPE_BUSY) {
      if (last_error != nullptr) {
        *last_error = error;
      }
      return false;
    }
    Sleep(kCoreHostPipeRetryIntervalMs);
  }
}

bool CreateChildSecretPipe(ScopedHandle* read_handle, ScopedHandle* write_handle) {
  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  HANDLE raw_read = nullptr;
  HANDLE raw_write = nullptr;
  if (!CreatePipe(&raw_read, &raw_write, &security, 0)) {
    return false;
  }
  read_handle->reset(raw_read);
  write_handle->reset(raw_write);
  return SetHandleInformation(write_handle->get(), HANDLE_FLAG_INHERIT, 0) != FALSE;
}

bool WriteChildSecret(HANDLE pipe, std::string_view secret) {
  if (secret.empty() || secret.size() > kCoreHostSecretLimit) {
    return false;
  }
  const auto size = static_cast<std::uint32_t>(secret.size());
  return fp::coreipc::WriteExact(pipe, &size, sizeof(size)) &&
         fp::coreipc::WriteExact(pipe, secret.data(), size);
}

std::optional<std::filesystem::path> ProcessImagePath(DWORD process_id) {
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
  if (process == nullptr) {
    return std::nullopt;
  }
  std::wstring path(32768, L'\0');
  DWORD size = static_cast<DWORD>(path.size());
  if (!QueryFullProcessImageNameW(process, 0, path.data(), &size) || size == 0) {
    CloseHandle(process);
    return std::nullopt;
  }
  CloseHandle(process);
  path.resize(size);
  return std::filesystem::path(path);
}

bool IsTrustedSignedFile(const std::filesystem::path& path) {
  WINTRUST_FILE_INFO file_info{};
  file_info.cbStruct = sizeof(file_info);
  const std::wstring path_text = path.wstring();
  file_info.pcwszFilePath = path_text.c_str();

  WINTRUST_DATA trust_data{};
  trust_data.cbStruct = sizeof(trust_data);
  trust_data.dwUIChoice = WTD_UI_NONE;
  trust_data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
  trust_data.dwUnionChoice = WTD_CHOICE_FILE;
  trust_data.dwStateAction = WTD_STATEACTION_VERIFY;
  trust_data.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
  trust_data.pFile = &file_info;

  GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  const LONG status = WinVerifyTrust(nullptr, &policy, &trust_data);
  trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(nullptr, &policy, &trust_data);
  return status == ERROR_SUCCESS;
}

bool VerifyCoreHostServer(HANDLE pipe, const std::filesystem::path& expected_corehost_path) {
  ULONG server_process_id = 0;
  if (!GetNamedPipeServerProcessId(pipe, &server_process_id) || server_process_id == 0) {
    const DWORD error = GetLastError();
    fp::LogWarning(L"tsf",
                   LastErrorMessage(L"Failed to query FluentPinyin core host pipe server.",
                                    error));
    return false;
  }
  const auto server_path = ProcessImagePath(server_process_id);
  if (!server_path) {
    fp::LogWarning(L"tsf",
                   L"Failed to query FluentPinyin core host process image path.");
    return false;
  }

  const std::wstring actual = NormalizePathForCompare(*server_path);
  const std::wstring expected = NormalizePathForCompare(expected_corehost_path);
  if (actual != expected) {
    fp::LogWarning(L"tsf",
                   L"FluentPinyin core host pipe server path mismatch. actual=" +
                       server_path->wstring() + L", expected=" + expected_corehost_path.wstring());
    return false;
  }

  const bool signed_ok = IsTrustedSignedFile(*server_path);
  if (!signed_ok) {
    fp::LogInfo(L"tsf",
                L"FluentPinyin core host is not Authenticode-signed; accepted by path.");
  }
  return true;
}

}  // namespace

RimeCoreClient::RimeCoreClient(std::filesystem::path corehost_path)
    : RimeCoreClient(std::move(corehost_path),
                     ProcessPipeSuffix(),
                     ProcessIpcSecret()) {}

RimeCoreClient::RimeCoreClient(std::filesystem::path corehost_path,
                               std::wstring pipe_suffix,
                               std::string ipc_secret)
    : corehost_path_(std::move(corehost_path)),
      pipe_suffix_(std::move(pipe_suffix)),
      ipc_secret_(ipc_secret.empty() ? SecretForSuffix(pipe_suffix_) : std::move(ipc_secret)) {}

bool RimeCoreClient::EnsureHostRunning() {
  DWORD wait_error = ERROR_SUCCESS;
  const std::wstring pipe_name = fp::coreipc::PipeNameForSuffix(pipe_suffix_);
  if (WaitForCoreHostPipe(pipe_name, 50, &wait_error)) {
    IpcTrace(L"host pipe already running");
    return true;
  }
  if (wait_error == ERROR_PIPE_BUSY || wait_error == ERROR_SEM_TIMEOUT) {
    if (WaitForCoreHostPipe(pipe_name, kCoreHostConnectTimeoutMs, &wait_error)) {
      return true;
    }
    fp::LogWarning(L"tsf",
                   LastErrorMessage(L"FluentPinyin core host pipe is busy.", wait_error));
    return false;
  }
  if (corehost_path_.empty()) {
    IpcTrace(L"corehost path empty");
    return false;
  }

  std::error_code path_error;
  const auto resolved_corehost = std::filesystem::absolute(corehost_path_, path_error);
  const std::filesystem::path launch_path =
      path_error ? corehost_path_ : resolved_corehost;
  ScopedHandle secret_read;
  ScopedHandle secret_write;
  if (!CreateChildSecretPipe(&secret_read, &secret_write)) {
    const DWORD error = GetLastError();
    fp::LogError(L"tsf",
                 LastErrorMessage(L"Failed to create FluentPinyin core host secret pipe.",
                                  error));
    return false;
  }

  std::wstring command_line =
      QuoteCommandLineArg(launch_path.wstring()) + L" --pipe-suffix " +
      QuoteCommandLineArg(pipe_suffix_) + L" --ipc-secret-handle " +
      std::to_wstring(reinterpret_cast<std::uintptr_t>(secret_read.get())) +
      L" --parent-pid " + std::to_wstring(GetCurrentProcessId());
  ScopedAttributeList attributes(1);
  if (!attributes.valid()) {
    fp::LogError(L"tsf", L"Failed to allocate FluentPinyin core host startup attributes.");
    return false;
  }
  HANDLE inherited_handles[] = {secret_read.get()};
  if (!UpdateProcThreadAttribute(attributes.get(),
                                 0,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                 inherited_handles,
                                 sizeof(inherited_handles),
                                 nullptr,
                                 nullptr)) {
    const DWORD error = GetLastError();
    fp::LogError(L"tsf",
                 LastErrorMessage(L"Failed to configure FluentPinyin core host handle list.",
                                  error));
    return false;
  }

  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
  startup.StartupInfo.wShowWindow = SW_HIDE;
  startup.lpAttributeList = attributes.get();
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessW(launch_path.c_str(),
                                      command_line.data(),
                                      nullptr,
                                      nullptr,
                                      TRUE,
                                      CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
                                      nullptr,
                                      launch_path.parent_path().c_str(),
                                      &startup.StartupInfo,
                                      &process);
  if (!created) {
    const DWORD error = GetLastError();
    IpcTrace(L"CreateProcess corehost failed", error);
    fp::LogError(L"tsf",
                 LastErrorMessage(L"Failed to start FluentPinyin core host.", error));
    return false;
  }
  secret_read.reset();
  const bool secret_written = WriteChildSecret(secret_write.get(), ipc_secret_);
  const DWORD secret_write_error = secret_written ? ERROR_SUCCESS : GetLastError();
  secret_write.reset();
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  if (!secret_written) {
    fp::LogError(L"tsf",
                 LastErrorMessage(L"Failed to send FluentPinyin core host IPC secret.",
                                  secret_write_error));
    return false;
  }
  IpcTrace(L"corehost process launched");
  if (!WaitForCoreHostPipe(pipe_name, kCoreHostConnectTimeoutMs, &wait_error)) {
    IpcTrace(L"timed out waiting for launched corehost", wait_error);
    fp::LogError(L"tsf",
                 LastErrorMessage(L"Timed out waiting for FluentPinyin core host pipe.",
                                  wait_error));
    return false;
  }
  return true;
}

std::optional<std::string> RimeCoreClient::SendRequest(std::string_view request,
                                                       bool allow_start,
                                                       unsigned long wait_timeout_ms) {
  IpcTrace(allow_start ? L"SendRequest allow_start" : L"SendRequest no_start");
  const std::wstring pipe_name = fp::coreipc::PipeNameForSuffix(pipe_suffix_);
  if (allow_start) {
    if (!EnsureHostRunning()) {
      IpcTrace(L"EnsureHostRunning failed");
      return std::nullopt;
    }
  } else if (!WaitForCoreHostPipe(pipe_name, wait_timeout_ms)) {
    IpcTrace(L"WaitForCoreHostPipe without start failed");
    return std::nullopt;
  }

  const ULONGLONG deadline = GetTickCount64() + wait_timeout_ms;
  HANDLE pipe = INVALID_HANDLE_VALUE;
  for (;;) {
    pipe = CreateFileW(pipe_name.c_str(),
                       GENERIC_READ | GENERIC_WRITE,
                       0,
                       nullptr,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       nullptr);
    if (pipe != INVALID_HANDLE_VALUE) {
      IpcTrace(L"CreateFile pipe opened");
      if (!VerifyCoreHostServer(pipe, corehost_path_)) {
        CloseHandle(pipe);
        return std::nullopt;
      }
      break;
    }

    const DWORD create_error = GetLastError();
    IpcTrace(L"CreateFile pipe failed", create_error);
    if (GetTickCount64() >= deadline ||
        (create_error != ERROR_PIPE_BUSY && create_error != ERROR_FILE_NOT_FOUND)) {
      fp::LogWarning(L"tsf",
                     LastErrorMessage(L"Failed to open FluentPinyin core host pipe.",
                                      create_error));
      return std::nullopt;
    }
    if (create_error == ERROR_FILE_NOT_FOUND && allow_start && !EnsureHostRunning()) {
      return std::nullopt;
    }
    if (create_error == ERROR_PIPE_BUSY) {
      WaitNamedPipeW(pipe_name.c_str(), kCoreHostPipeRetryIntervalMs);
    } else {
      Sleep(kCoreHostPipeRetryIntervalMs);
    }
  }

  std::optional<std::string> response;
  std::string payload;
  const std::string authenticated_request =
      fp::coreipc::EncodeAuthenticatedRequest(ipc_secret_, request);
  if (fp::coreipc::WriteMessage(pipe, authenticated_request) &&
      fp::coreipc::ReadMessage(pipe, &payload)) {
    IpcTrace(L"request completed");
    response = std::move(payload);
  } else {
    const DWORD error = GetLastError();
    IpcTrace(L"request write/read failed", error);
    fp::LogWarning(L"tsf",
                   LastErrorMessage(L"FluentPinyin core host request failed.", error));
  }
  CloseHandle(pipe);
  return response;
}

fp::core::RimeEngineStatus RimeCoreClient::Initialize() {
  const auto response = SendRequest(fp::coreipc::EncodeInitializeRequest(),
                                    true,
                                    kCoreHostInitializeTimeoutMs);
  fp::core::RimeEngineStatus status{false, L"Core host unavailable."};
  if (response) {
    if (fp::coreipc::DecodeStatusResponse(*response, &status)) {
      initialized_ = status.initialized;
      return status;
    }
    status.message = L"Invalid core host initialize response: " +
                     fp::coreipc::DecodeErrorMessage(*response) +
                     L"; bytes=" + std::to_wstring(response->size());
  }
  initialized_ = false;
  return status;
}

void RimeCoreClient::Shutdown() {
  initialized_ = false;
}

bool RimeCoreClient::ShutdownSharedEngine() {
  const auto response =
      SendRequest(fp::coreipc::EncodeShutdownRequest(), false, kCoreHostConnectTimeoutMs);
  initialized_ = false;
  fp::core::RimeEngineStatus status;
  return response && fp::coreipc::DecodeStatusResponse(*response, &status) && status.initialized;
}

void RimeCoreClient::ResetComposition() {
  if (!initialized_) {
    return;
  }
  SendRequest(fp::coreipc::EncodeResetCompositionRequest());
}

bool RimeCoreClient::SetOption(const std::string& option_name, bool enabled) {
  if (!initialized_) {
    return false;
  }
  const auto response = SendRequest(fp::coreipc::EncodeSetOptionRequest(option_name, enabled));
  fp::core::RimeEngineStatus status;
  return response && fp::coreipc::DecodeStatusResponse(*response, &status) && status.initialized;
}

fp::core::RimeCandidatePage RimeCoreClient::GetCandidatePageForInput(const std::string& input,
                                                                     int page_index,
                                                                     int page_size) {
  fp::core::RimeCandidatePage page;
  if (!initialized_) {
    return page;
  }
  const auto response =
      SendRequest(fp::coreipc::EncodeGetCandidatePageRequest(input, page_index, page_size));
  if (response) {
    if (!fp::coreipc::DecodeCandidatePageResponse(*response, &page)) {
      fp::LogWarning(L"tsf",
                     L"Failed to decode FluentPinyin core host candidate-page response: " +
                         fp::coreipc::DecodeErrorMessage(*response) +
                         L"; bytes=" + std::to_wstring(response->size()));
    }
  }
  return page;
}

fp::core::RimeCandidateCommit RimeCoreClient::SelectCandidateForInput(
    const std::string& input,
    int page_index,
    int page_size,
    size_t candidate_index) {
  fp::core::RimeCandidateCommit commit;
  if (!initialized_) {
    return commit;
  }
  const auto response = SendRequest(fp::coreipc::EncodeSelectCandidateRequest(
      input, page_index, page_size, candidate_index));
  if (response && !fp::coreipc::DecodeCandidateCommitResponse(*response, &commit)) {
    fp::LogWarning(L"tsf",
                   L"Failed to decode FluentPinyin core host select-candidate response: " +
                       fp::coreipc::DecodeErrorMessage(*response));
  }
  return commit;
}

fp::core::RimeEngineStatus RimeCoreClient::Redeploy() {
  const auto response = SendRequest(fp::coreipc::EncodeRedeployRequest());
  fp::core::RimeEngineStatus status{false, L"Core host unavailable."};
  if (response && fp::coreipc::DecodeStatusResponse(*response, &status)) {
    initialized_ = status.initialized;
    return status;
  }
  initialized_ = false;
  return status;
}

}  // namespace fp::tsf
