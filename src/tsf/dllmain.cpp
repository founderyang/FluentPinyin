#include "tsf/class_factory.h"
#include "tsf/guids.h"
#include "tsf/module.h"

#include <windows.h>

#include <new>

namespace fp::tsf {

HINSTANCE g_module_instance = nullptr;
std::atomic<unsigned long> g_object_count = 0;
std::atomic<unsigned long> g_server_lock_count = 0;

}  // namespace fp::tsf

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  (void)reserved;

  if (reason == DLL_PROCESS_ATTACH) {
    fp::tsf::g_module_instance = instance;
    DisableThreadLibraryCalls(instance);
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
