#include "core/rime_engine.h"

#include <windows.h>

#include <iostream>
#include <string>

namespace {

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

void PrintLine(const std::wstring& value) {
  std::cout << WideToUtf8(value) << "\n";
}

void PrintUsage() {
  std::cout << "Usage: fp-rime-smoke [input]\n";
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc > 2) {
    PrintUsage();
    return 1;
  }

  std::string input = "nihao";
  if (argc == 2) {
    const int required =
        WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
      std::wcerr << L"Failed to convert input to UTF-8.\n";
      return 1;
    }

    input.resize(static_cast<size_t>(required - 1));
    WideCharToMultiByte(
        CP_UTF8, 0, argv[1], -1, input.data(), required, nullptr, nullptr);
  }

  fp::core::RimeEngine engine;
  const auto status = engine.Initialize();
  if (!status.initialized) {
    std::cerr << "Rime init failed: " << WideToUtf8(status.message) << "\n";
    return 1;
  }

  std::cout << "Rime version: " << engine.rime_version() << "\n";
  PrintLine(L"Shared data: " + engine.shared_data_dir().wstring());
  PrintLine(L"User data: " + engine.user_data_dir().wstring());

  const auto candidates = engine.GetCandidatesForInput(input);
  std::cout << "Input: " << input << "\n";
  if (candidates.empty()) {
    std::cerr << "No candidates returned.\n";
    return 2;
  }

  for (size_t index = 0; index < candidates.size(); ++index) {
    std::wstring line = std::to_wstring(index + 1) + L". " + candidates[index].text;
    if (!candidates[index].comment.empty()) {
      line += L" [" + candidates[index].comment + L"]";
    }
    PrintLine(line);
  }

  return 0;
}
