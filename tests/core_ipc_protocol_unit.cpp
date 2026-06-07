#include "common/core_ipc_protocol.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace {

int g_failures = 0;

void Expect(bool condition, std::string_view message) {
  if (condition) {
    return;
  }
  ++g_failures;
  std::cerr << "FAIL: " << message << "\n";
}

void TestCandidatePageRoundTrip() {
  fp::core::RimeCandidatePage page;
  page.composition = L"ni hao";
  page.has_previous_page = true;
  page.has_next_page = false;
  page.candidates.push_back({L"你好", L"comment\nwith separator"});
  page.candidates.push_back({L"拟好", L"emoji \U0001F680"});

  const std::string payload = fp::coreipc::EncodeCandidatePageResponse(page);
  fp::core::RimeCandidatePage decoded;
  Expect(fp::coreipc::DecodeCandidatePageResponse(payload, &decoded),
         "candidate page response decodes");
  Expect(decoded.composition == page.composition, "composition round trips");
  Expect(decoded.has_previous_page == page.has_previous_page, "previous-page flag round trips");
  Expect(decoded.has_next_page == page.has_next_page, "next-page flag round trips");
  Expect(decoded.candidates.size() == page.candidates.size(), "candidate count round trips");
  if (decoded.candidates.size() == page.candidates.size()) {
    Expect(decoded.candidates[0].text == page.candidates[0].text, "candidate text round trips");
    Expect(decoded.candidates[0].comment == page.candidates[0].comment,
           "candidate comments can contain newlines");
    Expect(decoded.candidates[1].comment == page.candidates[1].comment,
           "candidate comments can contain non-BMP Unicode");
  }
}

void TestCommandRoundTrip() {
  const std::string request =
      fp::coreipc::EncodeSelectCandidateRequest("abc+def", 2, 36, 7);
  fp::coreipc::Command command{};
  std::vector<std::string> fields;
  Expect(fp::coreipc::DecodeCommand(request, &command, &fields), "command decodes");
  Expect(command == fp::coreipc::Command::kSelectCandidate, "command id round trips");
  Expect(fields.size() == 4, "command field count round trips");
  if (fields.size() == 4) {
    Expect(fields[0] == "abc+def", "input field preserves plus signs");
    Expect(fields[1] == "2", "page index field round trips");
    Expect(fields[2] == "36", "page size field round trips");
    Expect(fields[3] == "7", "candidate index field round trips");
  }
}

}  // namespace

int main() {
  TestCandidatePageRoundTrip();
  TestCommandRoundTrip();
  if (g_failures != 0) {
    std::cerr << g_failures << " core IPC protocol unit test failure(s)\n";
    return 1;
  }
  std::cout << "Core IPC protocol unit tests passed\n";
  return 0;
}
