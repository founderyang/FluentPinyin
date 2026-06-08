#include "config_winui/lexicon_store.h"

#include "common/encoding.h"
#include "common/path_utils.h"
#include "config_winui/app_paths.h"
#include "config_winui/settings_binding.h"
#include "common/constants.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>

namespace fp::config_winui {

std::wstring TrimLexiconText(std::wstring_view value) {
  size_t first = 0;
  while (first < value.size() && std::iswspace(value[first])) {
    ++first;
  }
  size_t last = value.size();
  while (last > first && std::iswspace(value[last - 1])) {
    --last;
  }
  return std::wstring(value.substr(first, last - first));
}

std::string TrimUtf8(std::string value) {
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t' ||
                           value.front() == '\r' || value.front() == '\n')) {
    value.erase(value.begin());
  }
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t' ||
                            value.back() == '\r' || value.back() == '\n')) {
    value.pop_back();
  }
  return value;
}

std::optional<std::string> ReadFileUtf8(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  std::string bytes = buffer.str();
  if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
      static_cast<unsigned char>(bytes[1]) == 0xBB &&
      static_cast<unsigned char>(bytes[2]) == 0xBF) {
    bytes.erase(0, 3);
  }
  return bytes;
}

bool WriteFileUtf8(const std::filesystem::path& path, std::string_view content) {
  fp::EnsureDirectory(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    return false;
  }
  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  return file.good();
}

std::string ParseYamlScalar(std::string value) {
  value = TrimUtf8(std::move(value));
  if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
    const char quote = value.front();
    const size_t closing_quote = value.find(quote, 1);
    if (closing_quote != std::string::npos) {
      return value.substr(1, closing_quote - 1);
    }
  }

  const size_t comment_pos = value.find('#');
  if (comment_pos != std::string::npos) {
    value = value.substr(0, comment_pos);
  }
  return TrimUtf8(std::move(value));
}

std::optional<std::string> ParseRimeDictName(std::string_view yaml) {
  std::istringstream stream{std::string(yaml)};
  std::string line;
  bool saw_header = false;
  bool saw_body = false;
  std::optional<std::string> name;

  while (std::getline(stream, line)) {
    line = TrimUtf8(line);
    if (line.empty() || line.starts_with("#")) {
      continue;
    }
    if (line == "---") {
      saw_header = true;
      continue;
    }
    if (line == "...") {
      saw_body = true;
      break;
    }

    constexpr std::string_view key = "name:";
    if (line.starts_with(key)) {
      std::string value = ParseYamlScalar(line.substr(key.size()));
      if (!value.empty()) {
        name = value;
      }
    }
  }

  if (!saw_header || !saw_body || !name) {
    return std::nullopt;
  }
  return name;
}

bool IsSafeRimeDictName(std::string_view name) {
  if (name.empty() || name.starts_with("/") || name.starts_with(".") ||
      name.find('\\') != std::string_view::npos ||
      name.find(':') != std::string_view::npos ||
      name.find("..") != std::string_view::npos) {
    return false;
  }

  for (const unsigned char ch : name) {
    const bool is_letter = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    const bool is_digit = ch >= '0' && ch <= '9';
    const bool is_separator = ch == '_' || ch == '-' || ch == '/';
    if (!is_letter && !is_digit && !is_separator) {
      return false;
    }
  }
  return true;
}

std::wstring FileStemDisplayName(const std::filesystem::path& path) {
  std::wstring name = path.filename().wstring();
  constexpr std::wstring_view suffix = L".dict.yaml";
  if (name.size() > suffix.size() &&
      name.substr(name.size() - suffix.size()) == suffix) {
    name.resize(name.size() - suffix.size());
    return name;
  }
  return path.stem().wstring();
}

std::wstring LexiconCountText(int count) {
  return count > 99 ? L"99+" : std::to_wstring(std::max(0, count));
}

bool ManagedDictionaryContains(const std::vector<ManagedDictionaryEntry>& entries,
                               std::string_view name) {
  return std::any_of(entries.begin(), entries.end(), [name](const auto& entry) {
    return entry.name == name;
  });
}

std::vector<ManagedDictionaryEntry> ReadManagedDictionaryManifest() {
  std::vector<ManagedDictionaryEntry> entries;
  const auto manifest = ReadFileUtf8(RimeUserDataPath() / L"fp_user_dicts.txt");
  if (!manifest) {
    return entries;
  }

  std::istringstream stream{*manifest};
  std::string line;
  while (std::getline(stream, line)) {
    line = TrimUtf8(line);
    if (line.empty() || line.starts_with("#")) {
      continue;
    }

    bool enabled = true;
    std::string name = line;
    const size_t tab = line.find('\t');
    if (tab != std::string::npos) {
      name = TrimUtf8(line.substr(0, tab));
      const std::string state = TrimUtf8(line.substr(tab + 1));
      enabled = state != "0" && state != "false" && state != "off" && state != "disabled";
    }

    if (IsSafeRimeDictName(name) && !ManagedDictionaryContains(entries, name)) {
      entries.push_back({name, enabled});
    }
  }
  return entries;
}

std::vector<std::string> EnabledManagedDictionaryNames(
    const std::vector<ManagedDictionaryEntry>& entries) {
  std::vector<std::string> names;
  for (const auto& entry : entries) {
    if (entry.enabled && IsSafeRimeDictName(entry.name)) {
      names.push_back(entry.name);
    }
  }
  return names;
}

std::wstring NormalizePhraseWeight(std::wstring_view value) {
  const std::wstring trimmed = TrimLexiconText(value);
  if (trimmed.empty()) {
    return L"5";
  }
  if (std::all_of(trimmed.begin(), trimmed.end(), [](wchar_t ch) {
        return ch >= L'0' && ch <= L'9';
      })) {
    return trimmed;
  }
  return L"5";
}

std::vector<PhraseEntry> ParseTabSeparatedLexiconLines(std::wstring_view value) {
  std::vector<PhraseEntry> entries;
  std::wstringstream stream{std::wstring(value)};
  std::wstring line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == L'\r') {
      line.pop_back();
    }
    const std::wstring trimmed = TrimLexiconText(line);
    if (trimmed.empty() || trimmed.starts_with(L"#")) {
      continue;
    }

    std::vector<std::wstring> columns;
    size_t start = 0;
    while (start <= line.size()) {
      const size_t tab = line.find(L'\t', start);
      if (tab == std::wstring::npos) {
        columns.push_back(line.substr(start));
        break;
      }
      columns.push_back(line.substr(start, tab - start));
      start = tab + 1;
    }
    if (columns.size() < 2) {
      continue;
    }

    PhraseEntry entry;
    entry.phrase = TrimLexiconText(columns[0]);
    entry.code = TrimLexiconText(columns[1]);
    if (columns.size() >= 3) {
      entry.weight = NormalizePhraseWeight(columns[2]);
    }
    if (!entry.phrase.empty() && !entry.code.empty()) {
      entries.push_back(std::move(entry));
    }
  }
  return entries;
}

std::vector<PhraseEntry> ReadUserLexiconEntries() {
  const auto content =
      ReadFileUtf8(RimeUserDataPath() /
                   (std::wstring(kDefaultManagedUserLexiconName) + L".dict.yaml"));
  if (!content) {
    return {};
  }

  const std::wstring wide = fp::Utf8ToWide(*content);
  const size_t body = wide.find(L"\n...\n");
  if (body == std::wstring::npos) {
    return {};
  }
  return ParseTabSeparatedLexiconLines(std::wstring_view(wide).substr(body + 5));
}

bool WriteUserLexiconEntries(const std::vector<PhraseEntry>& entries) {
  std::ostringstream output;
  output << "# encoding: utf-8\n"
         << "---\n"
         << "name: " << fp::WideToUtf8(kDefaultManagedUserLexiconName) << "\n"
         << "version: \"1\"\n"
         << "sort: by_weight\n"
         << "...\n";
  for (const auto& entry : entries) {
    if (entry.removed) {
      continue;
    }
    const std::wstring phrase = TrimLexiconText(entry.phrase);
    const std::wstring code = TrimLexiconText(entry.code);
    if (phrase.empty() || code.empty()) {
      continue;
    }
    output << fp::WideToUtf8(phrase) << "\t"
           << fp::WideToUtf8(code) << "\t"
           << fp::WideToUtf8(NormalizePhraseWeight(entry.weight)) << "\n";
  }
  return WriteFileUtf8(
      RimeUserDataPath() / (std::wstring(kDefaultManagedUserLexiconName) + L".dict.yaml"),
      output.str());
}

bool UserLexiconHasEntries() {
  return !ReadUserLexiconEntries().empty();
}

namespace {

bool WriteAggregateDictionary(const std::filesystem::path& user_data_dir,
                              std::wstring_view file_name,
                              std::string_view name,
                              std::string_view base_dictionary,
                              const std::vector<std::string>& enabled_names) {
  std::ostringstream aggregate;
  aggregate << "# encoding: utf-8\n"
               "---\n"
               "name: " << name << "\n"
               "version: \"1\"\n"
               "sort: by_weight\n"
               "import_tables:\n"
            << "  - " << base_dictionary << "\n";
  for (const auto& enabled_name : enabled_names) {
    aggregate << "  - " << enabled_name << "\n";
  }
  aggregate << "...\n";
  return WriteFileUtf8(user_data_dir / std::wstring(file_name), aggregate.str());
}

}  // namespace

bool WriteManagedDictionaryIntegrationFiles(
    const std::vector<ManagedDictionaryEntry>& entries,
    bool user_lexicon_enabled,
    bool imported_lexicons_enabled) {
  const auto user_data_dir = RimeUserDataPath();
  fp::EnsureDirectory(user_data_dir);

  std::ostringstream manifest;
  manifest << "# FluentPinyin imported dictionaries.\n"
           << "# Format: dictionary_name<TAB>enabled.\n";
  for (const auto& entry : entries) {
    if (IsSafeRimeDictName(entry.name)) {
      manifest << entry.name << "\t" << (entry.enabled ? "1" : "0") << "\n";
    }
  }
  if (!WriteFileUtf8(user_data_dir / L"fp_user_dicts.txt", manifest.str())) {
    return false;
  }

  std::vector<std::string> enabled_names;
  if (user_lexicon_enabled && UserLexiconHasEntries()) {
    enabled_names.push_back(fp::WideToUtf8(kDefaultManagedUserLexiconName));
  }
  if (imported_lexicons_enabled) {
    auto imported = EnabledManagedDictionaryNames(entries);
    enabled_names.insert(enabled_names.end(), imported.begin(), imported.end());
  }

  if (enabled_names.empty()) {
    std::error_code error;
    std::filesystem::remove(user_data_dir / L"fp_wanxiang.dict.yaml", error);
    std::filesystem::remove(user_data_dir / L"fp_wanxiang_pro.dict.yaml", error);
    return true;
  }

  return WriteAggregateDictionary(user_data_dir,
                                  L"fp_wanxiang.dict.yaml",
                                  "fp_wanxiang",
                                  "wanxiang",
                                  enabled_names) &&
         WriteAggregateDictionary(user_data_dir,
                                  L"fp_wanxiang_pro.dict.yaml",
                                  "fp_wanxiang_pro",
                                  "wanxiang_pro",
                                  enabled_names);
}

bool WriteManagedDictionaryIntegrationFiles(
    const std::vector<ManagedDictionaryEntry>& entries) {
  return WriteManagedDictionaryIntegrationFiles(
      entries,
      ReadBoolSetting(fp::kUserLexiconEnabledSetting, true),
      ReadBoolSetting(fp::kImportedLexiconsEnabledSetting, true));
}

bool ImportManagedDictionary(const std::filesystem::path& source_path,
                             std::wstring* error_message) {
  const auto yaml = ReadFileUtf8(source_path);
  if (!yaml) {
    if (error_message) {
      *error_message = L"\u65e0\u6cd5\u8bfb\u53d6\u6240\u9009\u6587\u4ef6\u3002";
    }
    return false;
  }

  const auto dict_name = ParseRimeDictName(*yaml);
  if (!dict_name || !IsSafeRimeDictName(*dict_name)) {
    if (error_message) {
      *error_message = L"\u8bf7\u9009\u62e9\u5e26\u6709\u5408\u6cd5 name "
                       L"\u5b57\u6bb5\u7684 .dict.yaml \u8bcd\u5e93\u3002";
    }
    return false;
  }

  const auto user_data_dir = RimeUserDataPath();
  fp::EnsureDirectory(user_data_dir);
  const auto target_path = user_data_dir / (fp::Utf8ToWide(*dict_name) + L".dict.yaml");
  if (!WriteFileUtf8(target_path, *yaml)) {
    if (error_message) {
      *error_message = L"\u5199\u5165\u7528\u6237\u8bcd\u5e93\u76ee\u5f55\u5931\u8d25\u3002";
    }
    return false;
  }

  auto entries = ReadManagedDictionaryManifest();
  auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
    return entry.name == *dict_name;
  });
  if (found == entries.end()) {
    entries.push_back({*dict_name, true});
  } else {
    found->enabled = true;
  }

  if (!WriteManagedDictionaryIntegrationFiles(entries)) {
    if (error_message) {
      *error_message = L"\u66f4\u65b0\u8bcd\u5e93\u6e05\u5355\u5931\u8d25\u3002";
    }
    return false;
  }
  return true;
}

bool RemoveManagedDictionaryFile(std::string_view name) {
  if (!IsSafeRimeDictName(name)) {
    return false;
  }
  std::error_code error;
  std::filesystem::remove(RimeUserDataPath() / (fp::Utf8ToWide(name) + L".dict.yaml"), error);
  return !error;
}

std::vector<PhraseEntry> ReadCustomPhraseEntries() {
  const auto content = ReadFileUtf8(RimeUserDataPath() / L"custom_phrase.txt");
  if (!content) {
    return {};
  }
  return ParseTabSeparatedLexiconLines(fp::Utf8ToWide(*content));
}

bool WriteCustomPhraseEntries(const std::vector<PhraseEntry>& entries) {
  std::ostringstream output;
  output << "# FluentPinyin managed custom phrases.\n"
         << "# Format: phrase<TAB>code<TAB>weight.\n";
  for (const auto& entry : entries) {
    if (entry.removed) {
      continue;
    }
    const std::wstring phrase = TrimLexiconText(entry.phrase);
    const std::wstring code = TrimLexiconText(entry.code);
    if (phrase.empty() || code.empty()) {
      continue;
    }
    output << fp::WideToUtf8(phrase) << "\t"
           << fp::WideToUtf8(code) << "\t"
           << fp::WideToUtf8(NormalizePhraseWeight(entry.weight)) << "\n";
  }
  return WriteFileUtf8(RimeUserDataPath() / L"custom_phrase.txt", output.str());
}

int CurrentCustomPhraseCount() {
  return static_cast<int>(ReadCustomPhraseEntries().size());
}

int CurrentUserLexiconCount() {
  return static_cast<int>(ReadUserLexiconEntries().size());
}

int CurrentManagedDictionaryCount(bool enabled_only) {
  const auto entries = ReadManagedDictionaryManifest();
  if (!enabled_only) {
    return static_cast<int>(entries.size());
  }
  return static_cast<int>(std::count_if(entries.begin(), entries.end(), [](const auto& entry) {
    return entry.enabled;
  }));
}

}  // namespace fp::config_winui
