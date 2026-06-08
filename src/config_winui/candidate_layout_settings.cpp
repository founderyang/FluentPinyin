#include "config_winui/candidate_layout_settings.h"

#include "common/constants.h"
#include "config_winui/settings_binding.h"

namespace fp::config_winui {

std::wstring ReadCandidateLayoutSetting(std::wstring_view default_value) {
  const std::wstring value = ReadStringSetting(fp::kCandidateLayoutSetting);
  const std::wstring_view view(value);
  if (view == fp::kCandidateLayoutHorizontal ||
      view == fp::kCandidateLayoutVertical) {
    return value;
  }
  return ReadBoolSetting(fp::kCandidateHorizontalSetting,
                         default_value != fp::kCandidateLayoutVertical)
             ? std::wstring(fp::kCandidateLayoutHorizontal)
             : std::wstring(fp::kCandidateLayoutVertical);
}

bool WriteCandidateLayoutSetting(std::wstring_view value) {
  const bool horizontal = value != fp::kCandidateLayoutVertical;
  bool changed = WriteStringSetting(fp::kCandidateLayoutSetting,
                                    horizontal ? fp::kCandidateLayoutHorizontal
                                               : fp::kCandidateLayoutVertical);
  changed = WriteBoolSetting(fp::kCandidateHorizontalSetting, horizontal) || changed;
  return changed;
}

}  // namespace fp::config_winui
