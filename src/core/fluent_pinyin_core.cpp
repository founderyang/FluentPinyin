#include "common/constants.h"

#include <windows.h>

#include <cstdint>

extern "C" __declspec(dllexport) std::uint32_t FpCoreAbiVersion() {
  return 1;
}

extern "C" __declspec(dllexport) const wchar_t* FpCoreVersion() {
  return fp::kProductVersion.data();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  (void)reserved;

  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(instance);
  }

  return TRUE;
}
