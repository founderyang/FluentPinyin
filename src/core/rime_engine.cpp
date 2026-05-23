#include "core/rime_engine.h"

#include "common/constants.h"
#include "common/logging.h"
#include "common/path_utils.h"

#ifdef FP_WITH_LIBRIME
#include <rime_api.h>
#endif

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace fp::core {
namespace {

std::filesystem::path DefaultSharedDataDir() {
  const auto exe_path = [] {
    std::wstring buffer(32768, L'\0');
    const DWORD length =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
      return std::filesystem::current_path();
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
  }();

  const auto packaged = exe_path / L"rime-data";
  const auto updated_package = fp::GetFpLocalDataPath() / L"Packages" / L"frost" / L"current";
  if (std::filesystem::exists(updated_package / L"default.yaml")) {
    return updated_package;
  }

  if (std::filesystem::exists(packaged)) {
    return packaged;
  }

  return std::filesystem::current_path() / L"schemas" / L"frost" / L"current";
}

std::string WideToUtf8(const std::wstring& value) {
  if (value.empty()) {
    return {};
  }

  const int required = WideCharToMultiByte(CP_UTF8,
                                           0,
                                           value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr);
  if (required <= 0) {
    return {};
  }

  std::string result(static_cast<size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8,
                      0,
                      value.data(),
                      static_cast<int>(value.size()),
                      result.data(),
                      required,
                      nullptr,
                      nullptr);
  return result;
}

std::wstring Utf8ToWide(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return {};
  }

  const int source_length = static_cast<int>(std::strlen(value));
  const int required =
      MultiByteToWideChar(CP_UTF8, 0, value, source_length, nullptr, 0);
  if (required <= 0) {
    return {};
  }

  std::wstring result(static_cast<size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value, source_length, result.data(), required);
  return result;
}

std::wstring PathMessage(const wchar_t* prefix, const std::filesystem::path& path) {
  std::wstring message(prefix);
  message += L": ";
  message += path.wstring();
  return message;
}

void RimeNotification(void*,
                      RimeSessionId,
                      const char* message_type,
                      const char* message_value) {
  fp::LogInfo(L"rime",
              Utf8ToWide(message_type) + L": " + Utf8ToWide(message_value));
}

constexpr int kRimePageDownKey = 0xFF56;

}  // namespace

RimeEngine::~RimeEngine() {
  Shutdown();
}

RimeEngineStatus RimeEngine::Initialize(const RimeEngineOptions& options) {
  if (initialized_) {
    return {.initialized = true, .message = L"Rime engine already initialized."};
  }

#ifndef FP_WITH_LIBRIME
  (void)options;
  fp::LogWarning(L"core", L"RimeEngine built without librime.");
  initialized_ = true;
  return {.initialized = true, .message = L"Rime engine placeholder initialized."};
#else
  shared_data_dir_ =
      options.shared_data_dir.empty() ? DefaultSharedDataDir() : options.shared_data_dir;
  user_data_dir_ = options.user_data_dir.empty() ? fp::GetFpRoamingDataPath() / L"Rime"
                                                 : options.user_data_dir;
  log_dir_ = options.log_dir.empty() ? fp::GetFpLogDirectory() / L"rime" : options.log_dir;
  staging_dir_ = options.staging_dir.empty() ? user_data_dir_ / L"build" : options.staging_dir;
  prebuilt_data_dir_ = options.prebuilt_data_dir.empty() ? shared_data_dir_ / L"build"
                                                         : options.prebuilt_data_dir;
  schema_id_ = options.schema_id.empty() ? "rime_frost" : options.schema_id;

  if (!fp::EnsureDirectory(user_data_dir_) || !fp::EnsureDirectory(log_dir_) ||
      !fp::EnsureDirectory(staging_dir_)) {
    return {.initialized = false, .message = L"Failed to create Rime data directories."};
  }

  if (!std::filesystem::exists(shared_data_dir_ / L"default.yaml")) {
    return {.initialized = false,
            .message = PathMessage(L"Rime shared data not found", shared_data_dir_)};
  }

  if (!EnsureUserConfig()) {
    return {.initialized = false, .message = L"Failed to create Rime user config files."};
  }

  RimeApi* api = rime_get_api();
  if (api == nullptr) {
    return {.initialized = false, .message = L"rime_get_api returned null."};
  }

  const std::string shared_data_dir = WideToUtf8(shared_data_dir_.wstring());
  const std::string user_data_dir = WideToUtf8(user_data_dir_.wstring());
  const std::string log_dir = WideToUtf8(log_dir_.wstring());
  const std::string staging_dir = WideToUtf8(staging_dir_.wstring());
  const std::string prebuilt_data_dir = WideToUtf8(prebuilt_data_dir_.wstring());
  const std::string distribution_name = WideToUtf8(std::wstring(fp::kProductName));
  const std::string distribution_version = WideToUtf8(std::wstring(fp::kProductVersion));

  RIME_STRUCT(RimeTraits, traits);
  traits.shared_data_dir = shared_data_dir.c_str();
  traits.user_data_dir = user_data_dir.c_str();
  traits.distribution_name = distribution_name.c_str();
  traits.distribution_code_name = "FluentPinyin";
  traits.distribution_version = distribution_version.c_str();
  traits.app_name = "rime.fluentpinyin";
  traits.min_log_level = 1;
  traits.log_dir = log_dir.c_str();
  traits.prebuilt_data_dir = prebuilt_data_dir.c_str();
  traits.staging_dir = staging_dir.c_str();

  if (options.deploy) {
    api->setup(&traits);
    api->set_notification_handler(RimeNotification, nullptr);
    api->deployer_initialize(&traits);
    const Bool deploy_result = api->deploy();
    if (!deploy_result) {
      api->finalize();
      return {.initialized = false, .message = L"Rime deploy failed."};
    }
  }

  api->setup(&traits);
  api->set_notification_handler(RimeNotification, nullptr);
  api->initialize(&traits);

  if (RIME_API_AVAILABLE(api, get_version)) {
    rime_version_ = api->get_version();
  }

  initialized_ = true;
  fp::LogInfo(L"core", L"RimeEngine initialized with librime.");
  return {.initialized = true, .message = L"Rime engine initialized."};
#endif
}

void RimeEngine::Shutdown() {
  if (!initialized_) {
    return;
  }

#ifdef FP_WITH_LIBRIME
  if (RimeApi* api = rime_get_api(); api != nullptr) {
    api->finalize();
  }
#endif

  fp::LogInfo(L"core", L"RimeEngine shut down.");
  initialized_ = false;
}

std::vector<RimeCandidateView> RimeEngine::GetCandidatesForInput(const std::string& input,
                                                                 int max_candidates) {
  std::vector<RimeCandidateView> candidates;
  if (!initialized_ || input.empty() || max_candidates <= 0) {
    return candidates;
  }

#ifdef FP_WITH_LIBRIME
  RimeApi* api = rime_get_api();
  if (api == nullptr) {
    return candidates;
  }

  const RimeSessionId session_id = api->create_session();
  if (session_id == 0) {
    return candidates;
  }

  api->select_schema(session_id, schema_id_.c_str());
  for (const unsigned char ch : input) {
    api->process_key(session_id, ch, 0);
  }

  RIME_STRUCT(RimeContext, context);
  if (api->get_context(session_id, &context)) {
    const int count = std::min(context.menu.num_candidates, max_candidates);
    candidates.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) {
      const RimeCandidate& candidate = context.menu.candidates[index];
      candidates.push_back(
          {.text = Utf8ToWide(candidate.text), .comment = Utf8ToWide(candidate.comment)});
    }
    api->free_context(&context);
  }

  api->destroy_session(session_id);
#endif

  return candidates;
}

RimeCandidatePage RimeEngine::GetCandidatePageForInput(const std::string& input,
                                                       int page_index,
                                                       int page_size) {
  RimeCandidatePage page;
  if (!initialized_ || input.empty() || page_size <= 0) {
    return page;
  }

#ifdef FP_WITH_LIBRIME
  RimeApi* api = rime_get_api();
  if (api == nullptr) {
    return page;
  }

  const RimeSessionId session_id = api->create_session();
  if (session_id == 0) {
    return page;
  }

  api->select_schema(session_id, schema_id_.c_str());
  for (const unsigned char ch : input) {
    api->process_key(session_id, ch, 0);
  }

  for (int index = 0; index < page_index; ++index) {
    api->process_key(session_id, kRimePageDownKey, 0);
  }

  RIME_STRUCT(RimeContext, context);
  if (api->get_context(session_id, &context)) {
    page.composition = Utf8ToWide(context.composition.preedit);
    page.has_previous_page = context.menu.page_no > 0;
    page.has_next_page = context.menu.is_last_page == 0;

    const int count = std::min(context.menu.num_candidates, page_size);
    page.candidates.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) {
      const RimeCandidate& candidate = context.menu.candidates[index];
      page.candidates.push_back(
          {.text = Utf8ToWide(candidate.text), .comment = Utf8ToWide(candidate.comment)});
    }
    api->free_context(&context);
  }

  api->destroy_session(session_id);
#else
  (void)page_index;
#endif

  return page;
}

bool RimeEngine::EnsureUserConfig() {
  std::error_code error;
  std::filesystem::create_directories(user_data_dir_, error);
  if (error) {
    return false;
  }

  const auto default_custom = user_data_dir_ / L"default.custom.yaml";
  if (!std::filesystem::exists(default_custom)) {
    std::ofstream file(default_custom, std::ios::binary);
    if (!file) {
      return false;
    }
    file << "patch:\n"
            "  schema_list:\n"
            "    - schema: rime_frost\n"
            "  menu/page_size: 8\n";
  }

  const auto frost_custom = user_data_dir_ / L"rime_frost.custom.yaml";
  if (!std::filesystem::exists(frost_custom)) {
    std::ofstream file(frost_custom, std::ios::binary);
    if (!file) {
      return false;
    }
    file << "patch:\n"
            "  schema/name: \"\\u6d41\\u7545\\u62fc\\u97f3\"\n";
  }

  return true;
}

}  // namespace fp::core
