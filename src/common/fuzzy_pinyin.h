#pragma once

#include <algorithm>
#include <array>
#include <string_view>

namespace fp {

inline constexpr std::array<std::wstring_view, 10> kFuzzyPinyinRuleIds{{
    L"nl",
    L"ry",
    L"hf",
    L"rl",
    L"kg",
    L"en_eng",
    L"in_ing",
    L"c_ch",
    L"z_zh",
    L"s_sh",
}};

inline bool IsKnownFuzzyPinyinRuleId(std::wstring_view id) {
  return std::find(kFuzzyPinyinRuleIds.begin(), kFuzzyPinyinRuleIds.end(), id) !=
         kFuzzyPinyinRuleIds.end();
}

}  // namespace fp
