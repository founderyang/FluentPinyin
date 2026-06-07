#include "tsf/rime_core_client.h"

#include "common/core_ipc_protocol.h"
#include "common/logging.h"

#include <windows.h>
#include <psapi.h>
#include <softpub.h>
#include <wintrust.h>

#include <algorithm>
#include <cwctype>
#include <cstdio>
#include <string>
#include <system_error>

namespace fp::tsf {
namespace {

constexpr DWORD kCoreHostConnectTimeoutMs = 2500;
constexpr DWORD kCoreHostInitializeTimeoutMs = 120000;
constexpr DWORD kCoreHostPipeBufferLimit = 1024 * 1024;
constexpr DWORD kCoreHostPipeRetryIntervalMs = 25;

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

bool WaitForCoreHostPipe(DWORD timeout_ms, DWORD* last_error = nullptr) {
  const std::wstring pipe_name = fp::coreipc::PipeName();
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

bool WriteMessage(HANDLE pipe, std::string_view payload) {
  if (payload.size() > kCoreHostPipeBufferLimit) {
    return false;
  }
  const auto size = static_cast<std::uint32_t>(payload.size());
  return WriteExact(pipe, &size, sizeof(size)) &&
         (size == 0 || WriteExact(pipe, payload.data(), size));
}

bool ReadMessage(HANDLE pipe, std::string* payload) {
  std::uint32_t size = 0;
  if (payload == nullptr || !ReadExact(pipe, &size, sizeof(size)) ||
      size > kCoreHostPipeBufferLimit) {
    return false;
  }
  payload->assign(size, '\0');
  return size == 0 || ReadExact(pipe, payload->data(), size);
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
    : corehost_path_(std::move(corehost_path)) {}

bool RimeCoreClient::EnsureHostRunning() {
  DWORD wait_error = ERROR_SUCCESS;
  if (WaitForCoreHostPipe(50, &wait_error)) {
    IpcTrace(L"host pipe already running");
    return true;
  }
  if (wait_error == ERROR_PIPE_BUSY || wait_error == ERROR_SEM_TIMEOUT) {
    if (WaitForCoreHostPipe(kCoreHostConnectTimeoutMs, &wait_error)) {
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
  std::wstring command_line = L"\"" + launch_path.wstring() + L"\"";
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESHOWWINDOW;
  startup.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessW(launch_path.c_str(),
                                      command_line.data(),
                                      nullptr,
                                      nullptr,
                                      FALSE,
                                      CREATE_NO_WINDOW,
                                      nullptr,
                                      launch_path.parent_path().c_str(),
                                      &startup,
                                      &process);
  if (!created) {
    const DWORD error = GetLastError();
    IpcTrace(L"CreateProcess corehost failed", error);
    fp::LogError(L"tsf",
                 LastErrorMessage(L"Failed to start FluentPinyin core host.", error));
    return false;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  IpcTrace(L"corehost process launched");
  if (!WaitForCoreHostPipe(kCoreHostConnectTimeoutMs, &wait_error)) {
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
  if (allow_start) {
    if (!EnsureHostRunning()) {
      IpcTrace(L"EnsureHostRunning failed");
      return std::nullopt;
    }
  } else if (!WaitForCoreHostPipe(wait_timeout_ms)) {
    IpcTrace(L"WaitForCoreHostPipe without start failed");
    return std::nullopt;
  }

  const ULONGLONG deadline = GetTickCount64() + wait_timeout_ms;
  const std::wstring pipe_name = fp::coreipc::PipeName();
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
  if (WriteMessage(pipe, request) && ReadMessage(pipe, &payload)) {
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
