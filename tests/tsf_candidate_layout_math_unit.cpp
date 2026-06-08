#include "tsf/candidate_layout_math.h"

#include "common/constants.h"

#include <iostream>

namespace {

int g_failures = 0;

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

}  // namespace

int main() {
  Expect(fp::tsf::CompactCandidateCount(0) == fp::kMinCandidateCount,
         "compact count clamps low values");
  Expect(fp::tsf::CompactCandidateCount(99) == fp::kMaxCandidateCount,
         "compact count clamps high values");
  Expect(fp::tsf::ExpandedCandidateColumnCount(true, 5) == 5,
         "horizontal expanded columns follow compact count");
  Expect(fp::tsf::ExpandedCandidateColumnCount(false, 5) == 4,
         "vertical expanded columns are fixed");
  Expect(fp::tsf::ExpandedCandidateRowCount(true) == 4,
         "horizontal expanded row count includes tail rows");
  Expect(fp::tsf::ExpandedCandidateRowCount(false) == 9,
         "vertical expanded row count is fixed");
  Expect(fp::tsf::ExpandedCandidatePageSize(true, 5) ==
             5 + fp::kMaxCandidateCount * 3,
         "expanded page size preserves existing horizontal sizing");
  Expect(fp::tsf::CandidateItemHeightDips(33, fp::kBaseCandidateFontPointSize) == 33,
         "base item height is unchanged");
  Expect(fp::tsf::CandidateItemHeightDips(33, fp::kBaseCandidateFontPointSize + 2) == 37,
         "larger candidate font increases item height");
  Expect(fp::tsf::CandidateRowStepDips(34, fp::kBaseCandidateFontPointSize + 1) == 36,
         "larger candidate font increases row step");
  Expect(fp::tsf::ScaleCandidateSelectionMark(10, 33, 96) == 10,
         "selection mark keeps base scale at 96 dpi");

  return g_failures == 0 ? 0 : 1;
}
