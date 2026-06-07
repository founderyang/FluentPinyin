#include "config_winui/lexicon_store.h"

#include "config_winui/app_paths.h"

#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string_view>
#include <system_error>

namespace {

int g_failures = 0;
std::filesystem::path g_isolated_appdata_root;

void Expect(bool condition, std::string_view message) {
  if (condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAIL: " << message << "\n";
}

void UseIsolatedAppData() {
  g_isolated_appdata_root = std::filesystem::current_path() /
                            (L"config-lexicon-store-appdata-" +
                             std::to_wstring(GetCurrentProcessId()));
  const auto local = g_isolated_appdata_root / L"Local";
  const auto roaming = g_isolated_appdata_root / L"Roaming";
  const auto program_data = g_isolated_appdata_root / L"ProgramData";
  std::error_code error;
  std::filesystem::remove_all(g_isolated_appdata_root, error);
  std::filesystem::create_directories(local, error);
  std::filesystem::create_directories(roaming, error);
  std::filesystem::create_directories(program_data, error);
  SetEnvironmentVariableW(L"LOCALAPPDATA", local.c_str());
  SetEnvironmentVariableW(L"APPDATA", roaming.c_str());
  SetEnvironmentVariableW(L"PROGRAMDATA", program_data.c_str());
}

void TestParsingHelpers() {
  namespace lexicon = fp::config_winui;

  Expect(lexicon::TrimLexiconText(L"  abc \t") == L"abc", "trims wide text");
  Expect(lexicon::TrimUtf8(" \r\nabc\t") == "abc", "trims utf-8 bytes");
  Expect(lexicon::ParseYamlScalar(" \"dict\" # comment") == "dict",
         "parses quoted yaml scalars");
  Expect(lexicon::ParseRimeDictName("---\nname: sample_dict\n...\nbody") ==
             std::optional<std::string>("sample_dict"),
         "parses rime dictionary name from header");
  Expect(!lexicon::ParseRimeDictName("name: sample_dict\n"),
         "rejects dictionaries without rime header/body markers");
  Expect(lexicon::IsSafeRimeDictName("domain/example-1"),
         "accepts safe rime dictionary names");
  Expect(!lexicon::IsSafeRimeDictName("../escape"),
         "rejects path traversal dictionary names");
  Expect(!lexicon::IsSafeRimeDictName("C:\\escape"),
         "rejects absolute Windows paths");
}

void TestPhraseEntries() {
  namespace lexicon = fp::config_winui;

  const auto entries =
      lexicon::ParseTabSeparatedLexiconLines(L"# comment\n  你好 \t nihao \t 8\r\nbad\n词\tci\tx\n");
  Expect(entries.size() == 2, "parses valid tab separated phrase rows");
  Expect(entries[0].phrase == L"你好" && entries[0].code == L"nihao" &&
             entries[0].weight == L"8",
         "trims phrase rows and keeps numeric weights");
  Expect(entries[1].weight == L"5", "normalizes invalid phrase weights");
}

void TestLexiconFiles() {
  namespace lexicon = fp::config_winui;
  const auto user_data = lexicon::RimeUserDataPath();

  const auto bom_file = user_data / L"bom.txt";
  Expect(lexicon::WriteFileUtf8(bom_file, "\xEF\xBB\xBFhello"),
         "writes utf-8 files");
  Expect(lexicon::ReadFileUtf8(bom_file) == std::optional<std::string>("hello"),
         "strips UTF-8 BOM when reading files");

  std::vector<lexicon::PhraseEntry> phrases{
      {L"  你好 ", L" nihao ", L"9", false},
      {L"跳过", L"tiaoguo", L"5", true},
  };
  Expect(lexicon::WriteUserLexiconEntries(phrases), "writes managed user lexicon");
  Expect(lexicon::CurrentUserLexiconCount() == 1, "reads managed user lexicon count");
  Expect(lexicon::WriteCustomPhraseEntries(phrases), "writes custom phrase file");
  Expect(lexicon::CurrentCustomPhraseCount() == 1, "reads custom phrase count");

  std::vector<lexicon::ManagedDictionaryEntry> dictionaries{
      {"sample_dict", true},
      {"disabled_dict", false},
      {"../bad", true},
  };
  Expect(lexicon::WriteManagedDictionaryIntegrationFiles(dictionaries, true, true),
         "writes managed dictionary manifest and aggregate dictionaries");
  Expect(lexicon::CurrentManagedDictionaryCount(false) == 2,
         "manifest ignores unsafe dictionary names");
  Expect(lexicon::CurrentManagedDictionaryCount(true) == 1,
         "counts enabled imported dictionaries");
  const auto aggregate = lexicon::ReadFileUtf8(user_data / L"fp_wanxiang.dict.yaml");
  Expect(aggregate && aggregate->find("fp_user_words") != std::string::npos &&
             aggregate->find("sample_dict") != std::string::npos &&
             aggregate->find("disabled_dict") == std::string::npos,
         "aggregate dictionary includes enabled user/imported dictionaries only");

  Expect(lexicon::WriteManagedDictionaryIntegrationFiles(dictionaries, false, false),
         "rewrites integration files with all lexicons disabled");
  Expect(!std::filesystem::exists(user_data / L"fp_wanxiang.dict.yaml"),
         "removes aggregate dictionaries when no imports are enabled");
}

}  // namespace

int main() {
  UseIsolatedAppData();
  TestParsingHelpers();
  TestPhraseEntries();
  TestLexiconFiles();
  std::error_code cleanup_error;
  std::filesystem::remove_all(g_isolated_appdata_root, cleanup_error);
  if (g_failures != 0) {
    std::cerr << g_failures << " config lexicon store failure(s)\n";
    return 1;
  }
  std::cout << "Config lexicon store tests passed\n";
  return 0;
}
