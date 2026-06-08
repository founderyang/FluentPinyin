#include "tsf/font_model.h"

#include "common/candidate_font.h"

#include <cwctype>

namespace fp::tsf {

const wchar_t* UiFontFamily(bool traditional) {
  return traditional ? L"MiSans TC" : L"MiSans";
}

const wchar_t* ToolbarTextFontFamily() {
  return UiFontFamily(false);
}

std::array<const wchar_t*, 4> UiFontFallbackFamilies(bool traditional) {
  return traditional
             ? std::array<const wchar_t*, 4>{L"MiSans TC", L"MiSans L3", L"MiSans", nullptr}
             : std::array<const wchar_t*, 4>{L"MiSans", L"MiSans TC", L"MiSans L3", nullptr};
}

CandidateFontFamily CandidateFontFamilyFromSetting(std::wstring_view value) {
  return fp::IsSourceHanSansCandidateFontFamily(value)
             ? CandidateFontFamily::kSourceHanSans
             : CandidateFontFamily::kMiSans;
}

const wchar_t* CandidateUiFontFamily(CandidateFontFamily family, bool traditional) {
  if (family == CandidateFontFamily::kSourceHanSans) {
    return traditional ? L"Source Han Sans TC" : L"Source Han Sans SC";
  }
  return UiFontFamily(traditional);
}

std::array<const wchar_t*, 5> CandidateUiFontFallbackFamilies(CandidateFontFamily family,
                                                              bool traditional) {
  if (family == CandidateFontFamily::kSourceHanSans) {
    return {CandidateUiFontFamily(family, traditional),
            L"Plangothic P1",
            L"Plangothic P2",
            L"MiSans L3",
            UiFontFamily(traditional)};
  }
  const auto ui_fallback = UiFontFallbackFamilies(traditional);
  return {ui_fallback[0], ui_fallback[1], ui_fallback[2], nullptr, nullptr};
}

bool IsBaseUiFontFamily(const wchar_t* family) {
  if (family == nullptr) {
    return true;
  }
  const std::wstring_view name(family);
  return name == L"MiSans" || name == L"MiSans TC";
}

bool TextFaceMatchesFamilyName(std::wstring_view face, std::wstring_view family) {
  if (face == family) {
    return true;
  }
  return face.size() > family.size() && face.substr(0, family.size()) == family &&
         std::iswspace(face[family.size()]) != 0;
}

bool TextFaceMatchesFamily(std::wstring_view face, std::wstring_view family) {
  if (TextFaceMatchesFamilyName(face, family)) {
    return true;
  }
  if (family == L"Source Han Sans SC") {
    return TextFaceMatchesFamilyName(face, L"\u601D\u6E90\u9ED1\u4F53");
  }
  if (family == L"Source Han Sans TC") {
    return TextFaceMatchesFamilyName(face, L"\u601D\u6E90\u9ED1\u9AD4");
  }
  if (family == L"Plangothic P1") {
    return TextFaceMatchesFamilyName(face, L"\u904D\u9ED1\u4F53P1") ||
           TextFaceMatchesFamilyName(face, L"\u904D\u9ED1\u9AD4P1");
  }
  if (family == L"Plangothic P2") {
    return TextFaceMatchesFamilyName(face, L"\u904D\u9ED1\u4F53P2") ||
           TextFaceMatchesFamilyName(face, L"\u904D\u9ED1\u9AD4P2");
  }
  return false;
}

CandidateFontFamily CandidateFontFamilyForFace(std::wstring_view face) {
  return TextFaceMatchesFamily(face, L"Source Han Sans SC") ||
                 TextFaceMatchesFamily(face, L"Source Han Sans TC") ||
                 TextFaceMatchesFamily(face, L"\u601D\u6E90\u9ED1\u4F53") ||
                 TextFaceMatchesFamily(face, L"\u601D\u6E90\u9ED1\u9AD4") ||
                 TextFaceMatchesFamily(face, L"Plangothic P1") ||
                 TextFaceMatchesFamily(face, L"Plangothic P2") ||
                 TextFaceMatchesFamily(face, L"\u904D\u9ED1\u4F53P1") ||
                 TextFaceMatchesFamily(face, L"\u904D\u9ED1\u4F53P2") ||
                 TextFaceMatchesFamily(face, L"\u904D\u9ED1\u9AD4P1") ||
                 TextFaceMatchesFamily(face, L"\u904D\u9ED1\u9AD4P2")
             ? CandidateFontFamily::kSourceHanSans
             : CandidateFontFamily::kMiSans;
}

bool IsSongtiFace(std::wstring_view face) {
  return TextFaceMatchesFamilyName(face, L"SimSun") ||
         TextFaceMatchesFamilyName(face, L"NSimSun") ||
         TextFaceMatchesFamilyName(face, L"\u5B8B\u4F53") ||
         TextFaceMatchesFamilyName(face, L"\u65B0\u5B8B\u4F53");
}

}  // namespace fp::tsf
