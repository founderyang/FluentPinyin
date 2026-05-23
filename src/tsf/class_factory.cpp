#include "tsf/class_factory.h"

#include "common/logging.h"
#include "tsf/module.h"
#include "tsf/tsf_text_service.h"

#include <new>

namespace fp::tsf {

ClassFactory::ClassFactory() {
  ++g_object_count;
}

ClassFactory::~ClassFactory() {
  --g_object_count;
}

STDMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** object) {
  if (object == nullptr) {
    return E_POINTER;
  }

  *object = nullptr;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
    *object = static_cast<IClassFactory*>(this);
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ClassFactory::AddRef() {
  return ++ref_count_;
}

STDMETHODIMP_(ULONG) ClassFactory::Release() {
  const ULONG count = --ref_count_;
  if (count == 0) {
    delete this;
  }

  return count;
}

STDMETHODIMP ClassFactory::CreateInstance(IUnknown* outer, REFIID riid, void** object) {
  if (object == nullptr) {
    return E_POINTER;
  }

  *object = nullptr;

  if (outer != nullptr) {
    return CLASS_E_NOAGGREGATION;
  }

  auto* service = new (std::nothrow) TsfTextService();
  if (service == nullptr) {
    return E_OUTOFMEMORY;
  }

  const HRESULT result = service->QueryInterface(riid, object);
  service->Release();

  if (FAILED(result)) {
    fp::LogWarning(L"tsf", L"TsfTextService QueryInterface failed during creation.");
  }

  return result;
}

STDMETHODIMP ClassFactory::LockServer(BOOL lock) {
  if (lock) {
    ++g_server_lock_count;
  } else if (g_server_lock_count > 0) {
    --g_server_lock_count;
  }

  return S_OK;
}

}  // namespace fp::tsf
