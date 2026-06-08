#pragma once

#include <windows.h>

#include <string_view>

namespace fp {

UINT RegisteredBroadcastMessage(std::wstring_view message_name);
bool PostRegisteredBroadcastMessage(std::wstring_view message_name,
                                    WPARAM wparam = 0,
                                    LPARAM lparam = 0);
bool SendRegisteredBroadcastMessage(std::wstring_view message_name,
                                    DWORD timeout_ms = 2000,
                                    WPARAM wparam = 0,
                                    LPARAM lparam = 0);

}  // namespace fp
