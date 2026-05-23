#include "common/path_utils.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::wstring Utf8ToWide(std::string_view value) {
  if (value.empty()) {
    return {};
  }

  const int required = MultiByteToWideChar(CP_UTF8,
                                           0,
                                           value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr,
                                           0);
  if (required <= 0) {
    return {};
  }

  std::wstring result(static_cast<size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8,
                      0,
                      value.data(),
                      static_cast<int>(value.size()),
                      result.data(),
                      required);
  return result;
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

std::string Trim(std::string value) {
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

std::string ParseYamlScalar(std::string value) {
  value = Trim(std::move(value));
  if (value.size() >= 2 &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1, value.size() - 2);
  }

  const size_t comment_pos = value.find('#');
  if (comment_pos != std::string::npos) {
    value = value.substr(0, comment_pos);
  }
  return Trim(std::move(value));
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

std::optional<std::string> ParseRimeDictName(std::string_view yaml) {
  std::istringstream stream{std::string(yaml)};
  std::string line;
  bool saw_header = false;
  bool saw_body = false;
  std::optional<std::string> name;

  while (std::getline(stream, line)) {
    line = Trim(line);
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

std::vector<std::string> ReadImportedDictManifest(
    const std::filesystem::path& user_data_dir) {
  std::vector<std::string> names;
  const auto manifest = ReadFileUtf8(user_data_dir / L"fp_user_dicts.txt");
  if (!manifest) {
    return names;
  }

  std::istringstream stream{*manifest};
  std::string line;
  while (std::getline(stream, line)) {
    line = Trim(line);
    if (!line.empty() && !line.starts_with("#")) {
      names.push_back(line);
    }
  }
  return names;
}

void AddUnique(std::vector<std::string>& names, const std::string& imported_dict_name) {
  for (const auto& name : names) {
    if (name == imported_dict_name) {
      return;
    }
  }
  names.push_back(imported_dict_name);
}

bool WriteTextFile(const std::filesystem::path& path, std::string_view content) {
  fp::EnsureDirectory(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  return file.good();
}

bool WriteIntegrationFiles(const std::filesystem::path& user_data_dir,
                           const std::vector<std::string>& imported_dict_names) {
  std::ostringstream manifest;
  manifest << "# FluentPinyin imported Rime dictionaries.\n";
  for (const auto& name : imported_dict_names) {
    manifest << name << "\n";
  }
  if (!WriteTextFile(user_data_dir / L"fp_user_dicts.txt", manifest.str())) {
    return false;
  }

  const auto aggregate_dict = user_data_dir / L"fp_frost.dict.yaml";
  std::ostringstream aggregate;
  aggregate << "# encoding: utf-8\n"
               "---\n"
               "name: fp_frost\n"
               "version: \"1\"\n"
               "sort: by_weight\n"
               "import_tables:\n"
               "  - rime_frost\n";
  for (const auto& name : imported_dict_names) {
    aggregate << "  - " << name << "\n";
  }
  aggregate << "...\n";
  if (!WriteTextFile(aggregate_dict, aggregate.str())) {
    return false;
  }

  const auto custom = user_data_dir / L"rime_frost.custom.yaml";
  const std::string patch =
      "patch:\n"
      "  schema/name: \"\\u6d41\\u7545\\u62fc\\u97f3\"\n"
      "  translator/dictionary: fp_frost\n";
  return WriteTextFile(custom, patch);
}

void PrintUsage() {
  std::wcout << L"Usage: fp-lexicon-import <rime-dict-yaml> [rime-user-dir]\n";
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc < 2 || argc > 3) {
    PrintUsage();
    return 1;
  }

  const std::filesystem::path input_path = argv[1];
  const std::filesystem::path user_data_dir =
      argc == 3 ? std::filesystem::path(argv[2])
                : fp::GetFpRoamingDataPath() / L"Rime";

  const auto yaml = ReadFileUtf8(input_path);
  if (!yaml) {
    std::wcerr << L"Failed to read " << input_path.wstring() << L"\n";
    return 2;
  }

  const auto dict_name = ParseRimeDictName(*yaml);
  if (!dict_name) {
    std::wcerr << L"Not a supported Rime .dict.yaml file: " << input_path.wstring()
               << L"\n";
    return 3;
  }
  if (!IsSafeRimeDictName(*dict_name)) {
    std::wcerr << L"Unsupported Rime dictionary name: " << Utf8ToWide(*dict_name)
               << L"\n";
    return 3;
  }

  if (!fp::EnsureDirectory(user_data_dir)) {
    std::wcerr << L"Failed to create " << user_data_dir.wstring() << L"\n";
    return 4;
  }

  const auto target_path = user_data_dir / (Utf8ToWide(*dict_name) + L".dict.yaml");
  if (!WriteTextFile(target_path, *yaml)) {
    std::wcerr << L"Failed to write " << target_path.wstring() << L"\n";
    return 5;
  }

  auto imported_dict_names = ReadImportedDictManifest(user_data_dir);
  AddUnique(imported_dict_names, *dict_name);

  if (!WriteIntegrationFiles(user_data_dir, imported_dict_names)) {
    std::wcerr << L"Failed to write Frost integration files.\n";
    return 6;
  }

  std::wcout << L"Imported Rime dictionary " << Utf8ToWide(*dict_name) << L" to "
             << target_path.wstring() << L"\n";
  std::wcout << L"Redeploy Rime to apply the dictionary.\n";
  return 0;
}
