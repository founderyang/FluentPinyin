#pragma once

#include <windows.h>
#include <unknwn.h>

#include <atomic>

namespace fp::tsf {

class ClassFactory final : public IClassFactory {
 public:
  ClassFactory();
  ClassFactory(const ClassFactory&) = delete;
  ClassFactory& operator=(const ClassFactory&) = delete;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** object) override;
  STDMETHODIMP LockServer(BOOL lock) override;

 private:
  ~ClassFactory();

  std::atomic<unsigned long> ref_count_{1};
};

}  // namespace fp::tsf
