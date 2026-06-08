#pragma once

#include "common/constants.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace fp {

inline constexpr std::wstring_view kSourceHanSansCandidateFontFamily =
    L"source_han_sans";
inline constexpr std::wstring_view kLegacyPlangothicCandidateFontFamily =
    L"plangothic";
inline constexpr int kDefaultCandidateCount = 7;
inline constexpr int kMinCandidateCount = 3;
inline constexpr int kMaxCandidateCount = 9;
inline constexpr int kDefaultCandidateFontSizeLevel = 0;
inline constexpr int kMinCandidateFontSizeLevel = 0;
inline constexpr int kMaxCandidateFontSizeLevel = 3;
inline constexpr int kBaseCandidateFontPointSize = 11;
inline constexpr std::array<int, 4> kCandidateFontPointSizes{11, 12, 13, 14};
inline constexpr std::array<std::wstring_view, 4> kCandidateFontSizeLabels{
    L"小",
    L"中",
    L"大",
    L"特大",
};

inline bool IsSourceHanSansCandidateFontFamily(std::wstring_view value) {
  return value == kSourceHanSansCandidateFontFamily ||
         value == kLegacyPlangothicCandidateFontFamily;
}

inline int ClampCandidateCount(int count) noexcept {
  return std::clamp(count, kMinCandidateCount, kMaxCandidateCount);
}

inline std::wstring NormalizeCandidateFontFamilySetting(std::wstring_view value) {
  return IsSourceHanSansCandidateFontFamily(value)
             ? std::wstring(kSourceHanSansCandidateFontFamily)
             : std::wstring(kDefaultCandidateFontFamily);
}

inline int ClampCandidateFontSizeLevel(int level) noexcept {
  return std::clamp(level, kMinCandidateFontSizeLevel, kMaxCandidateFontSizeLevel);
}

inline int CandidateFontPointSizeForLevel(int level) noexcept {
  return kCandidateFontPointSizes[static_cast<size_t>(ClampCandidateFontSizeLevel(level))];
}

}  // namespace fp
