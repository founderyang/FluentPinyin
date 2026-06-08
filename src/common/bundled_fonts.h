#pragma once

#include <array>
#include <string>
#include <string_view>

namespace fp {

inline constexpr std::wstring_view kSettingsUiFontFamily = L"MiSans";

struct BundledFontEntry {
  std::wstring_view file;
  std::wstring_view registry_name;
};

inline constexpr std::wstring_view kTrueTypeFontRegistryKind = L"TrueType";
inline constexpr std::wstring_view kOpenTypeFontRegistryKind = L"OpenType";

inline constexpr std::array<std::wstring_view, 6> kBundledBaseUiFontFiles{
    L"MiSans-Regular.ttf",
    L"MiSans-Medium.ttf",
    L"MiSans-Semibold.ttf",
    L"MiSansTC-Regular.ttf",
    L"MiSansTC-Medium.ttf",
    L"MiSansTC-Semibold.ttf",
};

inline constexpr std::array<std::wstring_view, 5> kBundledFallbackUiFontFiles{
    L"MiSansL3-Regular.ttf",
    L"SourceHanSansSC-Regular.otf",
    L"SourceHanSansTC-Regular.otf",
    L"PlangothicP1-Regular.ttf",
    L"PlangothicP2-Regular.ttf",
};

inline constexpr std::array<BundledFontEntry, 11> kBundledFontEntries{
    BundledFontEntry{L"MiSans-Regular.ttf", L"MiSans"},
    BundledFontEntry{L"MiSans-Medium.ttf", L"MiSans Medium"},
    BundledFontEntry{L"MiSans-Semibold.ttf", L"MiSans Semibold"},
    BundledFontEntry{L"MiSansTC-Regular.ttf", L"MiSans TC"},
    BundledFontEntry{L"MiSansTC-Medium.ttf", L"MiSans TC Medium"},
    BundledFontEntry{L"MiSansTC-Semibold.ttf", L"MiSans TC Semibold"},
    BundledFontEntry{L"MiSansL3-Regular.ttf", L"MiSans L3"},
    BundledFontEntry{L"SourceHanSansSC-Regular.otf", L"Source Han Sans SC"},
    BundledFontEntry{L"SourceHanSansTC-Regular.otf", L"Source Han Sans TC"},
    BundledFontEntry{L"PlangothicP1-Regular.ttf", L"Plangothic P1"},
    BundledFontEntry{L"PlangothicP2-Regular.ttf", L"Plangothic P2"},
};

inline std::wstring_view BundledFontRegistryKind(std::wstring_view file) {
  return file.ends_with(L".otf") ? kOpenTypeFontRegistryKind
                                 : kTrueTypeFontRegistryKind;
}

inline std::wstring BundledFontRegistryValueName(const BundledFontEntry& font,
                                                 std::wstring_view kind) {
  std::wstring name(font.registry_name);
  name.append(L" (");
  name.append(kind);
  name.push_back(L')');
  return name;
}

}  // namespace fp
