#include "core/rime_engine.h"

#include "common/constants.h"
#include "common/logging.h"
#include "common/path_utils.h"

#ifdef FP_WITH_LIBRIME
#include <rime_api.h>
#endif
#ifdef FP_WITH_OPENCC
#include <opencc/opencc.h>
#endif

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fp::core {
namespace {

std::filesystem::path DefaultSharedDataDir() {
  const auto module_path = [] {
    HMODULE module = nullptr;
    const BOOL found = GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&DefaultSharedDataDir),
        &module);

    std::wstring buffer(32768, L'\0');
    const DWORD length =
        GetModuleFileNameW(found ? module : nullptr,
                           buffer.data(),
                           static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
      return std::filesystem::current_path();
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
  }();

  const auto packaged = module_path / L"rime-data";
  const auto updated_package = fp::GetFpLocalDataPath() / L"Packages" / L"wanxiang" / L"current";
  if (std::filesystem::exists(updated_package / L"default.yaml")) {
    return updated_package;
  }

  if (std::filesystem::exists(packaged)) {
    return packaged;
  }

  return std::filesystem::current_path() / L"schemas" / L"wanxiang" / L"current";
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

std::string Utf8Literal(const char8_t* value) {
  return value == nullptr ? std::string() : std::string(reinterpret_cast<const char*>(value));
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

std::optional<std::filesystem::file_time_type> LastWriteTime(
    const std::filesystem::path& path) {
  std::error_code error;
  const auto value = std::filesystem::last_write_time(path, error);
  if (error) {
    return std::nullopt;
  }
  return value;
}

std::wstring SettingsPath() {
  return (fp::GetFpRoamingDataPath() / L"settings.ini").wstring();
}

std::wstring DecodeSettingsText(const std::string& bytes) {
  if (bytes.empty()) {
    return {};
  }

  if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
      static_cast<unsigned char>(bytes[1]) == 0xFE) {
    const size_t wchar_count = (bytes.size() - 2) / sizeof(wchar_t);
    return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data() + 2), wchar_count);
  }
  if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE &&
      static_cast<unsigned char>(bytes[1]) == 0xFF) {
    std::wstring result;
    result.reserve((bytes.size() - 2) / 2);
    for (size_t index = 2; index + 1 < bytes.size(); index += 2) {
      const wchar_t ch =
          static_cast<wchar_t>((static_cast<unsigned char>(bytes[index]) << 8) |
                               static_cast<unsigned char>(bytes[index + 1]));
      result.push_back(ch);
    }
    return result;
  }

  const char* data = bytes.data();
  int size = static_cast<int>(bytes.size());
  if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
      static_cast<unsigned char>(bytes[1]) == 0xBB &&
      static_cast<unsigned char>(bytes[2]) == 0xBF) {
    data += 3;
    size -= 3;
  }

  int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, size, nullptr, 0);
  UINT code_page = CP_UTF8;
  DWORD flags = MB_ERR_INVALID_CHARS;
  if (required <= 0) {
    code_page = CP_ACP;
    flags = 0;
    required = MultiByteToWideChar(code_page, flags, data, size, nullptr, 0);
  }
  if (required <= 0) {
    return {};
  }

  std::wstring result(static_cast<size_t>(required), L'\0');
  MultiByteToWideChar(code_page, flags, data, size, result.data(), required);
  return result;
}

std::wstring NormalizeSettingLine(std::wstring line) {
  if (!line.empty() && line.front() == L'\ufeff') {
    line.erase(line.begin());
  }
  if (!line.empty() && line.back() == L'\r') {
    line.pop_back();
  }
  return line;
}

std::wstring ReadStringSetting(std::wstring_view key, std::wstring_view default_value = L"") {
  std::ifstream input(SettingsPath(), std::ios::binary);
  if (!input) {
    return std::wstring(default_value);
  }

  std::stringstream buffer;
  buffer << input.rdbuf();
  std::wistringstream lines(DecodeSettingsText(buffer.str()));
  std::wstring line;
  const std::wstring prefix = std::wstring(key) + L"=";
  while (std::getline(lines, line)) {
    line = NormalizeSettingLine(std::move(line));
    if (line.starts_with(prefix)) {
      return line.substr(prefix.size());
    }
  }
  return std::wstring(default_value);
}

constexpr const char* kDefaultFuzzyPinyinRules =
    "nl,ry,hf,rl,kg,en_eng,in_ing,c_ch,z_zh,s_sh";

const std::vector<std::string>& FuzzyPinyinRuleIds() {
  static const std::vector<std::string> rules{
      "nl",
      "ry",
      "hf",
      "rl",
      "kg",
      "en_eng",
      "in_ing",
      "c_ch",
      "z_zh",
      "s_sh",
  };
  return rules;
}

struct FuzzyPinyinCustomRule {
  std::string left;
  std::string right;
};

bool IsKnownFuzzyPinyinRule(const std::string& id) {
  const auto& rules = FuzzyPinyinRuleIds();
  return std::find(rules.begin(), rules.end(), id) != rules.end();
}

std::vector<std::string> ParseFuzzyPinyinRules(const std::wstring& value,
                                               bool default_to_all) {
  std::vector<std::string> result;
  std::wstring token;
  auto append_token = [&]() {
    if (token.empty()) {
      return;
    }
    const std::string id = WideToUtf8(token);
    if (id == "all") {
      for (const auto& rule : FuzzyPinyinRuleIds()) {
        if (std::find(result.begin(), result.end(), rule) == result.end()) {
          result.push_back(rule);
        }
      }
    } else if (IsKnownFuzzyPinyinRule(id) &&
               std::find(result.begin(), result.end(), id) == result.end()) {
      result.push_back(id);
    }
    token.clear();
  };

  for (const wchar_t ch : value) {
    if (ch == L',' || ch == L';' || ch == L'|' || ch == L' ' || ch == L'\t') {
      append_token();
    } else {
      token.push_back(ch);
    }
  }
  append_token();

  if (result.empty() && default_to_all) {
    result = FuzzyPinyinRuleIds();
  }
  return result;
}

std::string JoinFuzzyPinyinRules(const std::vector<std::string>& rules) {
  std::string value;
  for (const auto& rule : rules) {
    if (!IsKnownFuzzyPinyinRule(rule)) {
      continue;
    }
    if (!value.empty()) {
      value += ',';
    }
    value += rule;
  }
  return value;
}

std::string NormalizeFuzzyPinyinCustomRuleToken(const std::wstring& token) {
  std::string normalized;
  bool has_separator = false;
  for (const wchar_t ch : token) {
    if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
      continue;
    }
    if (ch == L'=' || ch == L'/' || ch == L'／' || ch == L'＝' ||
        ch == L'↔' || ch == L'-') {
      if (has_separator) {
        return {};
      }
      normalized.push_back('=');
      has_separator = true;
      continue;
    }
    if (ch >= L'A' && ch <= L'Z') {
      normalized.push_back(static_cast<char>(ch - L'A' + L'a'));
      continue;
    }
    if (ch >= L'a' && ch <= L'z') {
      normalized.push_back(static_cast<char>(ch));
      continue;
    }
    return {};
  }

  const size_t separator = normalized.find('=');
  if (separator == std::string::npos || separator == 0 ||
      separator + 1 >= normalized.size()) {
    return {};
  }
  return normalized;
}

std::vector<FuzzyPinyinCustomRule> ParseFuzzyPinyinCustomRules(
    const std::wstring& value) {
  std::vector<FuzzyPinyinCustomRule> result;
  std::wstring token;
  auto append_token = [&]() {
    const std::string normalized = NormalizeFuzzyPinyinCustomRuleToken(token);
    token.clear();
    if (normalized.empty()) {
      return;
    }
    const size_t separator = normalized.find('=');
    FuzzyPinyinCustomRule rule{normalized.substr(0, separator),
                               normalized.substr(separator + 1)};
    if (rule.left == rule.right || rule.left.empty() || rule.right.empty()) {
      return;
    }
    const auto duplicate = std::find_if(result.begin(), result.end(), [&](const auto& item) {
      return (item.left == rule.left && item.right == rule.right) ||
             (item.left == rule.right && item.right == rule.left);
    });
    if (duplicate == result.end()) {
      result.push_back(std::move(rule));
    }
  };

  for (const wchar_t ch : value) {
    if (ch == L',' || ch == L';' || ch == L'|' || ch == L'\r' || ch == L'\n') {
      append_token();
    } else {
      token.push_back(ch);
    }
  }
  append_token();
  return result;
}

std::string JoinFuzzyPinyinCustomRules(
    const std::vector<FuzzyPinyinCustomRule>& rules) {
  std::string value;
  for (const auto& rule : rules) {
    if (rule.left.empty() || rule.right.empty()) {
      continue;
    }
    if (!value.empty()) {
      value += ',';
    }
    value += rule.left + '=' + rule.right;
  }
  return value;
}

bool HasAsciiPinyinVowel(const std::string& value) {
  return value.find_first_of("aeiou") != std::string::npos;
}

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::string PinyinToneMark(char vowel, int tone) {
  if (tone <= 0) {
    return vowel == 'v' ? Utf8Literal(u8"ü") : std::string(1, vowel);
  }
  switch (vowel) {
    case 'a':
      return tone == 1 ? Utf8Literal(u8"ā")
                       : tone == 2 ? Utf8Literal(u8"á")
                                   : tone == 3 ? Utf8Literal(u8"ǎ")
                                               : Utf8Literal(u8"à");
    case 'e':
      return tone == 1 ? Utf8Literal(u8"ē")
                       : tone == 2 ? Utf8Literal(u8"é")
                                   : tone == 3 ? Utf8Literal(u8"ě")
                                               : Utf8Literal(u8"è");
    case 'i':
      return tone == 1 ? Utf8Literal(u8"ī")
                       : tone == 2 ? Utf8Literal(u8"í")
                                   : tone == 3 ? Utf8Literal(u8"ǐ")
                                               : Utf8Literal(u8"ì");
    case 'o':
      return tone == 1 ? Utf8Literal(u8"ō")
                       : tone == 2 ? Utf8Literal(u8"ó")
                                   : tone == 3 ? Utf8Literal(u8"ǒ")
                                               : Utf8Literal(u8"ò");
    case 'u':
      return tone == 1 ? Utf8Literal(u8"ū")
                       : tone == 2 ? Utf8Literal(u8"ú")
                                   : tone == 3 ? Utf8Literal(u8"ǔ")
                                               : Utf8Literal(u8"ù");
    case 'v':
      return tone == 1 ? Utf8Literal(u8"ǖ")
                       : tone == 2 ? Utf8Literal(u8"ǘ")
                                   : tone == 3 ? Utf8Literal(u8"ǚ")
                                               : Utf8Literal(u8"ǜ");
    default:
      return std::string(1, vowel);
  }
}

size_t PinyinToneMarkIndex(const std::string& token) {
  const size_t a = token.find('a');
  if (a != std::string::npos) {
    return a;
  }
  const size_t e = token.find('e');
  if (e != std::string::npos) {
    return e;
  }
  const size_t ou = token.find("ou");
  if (ou != std::string::npos) {
    return ou;
  }
  for (size_t index = token.size(); index > 0; --index) {
    const char ch = token[index - 1];
    if (ch == 'i' || ch == 'o' || ch == 'u' || ch == 'v') {
      return index - 1;
    }
  }
  return std::string::npos;
}

std::string PinyinToneVariant(const std::string& token, int tone) {
  const size_t mark_index = PinyinToneMarkIndex(token);
  std::string result;
  for (size_t index = 0; index < token.size(); ++index) {
    const char ch = token[index];
    if (index == mark_index) {
      result += PinyinToneMark(ch, tone);
    } else if (ch == 'v') {
      result += Utf8Literal(u8"ü");
    } else {
      result.push_back(ch);
    }
  }
  return result;
}

void AppendCustomFuzzyPrefixRule(std::string& patch,
                                 const std::string& source,
                                 const std::string& target) {
  if (source.empty() || target.empty() || source == target) {
    return;
  }
  patch += "      - derive/^" + source;
  if (target.size() == source.size() + 1 && StartsWith(target, source) &&
      target.back() == 'h') {
    patch += "([^h]*)/" + target + "$1\n";
  } else {
    patch += "/" + target + "\n";
  }
}

void AppendCustomFuzzyFinalRule(std::string& patch,
                                const std::string& source,
                                const std::string& target) {
  if (source.empty() || target.empty() || source == target) {
    return;
  }
  for (int tone = 0; tone <= 4; ++tone) {
    const std::string source_variant = PinyinToneVariant(source, tone);
    const std::string target_variant = PinyinToneVariant(target, tone);
    if (source_variant != target_variant) {
      patch += "      - derive/" + source_variant + "(.*)$/" +
               target_variant + "$1\n";
    }
  }
}

void AppendCustomFuzzyPinyinRulePatch(std::string& patch,
                                      const FuzzyPinyinCustomRule& rule) {
  if (rule.left.empty() || rule.right.empty() || rule.left == rule.right) {
    return;
  }

  if (!HasAsciiPinyinVowel(rule.left) && !HasAsciiPinyinVowel(rule.right)) {
    AppendCustomFuzzyPrefixRule(patch, rule.left, rule.right);
    AppendCustomFuzzyPrefixRule(patch, rule.right, rule.left);
    return;
  }

  AppendCustomFuzzyFinalRule(patch, rule.left, rule.right);
  AppendCustomFuzzyFinalRule(patch, rule.right, rule.left);
}

std::string BuildCustomFuzzyPinyinRulePatch(
    const std::vector<FuzzyPinyinCustomRule>& rules) {
  std::string patch;
  for (const auto& rule : rules) {
    AppendCustomFuzzyPinyinRulePatch(patch, rule);
  }
  return patch;
}

std::string FluentPinyinAlgebraPatch(
    const std::vector<FuzzyPinyinCustomRule>& custom_fuzzy_rules) {
  std::string patch =
      "# FluentPinyin managed algebra customizations.\n"
      "custom_fuzzy:\n"
      "  __append:\n";
  const std::string custom_fuzzy_patch =
      BuildCustomFuzzyPinyinRulePatch(custom_fuzzy_rules);
  if (custom_fuzzy_patch.empty()) {
    patch += "    []\n";
  } else {
    patch += custom_fuzzy_patch;
  }
  return patch;
}

bool ParseBoolSettingValue(const std::wstring& value, bool default_value) {
  if (value == L"1" || value == L"true" || value == L"True" ||
      value == L"TRUE" || value == L"on" || value == L"yes") {
    return true;
  }
  if (value == L"0" || value == L"false" || value == L"False" ||
      value == L"FALSE" || value == L"off" || value == L"no") {
    return false;
  }
  return default_value;
}

bool ReadBoolSetting(std::wstring_view key, bool default_value) {
  return ParseBoolSettingValue(ReadStringSetting(key, default_value ? L"1" : L"0"),
                               default_value);
}

bool WanxiangModeSetting(std::wstring_view key,
                         bool default_value = true,
                         std::wstring_view legacy_key = L"") {
  if (!legacy_key.empty()) {
    const std::wstring marker = L"__fluent_missing_bool__";
    const std::wstring value = ReadStringSetting(key, marker);
    if (value != marker) {
      return ParseBoolSettingValue(value, default_value);
    }
    return ReadBoolSetting(legacy_key, default_value);
  }
  return ReadBoolSetting(key, default_value);
}

struct InputSchemaSelection {
  std::string schema_id = "wanxiang";
  std::string custom_file = "wanxiang.custom.yaml";
  std::string marker = "FluentPinyin managed base schema customizations";
  std::string algebra_path = Utf8Literal(u8"/base/全拼");
  std::string support_algebra = Utf8Literal(u8"全拼");
  std::string cache_key = "pinyin";
  std::string setting_fingerprint =
      "input_scheme=pinyin\n"
      "double_pinyin_scheme=zrm\n"
      "auto_pinyin_correction=1\n"
      "smart_fuzzy_pinyin=1\n"
      "fuzzy_pinyin=0\n"
      "fuzzy_pinyin_rules=nl,ry,hf,rl,kg,en_eng,in_ing,c_ch,z_zh,s_sh\n"
      "fuzzy_pinyin_custom_rules=\n";
  bool auto_pinyin_correction = true;
  bool smart_fuzzy_pinyin = true;
  bool fuzzy_pinyin = false;
  bool user_lexicon_enabled = true;
  bool imported_lexicons_enabled = true;
  bool custom_phrases_enabled = true;
  bool name_input_enabled = true;
  bool wanxiang_reverse_lookup_enabled = true;
  bool wanxiang_unicode_enabled = true;
  bool wanxiang_number_enabled = true;
  bool wanxiang_date_enabled = true;
  bool wanxiang_calculator_enabled = true;
  bool wanxiang_symbol_enabled = true;
  bool wanxiang_quick_symbol_enabled = false;
  bool wanxiang_paired_symbol_enabled = true;
  bool wanxiang_english_enabled = true;
  bool wanxiang_mixed_code_enabled = true;
  bool wanxiang_input_statistics_enabled = false;
  bool wanxiang_auto_phrase_enabled = false;
  bool wanxiang_user_phrase_enabled = false;
  bool wanxiang_schema_shortcuts_enabled = false;
  bool main_translator_user_dict_enabled = true;
  std::vector<std::string> fuzzy_pinyin_rules = FuzzyPinyinRuleIds();
  std::vector<FuzzyPinyinCustomRule> fuzzy_pinyin_custom_rules;
  bool pro = false;
};

void AppendMainTranslatorUserDictFingerprint(InputSchemaSelection* selection) {
  if (selection == nullptr) {
    return;
  }
  selection->setting_fingerprint +=
      "\nmain_translator_user_dict_enabled=" +
      std::string(selection->main_translator_user_dict_enabled ? "1" : "0") + "\n";
}

InputSchemaSelection CurrentInputSchemaSelection() {
  InputSchemaSelection selection;
  const std::wstring input_scheme =
      ReadStringSetting(L"input_scheme", fp::kDefaultInputScheme);
  const std::wstring double_pinyin_scheme =
      ReadStringSetting(L"double_pinyin_scheme", fp::kDefaultDoublePinyinScheme);
  selection.auto_pinyin_correction = ReadBoolSetting(L"auto_pinyin_correction", true);
  selection.smart_fuzzy_pinyin = ReadBoolSetting(L"smart_fuzzy_pinyin", true);
  selection.fuzzy_pinyin = ReadBoolSetting(L"fuzzy_pinyin", false);
  selection.user_lexicon_enabled = ReadBoolSetting(L"user_lexicon_enabled", true);
  selection.imported_lexicons_enabled = ReadBoolSetting(L"imported_lexicons_enabled", true);
  selection.custom_phrases_enabled = ReadBoolSetting(L"custom_phrases_enabled", true);
  selection.name_input_enabled = ReadBoolSetting(L"name_input", true);
  selection.wanxiang_reverse_lookup_enabled =
      WanxiangModeSetting(L"wanxiang_reverse_lookup_enabled", true, L"u_mode");
  selection.wanxiang_unicode_enabled =
      WanxiangModeSetting(L"wanxiang_unicode_enabled", true, L"u_mode");
  selection.wanxiang_number_enabled =
      WanxiangModeSetting(L"wanxiang_number_enabled", true, L"v_mode");
  selection.wanxiang_date_enabled =
      WanxiangModeSetting(L"wanxiang_date_enabled", true, L"v_mode");
  selection.wanxiang_calculator_enabled =
      WanxiangModeSetting(L"wanxiang_calculator_enabled", true, L"v_mode");
  selection.wanxiang_symbol_enabled =
      WanxiangModeSetting(L"wanxiang_symbol_enabled", true);
  selection.wanxiang_quick_symbol_enabled =
      WanxiangModeSetting(L"wanxiang_quick_symbol_enabled", false);
  selection.wanxiang_paired_symbol_enabled =
      WanxiangModeSetting(L"wanxiang_paired_symbol_enabled", true);
  selection.wanxiang_english_enabled =
      WanxiangModeSetting(L"wanxiang_english_enabled", true);
  selection.wanxiang_mixed_code_enabled =
      WanxiangModeSetting(L"wanxiang_mixed_code_enabled", true);
  selection.wanxiang_input_statistics_enabled =
      WanxiangModeSetting(L"wanxiang_input_statistics_enabled", false);
  selection.wanxiang_auto_phrase_enabled =
      WanxiangModeSetting(L"wanxiang_auto_phrase_enabled", false);
  selection.wanxiang_user_phrase_enabled =
      WanxiangModeSetting(L"wanxiang_user_phrase_enabled", false);
  selection.wanxiang_schema_shortcuts_enabled =
      WanxiangModeSetting(L"wanxiang_schema_shortcuts_enabled", false);
  const std::wstring missing_fuzzy_rules_marker = L"__fluent_default_fuzzy_rules__";
  const std::wstring fuzzy_rules_raw =
      ReadStringSetting(L"fuzzy_pinyin_rules", missing_fuzzy_rules_marker.c_str());
  selection.fuzzy_pinyin_rules =
      ParseFuzzyPinyinRules(fuzzy_rules_raw,
                            selection.fuzzy_pinyin &&
                                fuzzy_rules_raw == missing_fuzzy_rules_marker);
  selection.fuzzy_pinyin_custom_rules = ParseFuzzyPinyinCustomRules(
      ReadStringSetting(L"fuzzy_pinyin_custom_rules", L""));
  const std::string fuzzy_rules_fingerprint =
      selection.fuzzy_pinyin ? JoinFuzzyPinyinRules(selection.fuzzy_pinyin_rules) : "";
  const std::string custom_rules_fingerprint =
      selection.fuzzy_pinyin
          ? JoinFuzzyPinyinCustomRules(selection.fuzzy_pinyin_custom_rules)
          : "";
  selection.setting_fingerprint = "input_scheme=" + WideToUtf8(input_scheme) +
                                  "\ndouble_pinyin_scheme=" +
                                  WideToUtf8(double_pinyin_scheme) +
                                  "\nauto_pinyin_correction=" +
                                  (selection.auto_pinyin_correction ? "1" : "0") +
                                  "\nsmart_fuzzy_pinyin=" +
                                  (selection.smart_fuzzy_pinyin ? "1" : "0") +
                                  "\nfuzzy_pinyin=" +
                                  (selection.fuzzy_pinyin ? "1" : "0") +
                                  "\nuser_lexicon_enabled=" +
                                  (selection.user_lexicon_enabled ? "1" : "0") +
                                  "\nimported_lexicons_enabled=" +
                                  (selection.imported_lexicons_enabled ? "1" : "0") +
                                  "\ncustom_phrases_enabled=" +
                                  (selection.custom_phrases_enabled ? "1" : "0") +
                                  "\nname_input=" +
                                  (selection.name_input_enabled ? "1" : "0") +
                                  "\nwanxiang_reverse_lookup_enabled=" +
                                  (selection.wanxiang_reverse_lookup_enabled ? "1" : "0") +
                                  "\nwanxiang_unicode_enabled=" +
                                  (selection.wanxiang_unicode_enabled ? "1" : "0") +
                                  "\nwanxiang_number_enabled=" +
                                  (selection.wanxiang_number_enabled ? "1" : "0") +
                                  "\nwanxiang_date_enabled=" +
                                  (selection.wanxiang_date_enabled ? "1" : "0") +
                                  "\nwanxiang_calculator_enabled=" +
                                  (selection.wanxiang_calculator_enabled ? "1" : "0") +
                                  "\nwanxiang_symbol_enabled=" +
                                  (selection.wanxiang_symbol_enabled ? "1" : "0") +
                                  "\nwanxiang_quick_symbol_enabled=" +
                                  (selection.wanxiang_quick_symbol_enabled ? "1" : "0") +
                                  "\nwanxiang_paired_symbol_enabled=" +
                                  (selection.wanxiang_paired_symbol_enabled ? "1" : "0") +
                                  "\nwanxiang_english_enabled=" +
                                  (selection.wanxiang_english_enabled ? "1" : "0") +
                                  "\nwanxiang_mixed_code_enabled=" +
                                  (selection.wanxiang_mixed_code_enabled ? "1" : "0") +
                                  "\nwanxiang_input_statistics_enabled=" +
                                  (selection.wanxiang_input_statistics_enabled ? "1" : "0") +
                                  "\nwanxiang_auto_phrase_enabled=" +
                                  (selection.wanxiang_auto_phrase_enabled ? "1" : "0") +
                                  "\nwanxiang_user_phrase_enabled=" +
                                  (selection.wanxiang_user_phrase_enabled ? "1" : "0") +
                                  "\nwanxiang_schema_shortcuts_enabled=" +
                                  (selection.wanxiang_schema_shortcuts_enabled ? "1" : "0") +
                                  "\nfuzzy_pinyin_rules=" + fuzzy_rules_fingerprint +
                                  "\nfuzzy_pinyin_custom_rules=" +
                                  custom_rules_fingerprint + "\n";
  if (input_scheme != L"double_pinyin") {
    AppendMainTranslatorUserDictFingerprint(&selection);
    return selection;
  }

  selection.schema_id = "wanxiang_pro";
  selection.custom_file = "wanxiang_pro.custom.yaml";
  selection.marker = "FluentPinyin managed pro schema customizations";
  selection.cache_key = "double_" + WideToUtf8(double_pinyin_scheme);
  selection.pro = true;
  selection.main_translator_user_dict_enabled = false;
  AppendMainTranslatorUserDictFingerprint(&selection);

  if (double_pinyin_scheme == L"flypy") {
    selection.algebra_path = Utf8Literal(u8"/pro/小鹤双拼");
    selection.support_algebra = Utf8Literal(u8"小鹤双拼");
  } else if (double_pinyin_scheme == L"mspy") {
    selection.algebra_path = Utf8Literal(u8"/pro/微软双拼");
    selection.support_algebra = Utf8Literal(u8"微软双拼");
  } else if (double_pinyin_scheme == L"sogou") {
    selection.algebra_path = Utf8Literal(u8"/pro/搜狗双拼");
    selection.support_algebra = Utf8Literal(u8"搜狗双拼");
  } else if (double_pinyin_scheme == L"abc") {
    selection.algebra_path = Utf8Literal(u8"/pro/智能ABC");
    selection.support_algebra = Utf8Literal(u8"智能ABC");
  } else if (double_pinyin_scheme == L"ziguang") {
    selection.algebra_path = Utf8Literal(u8"/pro/紫光双拼");
    selection.support_algebra = Utf8Literal(u8"紫光双拼");
  } else if (double_pinyin_scheme == L"pyjj") {
    selection.algebra_path = Utf8Literal(u8"/pro/拼音加加");
    selection.support_algebra = Utf8Literal(u8"拼音加加");
  } else if (double_pinyin_scheme == L"gbpy") {
    selection.algebra_path = Utf8Literal(u8"/pro/国标双拼");
    selection.support_algebra = Utf8Literal(u8"国标双拼");
  } else if (double_pinyin_scheme == L"lxsq") {
    selection.algebra_path = Utf8Literal(u8"/pro/乱序17");
    selection.support_algebra = Utf8Literal(u8"乱序17");
  } else {
    selection.algebra_path = Utf8Literal(u8"/pro/自然码");
    selection.support_algebra = Utf8Literal(u8"自然码");
  }
  return selection;
}

bool CopyFileIfNewer(const std::filesystem::path& source,
                     const std::filesystem::path& target) {
  std::error_code error;
  if (!std::filesystem::exists(source, error)) {
    return false;
  }

  bool should_copy = !std::filesystem::exists(target, error);
  if (!should_copy) {
    const auto source_time = LastWriteTime(source);
    const auto target_time = LastWriteTime(target);
    should_copy = !target_time || (source_time && *source_time > *target_time);
  }
  if (!should_copy) {
    return false;
  }

  fp::EnsureDirectory(target.parent_path());
  std::filesystem::copy_file(source,
                             target,
                             std::filesystem::copy_options::overwrite_existing,
                             error);
  if (error) {
    fp::LogWarning(L"core", L"Failed to copy Rime data file: " + target.wstring());
    return false;
  }
  return true;
}

bool LinkOrCopyFileIfNewer(const std::filesystem::path& source,
                           const std::filesystem::path& target) {
  const ULONGLONG start_tick = GetTickCount64();
  std::error_code error;
  if (!std::filesystem::exists(source, error)) {
    return false;
  }

  bool should_update = !std::filesystem::exists(target, error);
  if (!should_update) {
    const auto source_time = LastWriteTime(source);
    const auto target_time = LastWriteTime(target);
    should_update = !target_time || (source_time && *source_time > *target_time);
  }
  if (!should_update) {
    return false;
  }

  fp::EnsureDirectory(target.parent_path());

  if (std::filesystem::exists(target, error)) {
    std::filesystem::remove(target, error);
  }

  if (!std::filesystem::exists(target, error) &&
      CreateHardLinkW(target.c_str(), source.c_str(), nullptr) != FALSE) {
    fp::LogInfo(L"core",
                L"Linked Rime data file in " +
                    std::to_wstring(GetTickCount64() - start_tick) + L" ms: " +
                    target.wstring());
    return true;
  }

  std::filesystem::copy_file(source,
                             target,
                             std::filesystem::copy_options::overwrite_existing,
                             error);
  if (error) {
    fp::LogWarning(L"core", L"Failed to copy Rime data file: " + target.wstring());
    return false;
  }
  fp::LogInfo(L"core",
              L"Copied Rime data file in " +
                  std::to_wstring(GetTickCount64() - start_tick) + L" ms: " +
                  target.wstring());
  return true;
}

void EnsureWanxiangRuntimeFiles(const std::filesystem::path& shared_data_dir,
                                const std::filesystem::path& user_data_dir) {
  LinkOrCopyFileIfNewer(shared_data_dir / L"wanxiang-lts-zh-hans.gram",
                        user_data_dir / L"wanxiang-lts-zh-hans.gram");

  for (const auto& dir : {L"lua",
                          L"lua\\predict.userdb",
                          L"lua\\tips.userdb",
                          L"lua\\sequence.userdb",
                          L"lua\\replacer.userdb",
                          L"lua\\stats.userdb",
                          L"lua\\data"}) {
    fp::EnsureDirectory(user_data_dir / dir);
  }
}

std::string WanxiangSupportCustomPatch(const std::string& algebra_name,
                                       const char* kind) {
  std::string patch = "patch:\n"
                      "  speller/algebra:\n";
  if (std::strcmp(kind, "english") == 0) {
    patch += "    __include: wanxiang_algebra:/english/通用规则\n"
             "    __patch: \"wanxiang_algebra:/english/" + algebra_name + "\"\n";
  } else if (std::strcmp(kind, "mixed") == 0) {
    patch +=
        "    __include: wanxiang_algebra:/mixed/通用派生规则\n"
        "    __patch: \"wanxiang_algebra:/mixed/" + algebra_name + "\"\n";
  } else {
    const std::string reverse_patch =
        algebra_name == "乱序17" ? "hslzy" : "hspzn";
    patch += "    __include: \"wanxiang_algebra:/reverse/" + algebra_name + "\"\n"
             "    __patch: \"wanxiang_algebra:/reverse/" + reverse_patch + "\"\n";
  }
  return patch;
}

bool WriteTextIfChanged(const std::filesystem::path& path,
                        const std::string& content,
                        bool* changed) {
  if (changed != nullptr) {
    *changed = false;
  }
  {
    std::ifstream existing(path, std::ios::binary);
    if (existing) {
      std::stringstream buffer;
      buffer << existing.rdbuf();
      if (buffer.str() == content) {
        return true;
      }
    }
  }

  fp::EnsureDirectory(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return false;
  }
  output << content;
  if (!output.good()) {
    return false;
  }
  if (changed != nullptr) {
    *changed = true;
  }
  return true;
}

std::wstring RimeSharedDataSignature(const std::filesystem::path& shared_data_dir);

std::wstring RimeDataSignature(const std::filesystem::path& shared_data_dir,
                               const InputSchemaSelection& selection) {
  std::wstringstream signature;
  signature << fp::kProductVersion << L"\n";
  signature << Utf8ToWide(selection.setting_fingerprint.c_str());
  signature << Utf8ToWide(selection.schema_id.c_str()) << L"\n";
  signature << Utf8ToWide(selection.custom_file.c_str()) << L"\n";
  signature << Utf8ToWide(selection.algebra_path.c_str()) << L"\n";
  signature << RimeSharedDataSignature(shared_data_dir);
  return signature.str();
}

std::wstring RimeSharedDataSignature(const std::filesystem::path& shared_data_dir) {
  std::wstringstream signature;
  const std::filesystem::path tracked_files[] = {
      shared_data_dir / L"default.yaml",
      shared_data_dir / L"wanxiang.schema.yaml",
      shared_data_dir / L"custom" / L"wanxiang_pro.schema.yaml",
      shared_data_dir / L"custom" / L"wanxiang_pro.dict.yaml",
      shared_data_dir / L"wanxiang.dict.yaml",
      shared_data_dir / L"wanxiang_algebra.yaml",
      shared_data_dir / L"wanxiang_mixedcode.schema.yaml",
      shared_data_dir / L"wanxiang_reverse.schema.yaml",
      shared_data_dir / L"wanxiang_english.schema.yaml",
      shared_data_dir / L"wanxiang-lts-zh-hans.gram",
      shared_data_dir / L"version.txt",
  };

  for (const auto& file : tracked_files) {
    signature << file.filename().wstring() << L'=';
    if (const auto timestamp = LastWriteTime(file)) {
      signature << timestamp->time_since_epoch().count();
    } else {
      signature << L"missing";
    }
    signature << L'\n';
  }
  return signature.str();
}

std::filesystem::path SelectionStagingDir(const std::filesystem::path& user_data_dir,
                                          const InputSchemaSelection& selection) {
  if (selection.cache_key == "pinyin") {
    return user_data_dir / L"build";
  }

  std::string safe_key;
  safe_key.reserve(selection.cache_key.size());
  for (char ch : selection.cache_key) {
    const bool safe = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                      (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
    safe_key.push_back(safe ? ch : '_');
  }
  if (safe_key.empty()) {
    safe_key = "double_zrm";
  }
  return user_data_dir / L"build" / L"fluentpinyin" / Utf8ToWide(safe_key.c_str());
}

bool HasBuiltSchemaFiles(const std::filesystem::path& staging_dir,
                         const std::string& schema_id) {
  if (staging_dir.empty() || schema_id.empty()) {
    return false;
  }

  std::error_code error;
  const std::wstring schema_name = Utf8ToWide(schema_id.c_str());
  return std::filesystem::exists(staging_dir / (schema_name + L".schema.yaml"), error) &&
         std::filesystem::exists(staging_dir / (schema_name + L".prism.bin"), error) &&
         std::filesystem::exists(staging_dir / (schema_name + L".table.bin"), error);
}

bool HasFreshRimeBuildCache(const std::filesystem::path& shared_data_dir,
                            const std::filesystem::path& user_data_dir,
                            const std::filesystem::path& staging_dir,
                            const InputSchemaSelection& selection) {
  if (!HasBuiltSchemaFiles(staging_dir, selection.schema_id)) {
    fp::LogInfo(L"core",
                L"Rime build cache miss: missing built schema files in " +
                    staging_dir.wstring());
    return false;
  }

  std::filesystem::path signature_path = staging_dir / L".build-signature";
  if (!std::filesystem::exists(signature_path) && staging_dir == user_data_dir / L"build") {
    signature_path = user_data_dir / L".build-signature";
  }
  const std::filesystem::path shared_data_signature_path =
      staging_dir / L".build-shared-data-signature";
  const std::wstring current_shared_data = RimeSharedDataSignature(shared_data_dir);
  std::wstring previous_shared_data;
  {
    std::wifstream input(shared_data_signature_path);
    std::wstringstream buffer;
    buffer << input.rdbuf();
    previous_shared_data = buffer.str();
  }
  if (!previous_shared_data.empty() && previous_shared_data != current_shared_data) {
    fp::LogInfo(L"core",
                L"Rime build cache note: shared data timestamp changed for " +
                    Utf8ToWide(selection.cache_key.c_str()) + L".");
  }

  std::wifstream input(signature_path);
  std::wstringstream buffer;
  buffer << input.rdbuf();
  const std::wstring previous = buffer.str();
  const std::wstring current = RimeDataSignature(shared_data_dir, selection);
  if (previous != current) {
    const std::wstring setting_fingerprint =
        Utf8ToWide(selection.setting_fingerprint.c_str());
    const bool same_selection =
        previous.find(setting_fingerprint) != std::wstring::npos &&
        previous.find(Utf8ToWide(selection.schema_id.c_str())) != std::wstring::npos &&
        previous.find(Utf8ToWide(selection.custom_file.c_str())) != std::wstring::npos;
    if (same_selection) {
      fp::LogInfo(L"core",
                  L"Rime build cache hit: accepting legacy signature for " +
                      Utf8ToWide(selection.cache_key.c_str()) + L".");
      return true;
    }
    fp::LogInfo(L"core",
                L"Rime build cache miss: signature changed for " +
                    Utf8ToWide(selection.cache_key.c_str()) + L" (" +
                    std::to_wstring(previous.size()) + L" -> " +
                    std::to_wstring(current.size()) + L").");
    return false;
  }
  return true;
}

void AppendYamlListItem(std::string& patch, std::string_view value) {
  patch += "    - ";
  patch += value;
  patch += "\n";
}

void AppendRecognizerPattern(std::string& patch,
                             std::string_view name,
                             bool enabled,
                             std::string_view pattern) {
  patch += "    ";
  patch += name;
  patch += ": \"";
  patch += enabled ? pattern : "^\\uE000$";
  patch += "\"\n";
}

std::string FluentPinyinWanxiangCustomPatch(const InputSchemaSelection& selection,
                                            bool use_imported_dictionary) {
  std::string patch =
      "# " + selection.marker + ".\n"
      "# Stable runtime: keep core pinyin candidates local and avoid Lua userdb locks.\n"
      "patch:\n"
      "  schema/name: \"\\u6d41\\u7545\\u62fc\\u97f3\"\n"
      "  grammar/language: wanxiang-lts-zh-hans\n"
      "  speller/algebra:\n"
      "    __patch:\n";
  if (selection.fuzzy_pinyin) {
    for (const auto& rule : selection.fuzzy_pinyin_rules) {
      if (IsKnownFuzzyPinyinRule(rule)) {
        patch += "      - \"wanxiang_algebra:" +
                 Utf8Literal(u8"/模糊音_") + rule + "\"\n";
      }
    }
    patch += "      - \"fluent_pinyin_algebra:/custom_fuzzy\"\n";
  } else if (selection.smart_fuzzy_pinyin) {
    patch +=
        "      - \"wanxiang_algebra:" + Utf8Literal(u8"/模糊音_en_eng") + "\"\n"
        "      - \"wanxiang_algebra:" + Utf8Literal(u8"/模糊音_in_ing") + "\"\n";
  }
  patch +=
      "      - \"wanxiang_algebra:" + selection.algebra_path + "\"\n";
  if (selection.pro) {
    patch +=
        "      - \"wanxiang_algebra:/pro/直接辅助\"\n";
  }
  patch +=
      "  translator/enable_user_dict: " +
      std::string(selection.main_translator_user_dict_enabled ? "true" : "false") + "\n"
      "  translator/enable_correction: " +
      std::string(selection.auto_pinyin_correction ? "true" : "false") + "\n"
      "  wanxiang_english/enable_user_dict: false\n"
      "  wanxiang_mixedcode/enable_user_dict: false\n"
      "  user_predict/enable_post_predict: false\n"
      "  user_predict/enable_context_reorder: false\n"
      "  user_predict/enable_fallback_reorder: false\n";
  const bool use_managed_dictionary =
      use_imported_dictionary &&
      (selection.user_lexicon_enabled || selection.imported_lexicons_enabled);
  if (use_managed_dictionary) {
    patch += std::string("  translator/dictionary: ") +
             (selection.pro ? "fp_wanxiang_pro" : "fp_wanxiang") + "\n";
  } else if (selection.pro) {
    patch += "  translator/dictionary: wanxiang_pro\n";
  } else {
    patch += "  translator/dictionary: wanxiang\n";
  }
  if (selection.wanxiang_quick_symbol_enabled) {
    patch +=
        "  super_processor/enable_backspace_limit: true\n"
        "  super_processor/enable_seg_loop: true\n"
        "  super_processor/enable_tone_fallback: true\n"
        "  super_processor/enable_predict_space: false\n"
        "  super_processor/kp_number_mode: auto\n"
        "  super_processor/limit_repeated: \"8,40\"\n"
        "  super_processor/select_character: \"[,]\"\n";
  }
  if (selection.wanxiang_date_enabled) {
    patch += "  key_binder/shijian_keys: [\"/\", \"o\"]\n";
  } else {
    patch += "  key_binder/shijian_keys: []\n";
  }
  if (!selection.wanxiang_paired_symbol_enabled) {
    patch += "  paired_symbols/trigger: \"\\uE000\"\n";
  }
  if (!selection.wanxiang_quick_symbol_enabled) {
    patch += "  quick_symbol_text/trigger: \"^\\uE000$\"\n";
  }
  const bool wanxiang_phrase_userdb_enabled =
      selection.wanxiang_auto_phrase_enabled ||
      selection.wanxiang_user_phrase_enabled;
  patch +=
      "  add_user_dict/enable_user_dict: " +
      std::string(wanxiang_phrase_userdb_enabled ? "true" : "false") + "\n"
      "  add_user_dict/enable_auto_phrase: " +
      std::string(selection.wanxiang_auto_phrase_enabled ? "true" : "false") + "\n"
      "  user_dict_set/enable_user_dict: " +
      std::string(wanxiang_phrase_userdb_enabled ? "true" : "false") + "\n";

  patch += "  engine/processors:\n";
  if (selection.wanxiang_quick_symbol_enabled) {
    AppendYamlListItem(patch, "lua_processor@*wanxiang.super_processor");
  }
  AppendYamlListItem(patch, "ascii_composer");
  AppendYamlListItem(patch, "recognizer");
  AppendYamlListItem(patch, "key_binder");
  AppendYamlListItem(patch, "speller");
  AppendYamlListItem(patch, "punctuator");
  AppendYamlListItem(patch, "selector");
  AppendYamlListItem(patch, "navigator");
  AppendYamlListItem(patch, "express_editor");

  patch += "  engine/segmentors:\n";
  AppendYamlListItem(patch, "ascii_segmentor");
  AppendYamlListItem(patch, "matcher");
  AppendYamlListItem(patch, "abc_segmentor");
  if (selection.wanxiang_reverse_lookup_enabled) {
    AppendYamlListItem(patch, "affix_segmentor@wanxiang_reverse");
  }
  if (selection.wanxiang_user_phrase_enabled) {
    AppendYamlListItem(patch, "affix_segmentor@add_user_dict");
  }
  AppendYamlListItem(patch, "punct_segmentor");
  AppendYamlListItem(patch, "fallback_segmentor");

  patch += "  engine/translators:\n";
  AppendYamlListItem(patch, "punct_translator");
  AppendYamlListItem(patch, "script_translator");
  AppendYamlListItem(patch, "lua_translator@*wanxiang.version_display");
  if (selection.wanxiang_schema_shortcuts_enabled) {
    AppendYamlListItem(patch, "lua_translator@*wanxiang.set_schema");
  }
  if (selection.wanxiang_date_enabled) {
    AppendYamlListItem(patch, "lua_translator@*wanxiang.shijian");
  }
  if (selection.wanxiang_unicode_enabled) {
    AppendYamlListItem(patch, "lua_translator@*wanxiang.unicode");
  }
  if (selection.wanxiang_number_enabled) {
    AppendYamlListItem(patch, "lua_translator@*wanxiang.number_translator");
  }
  if (selection.wanxiang_calculator_enabled) {
    AppendYamlListItem(patch, "lua_translator@*wanxiang.super_calculator");
  }
  if (selection.wanxiang_input_statistics_enabled) {
    AppendYamlListItem(patch, "lua_translator@*wanxiang.input_statistics");
  }
  if (selection.custom_phrases_enabled) {
    AppendYamlListItem(patch, "table_translator@custom_phrase");
  }
  if (selection.wanxiang_english_enabled) {
    AppendYamlListItem(patch, "table_translator@wanxiang_english");
  }
  if (selection.wanxiang_mixed_code_enabled) {
    AppendYamlListItem(patch, "table_translator@wanxiang_mixedcode");
  }
  if (selection.wanxiang_reverse_lookup_enabled) {
    AppendYamlListItem(patch, "reverse_lookup_translator@wanxiang_reverse");
  }
  if (selection.wanxiang_user_phrase_enabled) {
    AppendYamlListItem(patch, "script_translator@add_user_dict");
  }
  if (wanxiang_phrase_userdb_enabled) {
    AppendYamlListItem(patch, "script_translator@user_dict_set");
  }

  patch += "  engine/filters:\n";
  if (selection.wanxiang_auto_phrase_enabled) {
    AppendYamlListItem(patch, "lua_filter@*wanxiang.auto_phrase");
  }
  if (selection.wanxiang_reverse_lookup_enabled) {
    AppendYamlListItem(patch, "lua_filter@*wanxiang.super_lookup");
  }
  if (selection.wanxiang_english_enabled) {
    AppendYamlListItem(patch, "lua_filter@*wanxiang.super_english");
  }
  AppendYamlListItem(patch, "lua_filter@*wanxiang.charset_filter");
  AppendYamlListItem(patch, "lua_filter@*wanxiang.super_replacer");
  AppendYamlListItem(patch, "simplifier@s2t");
  AppendYamlListItem(patch, "simplifier@s2hk");
  AppendYamlListItem(patch, "simplifier@s2tw");
  AppendYamlListItem(patch, "lua_filter@*wanxiang.super_comment_preedit");
  AppendYamlListItem(patch, "lua_filter@*wanxiang.super_filter");
  AppendYamlListItem(patch, "uniquifier");

  patch += "  recognizer/import_preset: default\n"
           "  recognizer/patterns:\n";
  AppendRecognizerPattern(patch,
                          "punct",
                          selection.wanxiang_symbol_enabled,
                          "^/([0-9]|10|[A-Za-z]+)$");
  AppendRecognizerPattern(patch,
                          "wanxiang_reverse",
                          selection.wanxiang_reverse_lookup_enabled,
                          "^`[A-Za-z]*$");
  AppendRecognizerPattern(patch,
                          "unicode",
                          selection.wanxiang_unicode_enabled,
                          "^U[a-f0-9]+");
  AppendRecognizerPattern(patch,
                          "number",
                          selection.wanxiang_number_enabled,
                          "^R[0-9]+[.]?[0-9]*");
  AppendRecognizerPattern(patch,
                          "sjc",
                          selection.wanxiang_date_enabled,
                          "^[/o]rc\\\\d+[-+=op]?$");
  AppendRecognizerPattern(patch,
                          "yr1",
                          selection.wanxiang_date_enabled,
                          "^N0[1-9]?0?[1-9]?");
  AppendRecognizerPattern(patch,
                          "yr2",
                          selection.wanxiang_date_enabled,
                          "^N1[02]?0?[1-9]?");
  AppendRecognizerPattern(patch,
                          "yr3",
                          selection.wanxiang_date_enabled,
                          "^N0[1-9]?[1-2]?[1-9]?");
  AppendRecognizerPattern(patch,
                          "yr4",
                          selection.wanxiang_date_enabled,
                          "^N1[02]?[1-2]?[1-9]?");
  AppendRecognizerPattern(patch,
                          "yr5",
                          selection.wanxiang_date_enabled,
                          "^N0[1-9]?3?[01]?");
  AppendRecognizerPattern(patch,
                          "yr6",
                          selection.wanxiang_date_enabled,
                          "^N1[02]?3?[01]?");
  AppendRecognizerPattern(patch,
                          "nyr1",
                          selection.wanxiang_date_enabled,
                          "^N19?[0-9]?[0-9]?[0-1]?[0-9]?[0-9]?[0-9]?");
  AppendRecognizerPattern(patch,
                          "nyr2",
                          selection.wanxiang_date_enabled,
                          "^N20?[0-9]?[0-9]?[0-1]?[0-9]?[0-9]?[0-9]?");
  AppendRecognizerPattern(patch,
                          "calculator",
                          selection.wanxiang_calculator_enabled,
                          "^V.*$");
  AppendRecognizerPattern(patch,
                          "add_user_dict",
                          selection.wanxiang_user_phrase_enabled,
                          "^``[A-Za-z/`']*$");
  AppendRecognizerPattern(patch, "email", true, "^[A-Za-z][-_.0-9A-Za-z]*@.*$");
  AppendRecognizerPattern(patch,
                          "url",
                          true,
                          "^(www[.]|https?:|ftp[.:]|mailto:|file:).*$");
  AppendRecognizerPattern(patch,
                          "htj",
                          selection.wanxiang_input_statistics_enabled,
                          "^/htj.*$");
  AppendRecognizerPattern(patch,
                          "tj",
                          selection.wanxiang_input_statistics_enabled,
                          "^/(qctj|r?tj|[zyn]tj)$");

  patch +=
      "  s2t/option_name: s2t\n"
      "  s2t/opencc_config: s2t.json\n"
      "  s2t/tips: none\n"
      "  s2hk/option_name: s2hk\n"
      "  s2hk/opencc_config: s2hk.json\n"
      "  s2hk/tips: none\n"
      "  s2tw/option_name: s2tw\n"
      "  s2tw/opencc_config: s2tw.json\n"
      "  s2tw/tips: none\n";
  return patch;
}

void EnsureFreshRimeBuildCache(const std::filesystem::path& shared_data_dir,
                               const std::filesystem::path& user_data_dir,
                               const std::filesystem::path& staging_dir,
                               const InputSchemaSelection& selection,
                               bool force_rebuild_cache) {
  const auto signature_path = staging_dir / L".build-signature";
  const auto shared_data_signature_path = staging_dir / L".build-shared-data-signature";
  const std::wstring signature = RimeDataSignature(shared_data_dir, selection);
  const std::wstring shared_data_signature = RimeSharedDataSignature(shared_data_dir);

  std::wstring previous;
  {
    std::wifstream input(signature_path);
    std::wstringstream buffer;
    buffer << input.rdbuf();
    previous = buffer.str();
  }
  std::wstring previous_shared_data;
  {
    std::wifstream input(shared_data_signature_path);
    std::wstringstream buffer;
    buffer << input.rdbuf();
    previous_shared_data = buffer.str();
  }

  const bool shared_data_changed = force_rebuild_cache;
  if (shared_data_changed) {
    const auto legacy_staging = user_data_dir / L"build";
    const auto fluent_staging_root = legacy_staging / L"fluentpinyin";
    if (staging_dir == legacy_staging || staging_dir.parent_path() == fluent_staging_root) {
      std::error_code error;
      std::filesystem::remove_all(staging_dir, error);
      if (error) {
        fp::LogWarning(L"core", L"Failed to clear stale Rime build cache.");
      }
    }
  }
  if (previous != signature || previous_shared_data != shared_data_signature ||
      force_rebuild_cache) {
    fp::EnsureDirectory(staging_dir);
    std::wofstream output(signature_path, std::ios::trunc);
    output << signature;
    std::wofstream shared_output(shared_data_signature_path, std::ios::trunc);
    shared_output << shared_data_signature;
  }
}

void RimeNotification(void*,
                      RimeSessionId,
                      const char* message_type,
                      const char* message_value) {
  fp::LogInfo(L"rime",
              Utf8ToWide(message_type) + L": " + Utf8ToWide(message_value));
}

constexpr int kRimePageDownKey = 0xFF56;

class NamedMutexLock {
 public:
  NamedMutexLock(const wchar_t* name, DWORD timeout_ms) {
    mutex_ = CreateMutexW(nullptr, FALSE, name);
    if (mutex_ != nullptr) {
      const DWORD wait = WaitForSingleObject(mutex_, timeout_ms);
      locked_ = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
    }
  }

  NamedMutexLock(const NamedMutexLock&) = delete;
  NamedMutexLock& operator=(const NamedMutexLock&) = delete;

  ~NamedMutexLock() {
    if (locked_) {
      ReleaseMutex(mutex_);
    }
    if (mutex_ != nullptr) {
      CloseHandle(mutex_);
    }
  }

  [[nodiscard]] bool locked() const noexcept { return locked_; }

 private:
  HANDLE mutex_ = nullptr;
  bool locked_ = false;
};

std::recursive_mutex& RimeApiProcessMutex() {
  static std::recursive_mutex mutex;
  return mutex;
}

bool& RimeApiProcessInitialized() {
  static bool initialized = false;
  return initialized;
}

bool DeployRimeWorkspace(RimeApi* api, RimeTraits* traits) {
  if (api == nullptr || traits == nullptr) {
    return false;
  }

  api->deployer_initialize(traits);
  const Bool deploy_result = api->deploy();
  return deploy_result != 0;
}

}  // namespace

namespace detail {

std::string BuildWanxiangCustomPatchForTesting(bool pro, bool use_imported_dictionary) {
  InputSchemaSelection selection;
  selection.pro = pro;
  selection.schema_id = pro ? "wanxiang_pro" : "wanxiang";
  selection.custom_file = pro ? "wanxiang_pro.custom.yaml" : "wanxiang.custom.yaml";
  selection.marker = pro ? "FluentPinyin managed pro schema customizations"
                         : "FluentPinyin managed base schema customizations";
  selection.main_translator_user_dict_enabled = !pro;
  if (pro) {
    selection.algebra_path = "/pro/test";
    selection.support_algebra = "test";
  }
  return FluentPinyinWanxiangCustomPatch(selection, use_imported_dictionary);
}

std::string BuildWanxiangSettingFingerprintForTesting(bool pro) {
  InputSchemaSelection selection;
  selection.pro = pro;
  selection.schema_id = pro ? "wanxiang_pro" : "wanxiang";
  selection.custom_file = pro ? "wanxiang_pro.custom.yaml" : "wanxiang.custom.yaml";
  selection.main_translator_user_dict_enabled = !pro;
  selection.setting_fingerprint =
      "input_scheme=" + std::string(pro ? "double_pinyin" : "pinyin") +
      "\ndouble_pinyin_scheme=zrm"
      "\nfuzzy_pinyin_rules="
      "\nfuzzy_pinyin_custom_rules=\n";
  AppendMainTranslatorUserDictFingerprint(&selection);
  return selection.setting_fingerprint;
}

}  // namespace detail

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
  const ULONGLONG init_start_tick = GetTickCount64();
  ULONGLONG step_start_tick = init_start_tick;
  auto log_step = [&](std::wstring_view step) {
    const ULONGLONG now = GetTickCount64();
    fp::LogInfo(L"core",
                std::wstring(L"Rime init step ") + std::wstring(step) + L" took " +
                    std::to_wstring(now - step_start_tick) + L" ms.");
    step_start_tick = now;
  };

  shared_data_dir_ =
      options.shared_data_dir.empty() ? DefaultSharedDataDir() : options.shared_data_dir;
  user_data_dir_ = options.user_data_dir.empty() ? fp::GetFpRoamingDataPath() / L"Rime"
                                                  : options.user_data_dir;
  log_dir_ = options.log_dir.empty() ? fp::GetFpLogDirectory() / L"rime" : options.log_dir;
  const InputSchemaSelection schema_selection = CurrentInputSchemaSelection();
  staging_dir_ =
      options.staging_dir.empty() ? SelectionStagingDir(user_data_dir_, schema_selection)
                                  : options.staging_dir;
  prebuilt_data_dir_ = options.prebuilt_data_dir.empty() ? shared_data_dir_ / L"build"
                                                         : options.prebuilt_data_dir;
  schema_id_overridden_ = !options.schema_id.empty();
  schema_id_ = schema_id_overridden_ ? options.schema_id : schema_selection.schema_id;
  user_config_changed_ = false;
  log_step(L"resolve paths and schema settings");

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
  log_step(L"ensure user config");
  EnsureWanxiangRuntimeFiles(shared_data_dir_, user_data_dir_);
  log_step(L"ensure Wanxiang runtime files");

  const bool has_fresh_build_cache =
      HasFreshRimeBuildCache(shared_data_dir_, user_data_dir_, staging_dir_, schema_selection);
  const bool should_deploy = detail::ShouldDeployRimeWorkspace(options.deploy,
                                                               options.force_rebuild_cache,
                                                               user_config_changed_,
                                                               has_fresh_build_cache);
  fp::LogInfo(L"core",
              std::wstring(L"Rime build cache ") +
                  (has_fresh_build_cache ? L"hit" : L"miss") +
                  L"; deploy=" + (should_deploy ? std::wstring(L"yes") : std::wstring(L"no")) +
                  L"; allow_deploy=" +
                  (options.deploy ? std::wstring(L"yes") : std::wstring(L"no")) +
                  L"; force_rebuild=" +
                  (options.force_rebuild_cache ? std::wstring(L"yes") : std::wstring(L"no")) +
                  L"; staging=" + staging_dir_.wstring());
  NamedMutexLock deploy_lock(L"Local\\FluentPinyinRimeDeploy",
                             should_deploy ? 120000 : 120);
  if (!deploy_lock.locked()) {
    if (should_deploy) {
      return {.initialized = false, .message = L"Timed out waiting for Rime deploy lock."};
    }
    fp::LogInfo(L"core",
                L"Rime deploy lock is busy, but build cache is fresh; continuing without deploy.");
  }
  log_step(L"check build cache and acquire deploy lock");

  if (should_deploy) {
    EnsureFreshRimeBuildCache(shared_data_dir_,
                              user_data_dir_,
                              staging_dir_,
                              schema_selection,
                              options.force_rebuild_cache);
    log_step(L"prepare build cache signature");
  }

  RimeApi* api = rime_get_api();
  if (api == nullptr) {
    return {.initialized = false, .message = L"rime_get_api returned null."};
  }

  const std::string shared_data_dir = WideToUtf8(shared_data_dir_.wstring());
  const std::string user_data_dir = WideToUtf8(user_data_dir_.wstring());
  const std::string log_dir;
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

  {
    std::lock_guard<std::recursive_mutex> api_lock(RimeApiProcessMutex());
    bool& process_initialized = RimeApiProcessInitialized();
    if (should_deploy && !process_initialized) {
      const ULONGLONG deploy_start_tick = GetTickCount64();
      api->setup(&traits);
      api->set_notification_handler(RimeNotification, nullptr);
      if (!DeployRimeWorkspace(api, &traits)) {
        api->finalize();
        return {.initialized = false, .message = L"Rime deploy failed."};
      }
      api->finalize();
      fp::LogInfo(L"core",
                  L"Rime deploy completed in " +
                      std::to_wstring(GetTickCount64() - deploy_start_tick) + L" ms.");
    } else if (should_deploy) {
      const ULONGLONG deploy_start_tick = GetTickCount64();
      if (!DeployRimeWorkspace(api, &traits)) {
        return {.initialized = false, .message = L"Rime redeploy failed while librime is active."};
      }
      fp::LogInfo(L"core",
                  L"Rime redeploy completed in " +
                      std::to_wstring(GetTickCount64() - deploy_start_tick) + L" ms.");
    }
    log_step(L"deploy workspace");

    if (!process_initialized) {
      const ULONGLONG initialize_start_tick = GetTickCount64();
      api->setup(&traits);
      api->set_notification_handler(RimeNotification, nullptr);
      api->initialize(&traits);
      process_initialized = true;
      fp::LogInfo(L"core",
                  L"librime process initialize completed in " +
                      std::to_wstring(GetTickCount64() - initialize_start_tick) + L" ms.");
    }
  }
  log_step(L"initialize librime process");

  if (RIME_API_AVAILABLE(api, get_version)) {
    rime_version_ = api->get_version();
  }

  initialized_ = true;
  fp::LogInfo(L"core",
              L"RimeEngine initialized with librime in " +
                  std::to_wstring(GetTickCount64() - init_start_tick) + L" ms.");
  return {.initialized = true, .message = L"Rime engine initialized."};
#endif
}

void RimeEngine::Shutdown() {
  if (!initialized_) {
    return;
  }

  DestroySession();
  CloseOpenCcConverters();

  fp::LogInfo(L"core", L"RimeEngine shut down.");
  initialized_ = false;
}

RimeEngineStatus RimeEngine::Redeploy() {
  if (initialized_) {
    Shutdown();
  }

  const auto signature_path = user_data_dir_.empty()
                                  ? fp::GetFpRoamingDataPath() / L"Rime" / L".build-signature"
                                  : user_data_dir_ / L".build-signature";
  std::error_code error;
  std::filesystem::remove(signature_path, error);
  if (error) {
    fp::LogWarning(L"core", L"Failed to clear Rime build signature before redeploy.");
  }

  RimeEngineOptions options;
  options.force_rebuild_cache = true;
  return Initialize(options);
}

RimeEngineStatus RimeEngine::RedeployDefaultUserData() {
  RimeEngine engine;
  const auto status = engine.Initialize();
  if (!status.initialized) {
    return status;
  }
  return engine.Redeploy();
}

bool RimeEngine::HasBuiltSchema() const {
  return HasBuiltSchemaFiles(staging_dir_, schema_id_);
}

bool RimeEngine::EnsureSession() {
#ifndef FP_WITH_LIBRIME
  return initialized_;
#else
  std::lock_guard<std::recursive_mutex> api_lock(RimeApiProcessMutex());
  if (!initialized_) {
    return false;
  }
  if (session_id_ != 0) {
    return true;
  }

  RimeApi* api = rime_get_api();
  if (api == nullptr) {
    return false;
  }

  const RimeSessionId session_id = api->create_session();
  if (session_id == 0) {
    return false;
  }
  api->select_schema(session_id, schema_id_.c_str());
  session_id_ = static_cast<std::uintptr_t>(session_id);
  session_input_.clear();
  session_page_index_ = 0;
  return true;
#endif
}

void RimeEngine::DestroySession() {
#ifdef FP_WITH_LIBRIME
  std::lock_guard<std::recursive_mutex> api_lock(RimeApiProcessMutex());
  if (session_id_ != 0) {
    if (RimeApi* api = rime_get_api(); api != nullptr) {
      api->destroy_session(static_cast<RimeSessionId>(session_id_));
    }
  }
#endif
  session_id_ = 0;
  session_input_.clear();
  session_page_index_ = 0;
}

void RimeEngine::ResetComposition() {
#ifdef FP_WITH_LIBRIME
  std::lock_guard<std::recursive_mutex> api_lock(RimeApiProcessMutex());
  if (session_id_ != 0) {
    if (RimeApi* api = rime_get_api(); api != nullptr) {
      api->clear_composition(static_cast<RimeSessionId>(session_id_));
    }
  }
#endif
  session_input_.clear();
  session_page_index_ = 0;
}

bool RimeEngine::SetOption(const std::string& option_name, bool enabled) {
  if (option_name == "s2s" && enabled) {
    output_traditional_ = false;
  } else if (option_name == "s2t") {
    output_traditional_ = enabled;
  }

#ifndef FP_WITH_LIBRIME
  (void)option_name;
  (void)enabled;
  return initialized_;
#else
  std::lock_guard<std::recursive_mutex> api_lock(RimeApiProcessMutex());
  if (option_name.empty() || !EnsureSession()) {
    return false;
  }

  RimeApi* api = rime_get_api();
  if (api == nullptr || !RIME_API_AVAILABLE(api, set_option)) {
    return false;
  }

  api->set_option(static_cast<RimeSessionId>(session_id_),
                  option_name.c_str(),
                  enabled ? True : False);
  return true;
#endif
}

bool RimeEngine::GetOption(const std::string& option_name) {
#ifndef FP_WITH_LIBRIME
  (void)option_name;
  return false;
#else
  std::lock_guard<std::recursive_mutex> api_lock(RimeApiProcessMutex());
  if (option_name.empty() || !EnsureSession()) {
    return false;
  }

  RimeApi* api = rime_get_api();
  if (api == nullptr || !RIME_API_AVAILABLE(api, get_option)) {
    return false;
  }

  return api->get_option(static_cast<RimeSessionId>(session_id_), option_name.c_str()) != False;
#endif
}

bool RimeEngine::EnsureOpenCcS2TConverter() {
#ifndef FP_WITH_OPENCC
  return false;
#else
  if (opencc_s2t_ != nullptr) {
    return true;
  }
  if (opencc_s2t_failed_) {
    return false;
  }

  const std::filesystem::path config_path = shared_data_dir_ / L"opencc" / L"s2t.json";
  if (!std::filesystem::exists(config_path)) {
    opencc_s2t_failed_ = true;
    fp::LogWarning(L"core", PathMessage(L"OpenCC s2t config not found", config_path));
    return false;
  }

  opencc_t converter = opencc_open_w(config_path.c_str());
  if (converter == reinterpret_cast<opencc_t>(-1) || converter == nullptr) {
    opencc_s2t_failed_ = true;
    const char* error = opencc_error();
    fp::LogWarning(L"core",
                   L"OpenCC s2t converter init failed: " +
                       (error != nullptr ? Utf8ToWide(error) : std::wstring(L"unknown error")));
    return false;
  }

  opencc_s2t_ = converter;
  return true;
#endif
}

void RimeEngine::CloseOpenCcConverters() {
#ifdef FP_WITH_OPENCC
  if (opencc_s2t_ != nullptr) {
    opencc_close(static_cast<opencc_t>(opencc_s2t_));
    opencc_s2t_ = nullptr;
  }
#endif
  opencc_s2t_failed_ = false;
}

std::wstring RimeEngine::ConvertOutputText(const char* utf8_text) {
  if (utf8_text == nullptr || utf8_text[0] == '\0') {
    return {};
  }
  if (!output_traditional_) {
    return Utf8ToWide(utf8_text);
  }

#ifdef FP_WITH_OPENCC
  if (EnsureOpenCcS2TConverter()) {
    char* converted =
        opencc_convert_utf8(static_cast<opencc_t>(opencc_s2t_), utf8_text, std::strlen(utf8_text));
    if (converted != nullptr) {
      std::wstring result = Utf8ToWide(converted);
      opencc_convert_utf8_free(converted);
      if (!result.empty()) {
        return result;
      }
    }
  }
#endif

  return Utf8ToWide(utf8_text);
}

bool RimeEngine::SyncSessionInput(const std::string& input, bool reset_page) {
#ifndef FP_WITH_LIBRIME
  (void)input;
  (void)reset_page;
  return initialized_;
#else
  std::lock_guard<std::recursive_mutex> api_lock(RimeApiProcessMutex());
  if (!EnsureSession()) {
    return false;
  }

  RimeApi* api = rime_get_api();
  if (api == nullptr) {
    return false;
  }

  const auto session_id = static_cast<RimeSessionId>(session_id_);
  if (input.empty()) {
    api->clear_composition(session_id);
    session_input_.clear();
    session_page_index_ = 0;
    return true;
  }

  if (reset_page) {
    api->clear_composition(session_id);
    session_input_.clear();
    session_page_index_ = 0;
  }

  if (!reset_page && session_page_index_ == 0 && input.size() >= session_input_.size() &&
      input.compare(0, session_input_.size(), session_input_) == 0) {
    for (size_t index = session_input_.size(); index < input.size(); ++index) {
      api->process_key(session_id, static_cast<unsigned char>(input[index]), 0);
    }
    session_input_ = input;
    session_page_index_ = 0;
    return true;
  }

  if (RIME_API_AVAILABLE(api, set_input) && api->set_input(session_id, input.c_str())) {
    session_input_ = input;
    session_page_index_ = 0;
    return true;
  }

  api->clear_composition(session_id);
  for (const unsigned char ch : input) {
    api->process_key(session_id, ch, 0);
  }
  session_input_ = input;
  session_page_index_ = 0;
  return true;
#endif
}

std::vector<RimeCandidateView> RimeEngine::GetCandidatesForInput(const std::string& input,
                                                                 int max_candidates) {
  RimeCandidatePage page = GetCandidatePageForInput(input, 0, max_candidates);
  return page.candidates;
}

RimeCandidatePage RimeEngine::GetCandidatePageForInput(const std::string& input,
                                                       int page_index,
                                                       int page_size) {
  RimeCandidatePage page;
  if (!initialized_ || input.empty() || page_size <= 0) {
    return page;
  }

#ifdef FP_WITH_LIBRIME
  std::lock_guard<std::recursive_mutex> api_lock(RimeApiProcessMutex());
  RimeApi* api = rime_get_api();
  if (api == nullptr) {
    return page;
  }

  const bool reset_page = session_input_ != input || session_page_index_ != 0 || page_index > 0;
  if (!SyncSessionInput(input, reset_page)) {
    return page;
  }
  const auto session_id = static_cast<RimeSessionId>(session_id_);

  const int skip_count = std::max(0, page_index) * page_size;
  page.has_previous_page = page_index > 0;
  page.candidates.reserve(static_cast<size_t>(page_size));

  RIME_STRUCT(RimeContext, initial_context);
  if (api->get_context(session_id, &initial_context)) {
    page.composition = Utf8ToWide(initial_context.composition.preedit);
    api->free_context(&initial_context);
  }

  int skipped = 0;
  bool has_more_pages = true;
  bool last_seen_page_has_more = false;
  int previous_rime_page = -1;
  int iterations = 0;

  while (has_more_pages && static_cast<int>(page.candidates.size()) < page_size &&
         iterations++ < 64) {
    RIME_STRUCT(RimeContext, context);
    if (!api->get_context(session_id, &context)) {
      break;
    }

    if (page.composition.empty()) {
      page.composition = Utf8ToWide(context.composition.preedit);
    }

    has_more_pages = context.menu.is_last_page == 0;
    last_seen_page_has_more = has_more_pages;
    const int current_rime_page = context.menu.page_no;
    if (current_rime_page == previous_rime_page || context.menu.num_candidates <= 0) {
      api->free_context(&context);
      break;
    }
    previous_rime_page = current_rime_page;

    int start_index = 0;
    if (skipped < skip_count) {
      const int remaining_skip = skip_count - skipped;
      if (remaining_skip >= context.menu.num_candidates) {
        skipped += context.menu.num_candidates;
        api->free_context(&context);
        if (!has_more_pages) {
          break;
        }
        api->process_key(session_id, kRimePageDownKey, 0);
        ++session_page_index_;
        continue;
      }
      start_index = remaining_skip;
      skipped = skip_count;
    }

    const int candidates_before = static_cast<int>(page.candidates.size());
    for (int index = start_index;
         index < context.menu.num_candidates && static_cast<int>(page.candidates.size()) < page_size;
         ++index) {
      const RimeCandidate& candidate = context.menu.candidates[index];
      page.candidates.push_back(
          {.text = ConvertOutputText(candidate.text), .comment = Utf8ToWide(candidate.comment)});
    }

    const int consumed_from_context =
        start_index + static_cast<int>(page.candidates.size()) - candidates_before;
    const bool current_page_has_uncollected =
        static_cast<int>(page.candidates.size()) >= page_size &&
        consumed_from_context < context.menu.num_candidates;
    api->free_context(&context);
    if (static_cast<int>(page.candidates.size()) >= page_size) {
      page.has_next_page = current_page_has_uncollected || has_more_pages;
      break;
    }
    if (!has_more_pages) {
      break;
    }
    api->process_key(session_id, kRimePageDownKey, 0);
    ++session_page_index_;
  }

  if (static_cast<int>(page.candidates.size()) < page_size) {
    page.has_next_page = false;
  } else if (!page.has_next_page) {
    page.has_next_page = last_seen_page_has_more;
  }

#else
  (void)page_index;
#endif

  return page;
}

RimeCandidateCommit RimeEngine::SelectCandidateForInput(const std::string& input,
                                                        int page_index,
                                                        int page_size,
                                                        size_t candidate_index) {
  RimeCandidateCommit result;
  if (!initialized_ || input.empty() || page_size <= 0) {
    return result;
  }

#ifdef FP_WITH_LIBRIME
  std::lock_guard<std::recursive_mutex> api_lock(RimeApiProcessMutex());
  RimeApi* api = rime_get_api();
  if (api == nullptr || !RIME_API_AVAILABLE(api, get_context) ||
      !RIME_API_AVAILABLE(api, highlight_candidate_on_current_page)) {
    return result;
  }

  if (!SyncSessionInput(input, true)) {
    return result;
  }
  const auto session_id = static_cast<RimeSessionId>(session_id_);

  int remaining_index = std::max(0, page_index) * page_size +
                        static_cast<int>(candidate_index);
  int iterations = 0;
  while (iterations++ < 64) {
    RIME_STRUCT(RimeContext, context);
    if (!api->get_context(session_id, &context)) {
      return result;
    }
    const int num_candidates = context.menu.num_candidates;
    const bool has_next_page = context.menu.is_last_page == 0;
    if (num_candidates <= 0) {
      api->free_context(&context);
      return result;
    }
    if (remaining_index < num_candidates) {
      if (remaining_index < 0) {
        api->free_context(&context);
        return result;
      }
      const RimeCandidate& candidate = context.menu.candidates[remaining_index];
      const std::wstring selected_text = ConvertOutputText(candidate.text);
      int consumed = context.composition.sel_end;
      const bool already_highlighted =
          context.menu.highlighted_candidate_index == remaining_index;
      api->free_context(&context);

      if (!already_highlighted &&
          !api->highlight_candidate_on_current_page(session_id,
                                                    static_cast<size_t>(remaining_index))) {
        return {};
      }

      if (!already_highlighted) {
        consumed = static_cast<int>(input.size());
        RIME_STRUCT(RimeContext, highlighted_context);
        if (api->get_context(session_id, &highlighted_context)) {
          consumed = highlighted_context.composition.sel_end;
          api->free_context(&highlighted_context);
        }
      }
      consumed = std::clamp(consumed, 0, static_cast<int>(input.size()));
      result.remaining_input = input.substr(static_cast<size_t>(consumed));

      bool selected = false;
      if (RIME_API_AVAILABLE(api, select_candidate_on_current_page)) {
        selected = api->select_candidate_on_current_page(session_id,
                                                         static_cast<size_t>(remaining_index)) !=
                   False;
      }
      if (selected && RIME_API_AVAILABLE(api, get_commit)) {
        RIME_STRUCT(RimeCommit, selected_commit);
        if (api->get_commit(session_id, &selected_commit)) {
          result.text = Utf8ToWide(selected_commit.text);
          if (RIME_API_AVAILABLE(api, free_commit)) {
            api->free_commit(&selected_commit);
          }
        }
      }
      // Commit exactly what the candidate window shows. Some RIME filters
      // such as s2t can transform the visible candidate text while commit
      // retrieval still reports the pre-filter text.
      result.text = selected_text;

      result.handled = !result.text.empty();
      SyncSessionInput(result.remaining_input, true);

      RIME_STRUCT(RimeContext, remaining_context);
      if (api->get_context(session_id, &remaining_context)) {
        result.remaining_composition = Utf8ToWide(remaining_context.composition.preedit);
        api->free_context(&remaining_context);
      }
      break;
    }
    remaining_index -= num_candidates;
    api->free_context(&context);
    if (!has_next_page) {
      return result;
    }
    api->process_key(session_id, kRimePageDownKey, 0);
    ++session_page_index_;
  }
#else
  (void)page_index;
  (void)candidate_index;
#endif
  return result;
}

bool RimeEngine::EnsureUserConfig() {
  std::error_code error;
  std::filesystem::create_directories(user_data_dir_, error);
  if (error) {
    return false;
  }

  const InputSchemaSelection selection = CurrentInputSchemaSelection();
  if (!schema_id_overridden_) {
    schema_id_ = selection.schema_id;
  }

  if (selection.pro) {
    if (CopyFileIfNewer(shared_data_dir_ / L"custom" / L"wanxiang_pro.schema.yaml",
                        user_data_dir_ / L"wanxiang_pro.schema.yaml")) {
      user_config_changed_ = true;
    }
    if (CopyFileIfNewer(shared_data_dir_ / L"custom" / L"wanxiang_pro.dict.yaml",
                        user_data_dir_ / L"wanxiang_pro.dict.yaml")) {
      user_config_changed_ = true;
    }
  }

  const auto default_custom = user_data_dir_ / L"default.custom.yaml";
  const std::string configured_schema_id =
      schema_id_overridden_ ? schema_id_ : selection.schema_id;
  const std::string default_patch =
      "# FluentPinyin managed default schema selection.\n"
      "patch:\n"
      "  schema_list:\n"
      "    - schema: " + configured_schema_id + "\n"
      "  menu/page_size: 9\n"
      "  ascii_composer/switch_key/Caps_Lock: noop\n"
      "  ascii_composer/switch_key/Shift_L: noop\n"
      "  ascii_composer/switch_key/Shift_R: noop\n";
  bool changed = false;
  if (!WriteTextIfChanged(default_custom, default_patch, &changed)) {
    return false;
  }
  if (changed) {
    user_config_changed_ = true;
  }

  const auto selected_custom = user_data_dir_ / Utf8ToWide(selection.custom_file.c_str());
  const bool use_imported_dictionary =
      (selection.user_lexicon_enabled || selection.imported_lexicons_enabled) &&
      std::filesystem::exists(user_data_dir_ /
                              (selection.pro ? L"fp_wanxiang_pro.dict.yaml"
                                             : L"fp_wanxiang.dict.yaml"));
  const std::string selected_patch =
      FluentPinyinWanxiangCustomPatch(selection, use_imported_dictionary);
  if (!WriteTextIfChanged(selected_custom, selected_patch, &changed)) {
    return false;
  }
  if (changed) {
    user_config_changed_ = true;
  }

  if (!WriteTextIfChanged(user_data_dir_ / L"fluent_pinyin_algebra.yaml",
                          FluentPinyinAlgebraPatch(selection.fuzzy_pinyin_custom_rules),
                          &changed)) {
    return false;
  }
  if (changed) {
    user_config_changed_ = true;
  }

  if (!WriteTextIfChanged(user_data_dir_ / L"wanxiang_english.custom.yaml",
                          WanxiangSupportCustomPatch(selection.support_algebra, "english"),
                          &changed)) {
    return false;
  }
  if (changed) {
    user_config_changed_ = true;
  }
  if (!WriteTextIfChanged(user_data_dir_ / L"wanxiang_mixedcode.custom.yaml",
                          WanxiangSupportCustomPatch(selection.support_algebra, "mixed"),
                          &changed)) {
    return false;
  }
  if (changed) {
    user_config_changed_ = true;
  }
  if (!WriteTextIfChanged(user_data_dir_ / L"wanxiang_reverse.custom.yaml",
                          WanxiangSupportCustomPatch(selection.support_algebra, "reverse"),
                          &changed)) {
    return false;
  }
  if (changed) {
    user_config_changed_ = true;
  }

  return true;
}

}  // namespace fp::core
