#pragma once

#include <array>
#include <string_view>

namespace fp::tsf {

enum class CandidateFontFamily {
  kMiSans,
  kSourceHanSans,
};

const wchar_t* UiFontFamily(bool traditional = false);
const wchar_t* ToolbarTextFontFamily();
std::array<const wchar_t*, 4> UiFontFallbackFamilies(bool traditional);
CandidateFontFamily CandidateFontFamilyFromSetting(std::wstring_view value);
const wchar_t* CandidateUiFontFamily(CandidateFontFamily family, bool traditional);
std::array<const wchar_t*, 5> CandidateUiFontFallbackFamilies(CandidateFontFamily family,
                                                              bool traditional);
bool IsBaseUiFontFamily(const wchar_t* family);
bool TextFaceMatchesFamilyName(std::wstring_view face, std::wstring_view family);
bool TextFaceMatchesFamily(std::wstring_view face, std::wstring_view family);
CandidateFontFamily CandidateFontFamilyForFace(std::wstring_view face);

}  // namespace fp::tsf
