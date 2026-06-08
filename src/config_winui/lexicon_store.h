#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fp::config_winui {

inline constexpr std::wstring_view kDefaultManagedUserLexiconName = L"fp_user_words";

struct ManagedDictionaryEntry {
  std::string name;
  bool enabled = true;
};

struct PhraseEntry {
  std::wstring phrase;
  std::wstring code;
  std::wstring weight = L"5";
  bool removed = false;
};

std::wstring TrimLexiconText(std::wstring_view value);
std::string TrimUtf8(std::string value);
std::optional<std::string> ReadFileUtf8(const std::filesystem::path& path);
bool WriteFileUtf8(const std::filesystem::path& path, std::string_view content);
std::string ParseYamlScalar(std::string value);
std::optional<std::string> ParseRimeDictName(std::string_view yaml);
bool IsSafeRimeDictName(std::string_view name);
std::wstring FileStemDisplayName(const std::filesystem::path& path);
std::wstring LexiconCountText(int count);

bool ManagedDictionaryContains(const std::vector<ManagedDictionaryEntry>& entries,
                               std::string_view name);
std::vector<ManagedDictionaryEntry> ReadManagedDictionaryManifest();
std::vector<std::string> EnabledManagedDictionaryNames(
    const std::vector<ManagedDictionaryEntry>& entries);

std::wstring NormalizePhraseWeight(std::wstring_view value);
std::vector<PhraseEntry> ParseTabSeparatedLexiconLines(std::wstring_view value);
std::vector<PhraseEntry> ReadUserLexiconEntries();
bool WriteUserLexiconEntries(const std::vector<PhraseEntry>& entries);
bool UserLexiconHasEntries();
bool WriteManagedDictionaryIntegrationFiles(
    const std::vector<ManagedDictionaryEntry>& entries,
    bool user_lexicon_enabled,
    bool imported_lexicons_enabled);
bool WriteManagedDictionaryIntegrationFiles(
    const std::vector<ManagedDictionaryEntry>& entries);
bool ImportManagedDictionary(const std::filesystem::path& source_path,
                             std::wstring* error_message);
bool RemoveManagedDictionaryFile(std::string_view name);
std::vector<PhraseEntry> ReadCustomPhraseEntries();
bool WriteCustomPhraseEntries(const std::vector<PhraseEntry>& entries);
int CurrentCustomPhraseCount();
int CurrentUserLexiconCount();
int CurrentManagedDictionaryCount(bool enabled_only);

}  // namespace fp::config_winui
