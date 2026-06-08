#pragma once

#include "common\rime_types.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fp::coreipc {

inline constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\FluentPinyin.CoreHost.V1";
inline constexpr wchar_t kCoreHostMutexName[] = L"Local\\FluentPinyin.CoreHost.V1";
inline constexpr wchar_t kCoreHostExecutableName[] = L"fluent-pinyin-corehost.exe";
inline constexpr DWORD kPipeMessageSizeLimit = 1024 * 1024;

std::wstring PipeName();
std::wstring PipeNameForSuffix(std::wstring_view suffix);
std::wstring CoreHostMutexName();
std::wstring CoreHostMutexNameForSuffix(std::wstring_view suffix);

enum class Command : std::uint32_t {
  kInitialize = 1,
  kShutdown = 2,
  kResetComposition = 3,
  kSetOption = 4,
  kGetCandidatePage = 5,
  kSelectCandidate = 6,
  kRedeploy = 7,
  kHandshake = 8,
  kAuthenticatedRequest = 9,
};

enum class Status : std::uint32_t {
  kOk = 0,
  kError = 1,
};

std::string EncodeInitializeRequest();
std::string EncodeShutdownRequest();
std::string EncodeResetCompositionRequest();
std::string EncodeSetOptionRequest(std::string_view option_name, bool enabled);
std::string EncodeGetCandidatePageRequest(std::string_view input, int page_index, int page_size);
std::string EncodeSelectCandidateRequest(std::string_view input,
                                         int page_index,
                                         int page_size,
                                         size_t candidate_index);
std::string EncodeRedeployRequest();
std::string EncodeHandshakeRequest(std::string_view nonce);
std::string EncodeAuthenticatedRequest(std::string_view secret, std::string_view request);
bool DecodeAuthenticatedRequest(std::string_view payload,
                                std::string_view expected_secret,
                                std::string* request);

bool ReadExact(HANDLE pipe, void* buffer, DWORD bytes);
bool WriteExact(HANDLE pipe, const void* buffer, DWORD bytes);
bool ReadMessage(HANDLE pipe,
                 std::string* payload,
                 DWORD size_limit = kPipeMessageSizeLimit);
bool WriteMessage(HANDLE pipe,
                  std::string_view payload,
                  DWORD size_limit = kPipeMessageSizeLimit);

bool DecodeCommand(std::string_view payload, Command* command, std::vector<std::string>* fields);

std::string EncodeStatusResponse(const fp::core::RimeEngineStatus& status);
bool DecodeStatusResponse(std::string_view payload, fp::core::RimeEngineStatus* status);

std::string EncodeCandidatePageResponse(const fp::core::RimeCandidatePage& page);
bool DecodeCandidatePageResponse(std::string_view payload, fp::core::RimeCandidatePage* page);

std::string EncodeCandidateCommitResponse(const fp::core::RimeCandidateCommit& commit);
bool DecodeCandidateCommitResponse(std::string_view payload, fp::core::RimeCandidateCommit* commit);

std::string EncodeErrorResponse(std::wstring_view message);
std::wstring DecodeErrorMessage(std::string_view payload);
std::string EncodeHandshakeResponse(std::string_view nonce);
bool DecodeHandshakeResponse(std::string_view payload, std::string_view expected_nonce);

}  // namespace fp::coreipc
