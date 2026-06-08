#include "devtools/registry_utils.h"

#include "common/bundled_fonts.h"
#include "common/encoding.h"

#include <algorithm>
#include <cwchar>

namespace fp::devtools {
namespace {

constexpr wchar_t kUninstallRegistryRoot[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

bool ReadRegistryString(HKEY root,
                        std::wstring_view subkey,
                        std::wstring_view name,
                        std::wstring* value) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(root, std::wstring(subkey).c_str(), 0, KEY_QUERY_VALUE, &key) !=
      ERROR_SUCCESS) {
    return false;
  }

  DWORD type = 0;
  DWORD bytes = 0;
  LSTATUS status =
      RegQueryValueExW(key, std::wstring(name).c_str(), nullptr, &type, nullptr, &bytes);
  if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0) {
    RegCloseKey(key);
    return false;
  }

  std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
  status = RegQueryValueExW(key,
                            std::wstring(name).c_str(),
                            nullptr,
                            &type,
                            reinterpret_cast<BYTE*>(buffer.data()),
                            &bytes);
  RegCloseKey(key);
  if (status != ERROR_SUCCESS) {
    return false;
  }
  buffer.resize(wcsnlen_s(buffer.c_str(), buffer.size()));
  *value = buffer;
  return true;
}

std::wstring RegistryDataToString(DWORD type, const std::vector<BYTE>& data) {
  if (data.empty()) {
    return {};
  }

  if (type == REG_SZ || type == REG_EXPAND_SZ) {
    const auto* text = reinterpret_cast<const wchar_t*>(data.data());
    const size_t chars = data.size() / sizeof(wchar_t);
    return std::wstring(text, wcsnlen_s(text, chars));
  }
  if (type == REG_MULTI_SZ) {
    std::wstring result;
    const auto* text = reinterpret_cast<const wchar_t*>(data.data());
    const size_t chars = data.size() / sizeof(wchar_t);
    size_t index = 0;
    while (index < chars) {
      const size_t start = index;
      while (index < chars && text[index] != L'\0') {
        ++index;
      }
      if (start == index) {
        bool rest_is_empty = true;
        for (size_t rest = index; rest < chars; ++rest) {
          if (text[rest] != L'\0') {
            rest_is_empty = false;
            break;
          }
        }
        if (rest_is_empty) {
          break;
        }
      }
      if (!result.empty()) {
        result.push_back(L'\n');
      }
      result.append(text + start, index - start);
      ++index;
    }
    return result;
  }
  return {};
}

std::vector<std::wstring> ParseMultiStringWithEmptyItems(const std::vector<BYTE>& bytes) {
  std::vector<std::wstring> entries;
  if (bytes.empty()) {
    return entries;
  }

  const auto* text = reinterpret_cast<const wchar_t*>(bytes.data());
  const size_t chars = bytes.size() / sizeof(wchar_t);
  size_t index = 0;
  while (index < chars) {
    const size_t start = index;
    while (index < chars && text[index] != L'\0') {
      ++index;
    }

    if (start == index && entries.size() % 2 == 0) {
      bool rest_is_empty = true;
      for (size_t rest = index; rest < chars; ++rest) {
        if (text[rest] != L'\0') {
          rest_is_empty = false;
          break;
        }
      }
      if (rest_is_empty) {
        break;
      }
    }

    entries.emplace_back(text + start, index - start);
    ++index;
  }
  return entries;
}

}  // namespace

bool SetRegistryStringValue(HKEY root,
                            std::wstring_view subkey,
                            std::wstring_view name,
                            std::wstring_view value) {
  HKEY key = nullptr;
  const LSTATUS status = RegCreateKeyExW(root,
                                         std::wstring(subkey).c_str(),
                                         0,
                                         nullptr,
                                         0,
                                         KEY_SET_VALUE,
                                         nullptr,
                                         &key,
                                         nullptr);
  if (status != ERROR_SUCCESS) {
    return false;
  }
  const std::wstring value_text(value);
  const DWORD byte_size = static_cast<DWORD>((value_text.size() + 1) * sizeof(wchar_t));
  const LSTATUS set_status = RegSetValueExW(key,
                                            std::wstring(name).c_str(),
                                            0,
                                            REG_SZ,
                                            reinterpret_cast<const BYTE*>(value_text.c_str()),
                                            byte_size);
  RegCloseKey(key);
  return set_status == ERROR_SUCCESS;
}

bool DeleteRegistryValue(HKEY root, std::wstring_view subkey, std::wstring_view name) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(root, std::wstring(subkey).c_str(), 0, KEY_SET_VALUE, &key) !=
      ERROR_SUCCESS) {
    return false;
  }
  const bool deleted = RegDeleteValueW(key, std::wstring(name).c_str()) == ERROR_SUCCESS;
  RegCloseKey(key);
  return deleted;
}

void DeleteRegistryTree(HKEY root, std::wstring_view subkey) {
  RegDeleteTreeW(root, std::wstring(subkey).c_str());
}

std::vector<std::wstring> FindUninstallKeysByName(std::wstring_view display_name) {
  std::vector<std::wstring> matches;
  HKEY root_key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    kUninstallRegistryRoot,
                    0,
                    KEY_ENUMERATE_SUB_KEYS,
                    &root_key) != ERROR_SUCCESS) {
    return matches;
  }

  DWORD index = 0;
  for (;;) {
    DWORD name_chars = 256;
    std::wstring child(name_chars, L'\0');
    const LSTATUS status = RegEnumKeyExW(
        root_key, index, child.data(), &name_chars, nullptr, nullptr, nullptr, nullptr);
    if (status == ERROR_NO_MORE_ITEMS) {
      break;
    }
    if (status != ERROR_SUCCESS) {
      ++index;
      continue;
    }
    child.resize(name_chars);
    const std::wstring subkey =
        std::wstring(kUninstallRegistryRoot) + L"\\" + child;
    std::wstring value;
    if (ReadRegistryString(HKEY_LOCAL_MACHINE, subkey, L"DisplayName", &value) &&
        value == display_name) {
      matches.push_back(subkey);
    }
    ++index;
  }

  RegCloseKey(root_key);
  return matches;
}

void RemoveRegistryValuesMatching(
    HKEY root,
    std::wstring_view subkey,
    std::initializer_list<std::wstring_view> name_needles,
    std::initializer_list<std::wstring_view> value_needles) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(root,
                    std::wstring(subkey).c_str(),
                    0,
                    KEY_READ | KEY_SET_VALUE,
                    &key) != ERROR_SUCCESS) {
    return;
  }

  DWORD index = 0;
  std::vector<std::wstring> names_to_delete;
  for (;;) {
    DWORD name_chars = 512;
    std::wstring name(name_chars, L'\0');
    DWORD type = 0;
    DWORD data_bytes = 4096;
    std::vector<BYTE> data(data_bytes);
    LSTATUS status = RegEnumValueW(key,
                                   index,
                                   name.data(),
                                   &name_chars,
                                   nullptr,
                                   &type,
                                   data.data(),
                                   &data_bytes);
    if (status == ERROR_MORE_DATA) {
      name_chars = 32767;
      name.assign(name_chars, L'\0');
      data.assign(data_bytes, 0);
      status = RegEnumValueW(key,
                             index,
                             name.data(),
                             &name_chars,
                             nullptr,
                             &type,
                             data.data(),
                             &data_bytes);
    }
    if (status == ERROR_NO_MORE_ITEMS) {
      break;
    }
    if (status != ERROR_SUCCESS) {
      ++index;
      continue;
    }

    name.resize(name_chars);
    data.resize(data_bytes);
    const std::wstring value = RegistryDataToString(type, data);
    bool matched = false;
    for (const auto needle : name_needles) {
      if (!needle.empty() && fp::ContainsInsensitive(name, needle)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      for (const auto needle : value_needles) {
        if (!needle.empty() && fp::ContainsInsensitive(value, needle)) {
          matched = true;
          break;
        }
      }
    }
    if (matched) {
      names_to_delete.push_back(name);
    }
    ++index;
  }

  for (const auto& name : names_to_delete) {
    RegDeleteValueW(key, name.c_str());
  }
  RegCloseKey(key);
}

void RemoveStalePendingDeletes() {
  constexpr std::wstring_view kInstallDirNeedle = L"\\FluentPinyin";

  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SYSTEM\\CurrentControlSet\\Control\\Session Manager",
                    0,
                    KEY_QUERY_VALUE | KEY_SET_VALUE,
                    &key) != ERROR_SUCCESS) {
    return;
  }

  DWORD type = 0;
  DWORD bytes = 0;
  if (RegQueryValueExW(key, L"PendingFileRenameOperations", nullptr, &type, nullptr, &bytes) !=
          ERROR_SUCCESS ||
      type != REG_MULTI_SZ || bytes == 0) {
    RegCloseKey(key);
    return;
  }

  std::vector<BYTE> data(bytes);
  if (RegQueryValueExW(
          key, L"PendingFileRenameOperations", nullptr, &type, data.data(), &bytes) !=
      ERROR_SUCCESS) {
    RegCloseKey(key);
    return;
  }
  data.resize(bytes);

  const auto entries = ParseMultiStringWithEmptyItems(data);
  const auto font_match = [](std::wstring_view value) {
    return std::any_of(fp::kBundledFontEntries.begin(),
                       fp::kBundledFontEntries.end(),
                       [&](const auto& font) {
                         return fp::ContainsInsensitive(value, font.file);
                       });
  };
  std::vector<std::wstring> filtered;
  bool changed = false;
  for (size_t index = 0; index < entries.size(); index += 2) {
    const auto& source = entries[index];
    const std::wstring target = index + 1 < entries.size() ? entries[index + 1] : L"";
    bool remove = false;
    const bool install_dir_match =
        fp::ContainsInsensitive(source, kInstallDirNeedle) ||
        fp::ContainsInsensitive(target, kInstallDirNeedle);
    if (install_dir_match || font_match(source) || font_match(target)) {
      remove = true;
      changed = true;
    }
    if (!remove) {
      filtered.push_back(source);
      if (index + 1 < entries.size()) {
        filtered.push_back(target);
      }
    }
  }

  if (!changed) {
    RegCloseKey(key);
    return;
  }
  if (filtered.empty()) {
    RegDeleteValueW(key, L"PendingFileRenameOperations");
    RegCloseKey(key);
    return;
  }

  std::wstring multi;
  for (const auto& entry : filtered) {
    multi.append(entry);
    multi.push_back(L'\0');
  }
  multi.push_back(L'\0');
  RegSetValueExW(key,
                 L"PendingFileRenameOperations",
                 0,
                 REG_MULTI_SZ,
                 reinterpret_cast<const BYTE*>(multi.data()),
                 static_cast<DWORD>(multi.size() * sizeof(wchar_t)));
  RegCloseKey(key);
}

}  // namespace fp::devtools
