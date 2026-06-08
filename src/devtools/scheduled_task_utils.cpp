#include "devtools/scheduled_task_utils.h"

#include "common/logging.h"

#include <taskschd.h>
#include <windows.h>

#include <string>

namespace fp::devtools {
namespace {

constexpr std::wstring_view kTaskName = L"FluentPinyinAutoSync";

std::wstring HresultToString(HRESULT result) {
  wchar_t buffer[16]{};
  swprintf_s(buffer, L"0x%08lX", static_cast<unsigned long>(result));
  return buffer;
}

template <typename T>
class ComPtr {
 public:
  ComPtr() = default;
  ComPtr(const ComPtr&) = delete;
  ComPtr& operator=(const ComPtr&) = delete;
  ~ComPtr() { Reset(); }

  T** put() {
    Reset();
    return &ptr_;
  }

  T* get() const { return ptr_; }
  T* operator->() const { return ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

  void Reset() {
    if (ptr_ != nullptr) {
      ptr_->Release();
      ptr_ = nullptr;
    }
  }

 private:
  T* ptr_ = nullptr;
};

}  // namespace

bool RemoveScheduledTask() {
  ComPtr<ITaskService> service;
  HRESULT result = CoCreateInstance(CLSID_TaskScheduler,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_ITaskService,
                                    reinterpret_cast<void**>(service.put()));
  if (FAILED(result) || !service) {
    fp::LogWarning(L"installer",
                   L"scheduled task cleanup: failed to create task service: " +
                       HresultToString(result));
    return false;
  }

  VARIANT empty;
  VariantInit(&empty);
  result = service->Connect(empty, empty, empty, empty);
  if (FAILED(result)) {
    fp::LogWarning(L"installer",
                   L"scheduled task cleanup: failed to connect task service: " +
                       HresultToString(result));
    return false;
  }

  ComPtr<ITaskFolder> root;
  BSTR root_path = SysAllocString(L"\\");
  result = service->GetFolder(root_path, root.put());
  SysFreeString(root_path);
  if (FAILED(result) || !root) {
    fp::LogWarning(L"installer",
                   L"scheduled task cleanup: failed to open root folder: " +
                       HresultToString(result));
    return false;
  }

  BSTR task_name = SysAllocString(std::wstring(kTaskName).c_str());
  result = root->DeleteTask(task_name, 0);
  SysFreeString(task_name);
  if (SUCCEEDED(result)) {
    fp::LogInfo(L"installer", L"scheduled task cleanup: removed task.");
    return true;
  }
  if (HRESULT_CODE(result) == ERROR_FILE_NOT_FOUND) {
    fp::LogInfo(L"installer", L"scheduled task cleanup: task not present.");
    return true;
  }
  fp::LogWarning(L"installer",
                 L"scheduled task cleanup: DeleteTask failed: " + HresultToString(result));
  return false;
}

}  // namespace fp::devtools
