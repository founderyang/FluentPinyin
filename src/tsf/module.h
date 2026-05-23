#pragma once

#include <windows.h>

#include <atomic>

namespace fp::tsf {

extern HINSTANCE g_module_instance;
extern std::atomic<unsigned long> g_object_count;
extern std::atomic<unsigned long> g_server_lock_count;

HRESULT RegisterServer();
HRESULT UnregisterServer();

}  // namespace fp::tsf
