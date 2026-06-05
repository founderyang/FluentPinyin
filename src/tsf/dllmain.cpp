#include "tsf/class_factory.h"
#include "tsf/guids.h"
#include "tsf/module.h"

#include <gdiplus.h>
#include <windows.h>

#include <new>
#include <mutex>

namespace fp::tsf {

HINSTANCE g_module_instance = nullptr;
std::atomic<unsigned long> g_object_count = 0;
std::atomic<unsigned long> g_server_lock_count = 0;
ULONG_PTR g_gdiplus_token = 0;
bool g_gdiplus_ready = false;
std::once_flag g_gdiplus_once;

void EnsureGdiplus() {
  std::call_once(g_gdiplus_once, [] {
    Gdiplus::GdiplusStartupInput gdiplus_input{};
    g_gdiplus_ready =
        Gdiplus::GdiplusStartup(&g_gdiplus_token, &gdiplus_input, nullptr) == Gdiplus::Ok;
  });
}

}  // namespace fp::tsf

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  (void)reserved;

  if (reason == DLL_PROCESS_ATTACH) {
    fp::tsf::g_module_instance = instance;
    DisableThreadLibraryCalls(instance);
  } else if (reason == DLL_PROCESS_DETACH) {
    if (fp::tsf::g_gdiplus_ready) {
      Gdiplus::GdiplusShutdown(fp::tsf::g_gdiplus_token);
      fp::tsf::g_gdiplus_ready = false;
      fp::tsf::g_gdiplus_token = 0;
    }
  }

  return TRUE;
}

STDAPI DllCanUnloadNow() {
  return (fp::tsf::g_object_count == 0 && fp::tsf::g_server_lock_count == 0)
             ? S_OK
             : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** object) {
  if (object == nullptr) {
    return E_POINTER;
  }

  *object = nullptr;

  if (!IsEqualCLSID(clsid, fp::tsf::kTextServiceClsid)) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }

  auto* factory = new (std::nothrow) fp::tsf::ClassFactory();
  if (factory == nullptr) {
    return E_OUTOFMEMORY;
  }

  const HRESULT result = factory->QueryInterface(iid, object);
  factory->Release();
  return result;
}

STDAPI DllRegisterServer() {
  return fp::tsf::RegisterServer();
}

STDAPI DllUnregisterServer() {
  return fp::tsf::UnregisterServer();
}
