#include "tsf/font_model.h"

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
  Expect(std::wstring_view(fp::tsf::UiFontFamily(false)) == L"MiSans",
         "simplified UI font is MiSans");
  Expect(std::wstring_view(fp::tsf::UiFontFamily(true)) == L"MiSans TC",
         "traditional UI font is MiSans TC");
  Expect(std::wstring_view(fp::tsf::ToolbarTextFontFamily()) == L"MiSans",
         "toolbar text font is MiSans");
  Expect(fp::tsf::IsBaseUiFontFamily(nullptr), "null font family uses base font set");
  Expect(fp::tsf::IsBaseUiFontFamily(L"MiSans TC"), "MiSans TC is a base font");
  Expect(!fp::tsf::IsBaseUiFontFamily(L"Source Han Sans SC"),
         "Source Han requires candidate fallback font set");

  const auto mi_fallbacks =
      fp::tsf::CandidateUiFontFallbackFamilies(fp::tsf::CandidateFontFamily::kMiSans, false);
  Expect(std::wstring_view(mi_fallbacks[0]) == L"MiSans",
         "MiSans candidate fallback starts with MiSans");
  Expect(mi_fallbacks[3] == nullptr, "MiSans candidate fallback has null tail");

  const auto source_fallbacks = fp::tsf::CandidateUiFontFallbackFamilies(
      fp::tsf::CandidateFontFamily::kSourceHanSans,
      false);
  Expect(std::wstring_view(source_fallbacks[0]) == L"Source Han Sans SC",
         "Source Han candidate fallback starts with Source Han SC");
  Expect(std::wstring_view(source_fallbacks[3]) == L"MiSans L3",
         "Source Han candidate fallback keeps MiSans L3 before base UI font");

  Expect(fp::tsf::TextFaceMatchesFamilyName(L"MiSans Regular", L"MiSans"),
         "font family name matches style suffix");
  Expect(fp::tsf::TextFaceMatchesFamily(L"\u601D\u6E90\u9ED1\u4F53", L"Source Han Sans SC"),
         "Source Han SC matches Chinese alias");
  Expect(fp::tsf::TextFaceMatchesFamily(L"\u904D\u9ED1\u9AD4P2", L"Plangothic P2"),
         "Plangothic P2 matches traditional alias");
  Expect(fp::tsf::CandidateFontFamilyForFace(L"\u904D\u9ED1\u4F53P1") ==
             fp::tsf::CandidateFontFamily::kSourceHanSans,
         "Plangothic face selects Source Han candidate family");
  Expect(fp::tsf::CandidateFontFamilyForFace(L"MiSans") ==
             fp::tsf::CandidateFontFamily::kMiSans,
         "MiSans face selects MiSans candidate family");
  Expect(fp::tsf::IsSongtiFace(L"SimSun"), "SimSun is treated as Songti");
  Expect(fp::tsf::IsSongtiFace(L"\u65B0\u5B8B\u4F53"),
         "Chinese NSimSun alias is treated as Songti");
  Expect(!fp::tsf::IsSongtiFace(L"MiSans"), "MiSans is not Songti");

  return g_failures == 0 ? 0 : 1;
}
