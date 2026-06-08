#include "common/core_ipc_protocol.h"

#include "common/encoding.h"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace fp::coreipc {
namespace {

bool IsSafePipeSuffix(std::wstring_view suffix) {
  for (wchar_t ch : suffix) {
    const bool safe = (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                      (ch >= L'0' && ch <= L'9') || ch == L'.' || ch == L'_' ||
                      ch == L'-';
    if (!safe) {
      return false;
    }
  }
  return true;
}

bool ConstantTimeEquals(std::string_view left, std::string_view right) {
  size_t diff = left.size() ^ right.size();
  const size_t max_size = (std::max)(left.size(), right.size());
  for (size_t index = 0; index < max_size; ++index) {
    const unsigned char left_byte =
        index < left.size() ? static_cast<unsigned char>(left[index]) : 0;
    const unsigned char right_byte =
        index < right.size() ? static_cast<unsigned char>(right[index]) : 0;
    diff |= static_cast<size_t>(left_byte ^ right_byte);
  }
  return diff == 0;
}

std::wstring SafePipeSuffixFromEnvironment() {
  wchar_t buffer[128]{};
  const DWORD length = GetEnvironmentVariableW(L"FLUENT_PINYIN_COREHOST_PIPE_SUFFIX",
                                               buffer,
                                               static_cast<DWORD>(std::size(buffer)));
  if (length == 0 || length >= std::size(buffer)) {
    return {};
  }
  std::wstring suffix(buffer, length);
  if (!IsSafePipeSuffix(suffix)) {
    return {};
  }
  return suffix;
}

void AppendUint32(std::string* output, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    output->push_back(static_cast<char>((value >> shift) & 0xFF));
  }
}

std::optional<std::uint32_t> ReadUint32(std::string_view input, size_t* offset) {
  if (*offset + sizeof(std::uint32_t) > input.size()) {
    return std::nullopt;
  }
  std::uint32_t value = 0;
  for (int shift = 0; shift < 32; shift += 8) {
    value |= static_cast<std::uint32_t>(
                 static_cast<unsigned char>(input[(*offset)++])) << shift;
  }
  return value;
}

void AppendField(std::string* output, std::string_view field) {
  AppendUint32(output, static_cast<std::uint32_t>(field.size()));
  output->append(field);
}

bool ReadField(std::string_view input, size_t* offset, std::string* field) {
  const auto length = ReadUint32(input, offset);
  if (!length || *offset + *length > input.size()) {
    return false;
  }
  field->assign(input.substr(*offset, *length));
  *offset += *length;
  return true;
}

std::string Number(int value) {
  return std::to_string(value);
}

std::string Number(size_t value) {
  return std::to_string(value);
}

std::string Bool(bool value) {
  return value ? "1" : "0";
}

bool ParseInt(std::string_view text, int* value) {
  const char* first = text.data();
  const char* last = text.data() + text.size();
  const auto result = std::from_chars(first, last, *value);
  return result.ec == std::errc{} && result.ptr == last;
}

bool ParseSize(std::string_view text, size_t* value) {
  unsigned long long parsed = 0;
  const char* first = text.data();
  const char* last = text.data() + text.size();
  const auto result = std::from_chars(first, last, parsed);
  if (result.ec != std::errc{} || result.ptr != last ||
      parsed > (std::numeric_limits<size_t>::max)()) {
    return false;
  }
  *value = static_cast<size_t>(parsed);
  return true;
}

bool ParseBool(std::string_view text) {
  return text == "1" || text == "true";
}

std::string EncodeMessage(Command command, std::initializer_list<std::string_view> fields) {
  std::string output;
  AppendUint32(&output, static_cast<std::uint32_t>(command));
  AppendUint32(&output, static_cast<std::uint32_t>(fields.size()));
  for (std::string_view field : fields) {
    AppendField(&output, field);
  }
  return output;
}

std::string EncodeResponse(Status status, std::initializer_list<std::string_view> fields) {
  std::string output;
  AppendUint32(&output, static_cast<std::uint32_t>(status));
  AppendUint32(&output, static_cast<std::uint32_t>(fields.size()));
  for (std::string_view field : fields) {
    AppendField(&output, field);
  }
  return output;
}

bool DecodeResponse(std::string_view payload, Status* status, std::vector<std::string>* fields) {
  if (status == nullptr || fields == nullptr) {
    return false;
  }
  size_t offset = 0;
  const auto raw_status = ReadUint32(payload, &offset);
  const auto field_count = ReadUint32(payload, &offset);
  if (!raw_status || !field_count) {
    return false;
  }
  *status = static_cast<Status>(*raw_status);
  fields->clear();
  fields->reserve(*field_count);
  for (std::uint32_t index = 0; index < *field_count; ++index) {
    std::string field;
    if (!ReadField(payload, &offset, &field)) {
      return false;
    }
    fields->push_back(std::move(field));
  }
  return offset == payload.size();
}

}  // namespace

std::wstring PipeName() {
  return PipeNameForSuffix(SafePipeSuffixFromEnvironment());
}

std::wstring PipeNameForSuffix(std::wstring_view suffix) {
  if (suffix.empty()) {
    return kPipeName;
  }
  if (!IsSafePipeSuffix(suffix)) {
    return kPipeName;
  }
  return L"\\\\.\\pipe\\FluentPinyin.CoreHost.V1." + std::wstring(suffix);
}

std::wstring CoreHostMutexName() {
  return CoreHostMutexNameForSuffix(SafePipeSuffixFromEnvironment());
}

std::wstring CoreHostMutexNameForSuffix(std::wstring_view suffix) {
  if (suffix.empty()) {
    return kCoreHostMutexName;
  }
  if (!IsSafePipeSuffix(suffix)) {
    return kCoreHostMutexName;
  }
  return std::wstring(kCoreHostMutexName) + L"." + std::wstring(suffix);
}

std::string EncodeInitializeRequest() {
  return EncodeMessage(Command::kInitialize, {});
}

std::string EncodeShutdownRequest() {
  return EncodeMessage(Command::kShutdown, {});
}

std::string EncodeResetCompositionRequest() {
  return EncodeMessage(Command::kResetComposition, {});
}

std::string EncodeSetOptionRequest(std::string_view option_name, bool enabled) {
  return EncodeMessage(Command::kSetOption, {option_name, Bool(enabled)});
}

std::string EncodeGetCandidatePageRequest(std::string_view input, int page_index, int page_size) {
  return EncodeMessage(Command::kGetCandidatePage, {input, Number(page_index), Number(page_size)});
}

std::string EncodeSelectCandidateRequest(std::string_view input,
                                         int page_index,
                                         int page_size,
                                         size_t candidate_index) {
  return EncodeMessage(Command::kSelectCandidate,
                       {input, Number(page_index), Number(page_size), Number(candidate_index)});
}

std::string EncodeRedeployRequest() {
  return EncodeMessage(Command::kRedeploy, {});
}

std::string EncodeHandshakeRequest(std::string_view nonce) {
  return EncodeMessage(Command::kHandshake, {nonce});
}

std::string EncodeAuthenticatedRequest(std::string_view secret, std::string_view request) {
  return EncodeMessage(Command::kAuthenticatedRequest, {secret, request});
}

bool DecodeAuthenticatedRequest(std::string_view payload,
                                std::string_view expected_secret,
                                std::string* request) {
  if (request == nullptr) {
    return false;
  }
  Command command{};
  std::vector<std::string> fields;
  if (!DecodeCommand(payload, &command, &fields) ||
      command != Command::kAuthenticatedRequest || fields.size() != 2 ||
      !ConstantTimeEquals(fields[0], expected_secret)) {
    return false;
  }
  *request = std::move(fields[1]);
  return true;
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

bool ReadMessage(HANDLE pipe, std::string* payload, DWORD size_limit) {
  std::uint32_t size = 0;
  if (payload == nullptr || !ReadExact(pipe, &size, sizeof(size)) || size > size_limit) {
    return false;
  }
  payload->assign(size, '\0');
  return size == 0 || ReadExact(pipe, payload->data(), size);
}

bool WriteMessage(HANDLE pipe, std::string_view payload, DWORD size_limit) {
  if (payload.size() > size_limit) {
    return false;
  }
  const auto size = static_cast<std::uint32_t>(payload.size());
  return WriteExact(pipe, &size, sizeof(size)) &&
         (size == 0 || WriteExact(pipe, payload.data(), size));
}

bool DecodeCommand(std::string_view payload, Command* command, std::vector<std::string>* fields) {
  if (command == nullptr || fields == nullptr) {
    return false;
  }
  size_t offset = 0;
  const auto raw_command = ReadUint32(payload, &offset);
  const auto field_count = ReadUint32(payload, &offset);
  if (!raw_command || !field_count) {
    return false;
  }
  *command = static_cast<Command>(*raw_command);
  fields->clear();
  fields->reserve(*field_count);
  for (std::uint32_t index = 0; index < *field_count; ++index) {
    std::string field;
    if (!ReadField(payload, &offset, &field)) {
      return false;
    }
    fields->push_back(std::move(field));
  }
  return offset == payload.size();
}

std::string EncodeStatusResponse(const fp::core::RimeEngineStatus& status) {
  return EncodeResponse(status.initialized ? Status::kOk : Status::kError,
                        {Bool(status.initialized), WideToUtf8(status.message)});
}

bool DecodeStatusResponse(std::string_view payload, fp::core::RimeEngineStatus* status) {
  Status response_status = Status::kError;
  std::vector<std::string> fields;
  if (status == nullptr || !DecodeResponse(payload, &response_status, &fields) ||
      fields.empty()) {
    return false;
  }
  if (response_status != Status::kOk && fields.size() == 1) {
    status->initialized = false;
    status->message = Utf8ToWide(fields[0]);
    return true;
  }
  if (fields.size() < 2) {
    return false;
  }
  status->initialized = response_status == Status::kOk && ParseBool(fields[0]);
  status->message = Utf8ToWide(fields[1]);
  return true;
}

std::string EncodeCandidatePageResponse(const fp::core::RimeCandidatePage& page) {
  std::vector<std::string> owned_fields;
  owned_fields.reserve(4 + page.candidates.size() * 2);
  owned_fields.push_back(WideToUtf8(page.composition));
  owned_fields.push_back(Bool(page.has_previous_page));
  owned_fields.push_back(Bool(page.has_next_page));
  owned_fields.push_back(Number(page.candidates.size()));
  for (const auto& candidate : page.candidates) {
    owned_fields.push_back(WideToUtf8(candidate.text));
    owned_fields.push_back(WideToUtf8(candidate.comment));
  }

  std::string output;
  AppendUint32(&output, static_cast<std::uint32_t>(Status::kOk));
  AppendUint32(&output, static_cast<std::uint32_t>(owned_fields.size()));
  for (const auto& field : owned_fields) {
    AppendField(&output, field);
  }
  return output;
}

bool DecodeCandidatePageResponse(std::string_view payload, fp::core::RimeCandidatePage* page) {
  Status status = Status::kError;
  std::vector<std::string> fields;
  if (page == nullptr || !DecodeResponse(payload, &status, &fields) ||
      status != Status::kOk || fields.size() < 4) {
    return false;
  }
  size_t candidate_count = 0;
  if (!ParseSize(fields[3], &candidate_count) ||
      candidate_count > ((std::numeric_limits<size_t>::max)() - 4) / 2 ||
      fields.size() != 4 + candidate_count * 2) {
    return false;
  }
  page->composition = Utf8ToWide(fields[0]);
  page->has_previous_page = ParseBool(fields[1]);
  page->has_next_page = ParseBool(fields[2]);
  page->candidates.clear();
  page->candidates.reserve(candidate_count);
  for (size_t index = 0; index < candidate_count; ++index) {
    page->candidates.push_back({
        Utf8ToWide(fields[4 + index * 2]),
        Utf8ToWide(fields[5 + index * 2]),
    });
  }
  return true;
}

std::string EncodeCandidateCommitResponse(const fp::core::RimeCandidateCommit& commit) {
  return EncodeResponse(Status::kOk,
                        {Bool(commit.handled),
                         WideToUtf8(commit.text),
                         commit.remaining_input,
                         WideToUtf8(commit.remaining_composition)});
}

bool DecodeCandidateCommitResponse(std::string_view payload,
                                   fp::core::RimeCandidateCommit* commit) {
  Status status = Status::kError;
  std::vector<std::string> fields;
  if (commit == nullptr || !DecodeResponse(payload, &status, &fields) ||
      status != Status::kOk || fields.size() != 4) {
    return false;
  }
  commit->handled = ParseBool(fields[0]);
  commit->text = Utf8ToWide(fields[1]);
  commit->remaining_input = fields[2];
  commit->remaining_composition = Utf8ToWide(fields[3]);
  return true;
}

std::string EncodeErrorResponse(std::wstring_view message) {
  return EncodeResponse(Status::kError, {WideToUtf8(message)});
}

std::wstring DecodeErrorMessage(std::string_view payload) {
  Status status = Status::kError;
  std::vector<std::string> fields;
  if (!DecodeResponse(payload, &status, &fields) || fields.empty()) {
    return L"Invalid core host response.";
  }
  return Utf8ToWide(fields[0]);
}

std::string EncodeHandshakeResponse(std::string_view nonce) {
  return EncodeResponse(Status::kOk, {nonce});
}

bool DecodeHandshakeResponse(std::string_view payload, std::string_view expected_nonce) {
  Status status = Status::kError;
  std::vector<std::string> fields;
  return DecodeResponse(payload, &status, &fields) && status == Status::kOk &&
         fields.size() == 1 && fields[0] == expected_nonce;
}

}  // namespace fp::coreipc
