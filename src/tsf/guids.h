#pragma once

#include <windows.h>

#include <guiddef.h>

namespace fp::tsf {

inline constexpr CLSID kTextServiceClsid = {
    0xa34e3c15,
    0x1c4a,
    0x43ce,
    {0x92, 0xe4, 0x3c, 0xb5, 0xd3, 0x3c, 0x59, 0x10}};

inline constexpr GUID kProfileGuid = {
    0xd95a5d5b,
    0xb7a1,
    0x4e96,
    {0x9f, 0x2e, 0x82, 0xce, 0x1e, 0xf7, 0xdd, 0x5e}};

inline constexpr wchar_t kProfileDescription[] = L"\u6D41\u7545\u62FC\u97F3";
inline constexpr wchar_t kLanguageListLabel[] = L"\u7545";
inline constexpr LANGID kLanguageId = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);

}  // namespace fp::tsf
