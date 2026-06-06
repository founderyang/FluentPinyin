#include "common/encoding.h"

#include <windows.h>

namespace fp {

std::string WideToUtf8(std::wstring_view value) {
  if (value.empty()) {
    return {};
  }

  const int required = WideCharToMultiByte(CP_UTF8,
                                           0,
                                           value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr);
  if (required <= 0) {
    return {};
  }

  std::string result(static_cast<size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8,
                      0,
                      value.data(),
                      static_cast<int>(value.size()),
                      result.data(),
                      required,
                      nullptr,
                      nullptr);
  return result;
}

std::wstring Utf8ToWideWithFlags(std::string_view value, DWORD flags) {
  if (value.empty()) {
    return {};
  }

  const int required = MultiByteToWideChar(CP_UTF8,
                                           flags,
                                           value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr,
                                           0);
  if (required <= 0) {
    return {};
  }

  std::wstring result(static_cast<size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8,
                      flags,
                      value.data(),
                      static_cast<int>(value.size()),
                      result.data(),
                      required);
  return result;
}

std::wstring Utf8ToWide(std::string_view value) {
  return Utf8ToWideWithFlags(value, 0);
}

std::wstring Utf8ToWideStrict(std::string_view value) {
  return Utf8ToWideWithFlags(value, MB_ERR_INVALID_CHARS);
}

}  // namespace fp
