#include "common/broadcast_messages.h"

#include <string>

namespace fp {

UINT RegisteredBroadcastMessage(std::wstring_view message_name) {
  if (message_name.empty()) {
    return 0;
  }
  return RegisterWindowMessageW(std::wstring(message_name).c_str());
}

bool PostRegisteredBroadcastMessage(std::wstring_view message_name,
                                    WPARAM wparam,
                                    LPARAM lparam) {
  const UINT message = RegisteredBroadcastMessage(message_name);
  return message != 0 && PostMessageW(HWND_BROADCAST, message, wparam, lparam) != FALSE;
}

bool SendRegisteredBroadcastMessage(std::wstring_view message_name,
                                    DWORD timeout_ms,
                                    WPARAM wparam,
                                    LPARAM lparam) {
  const UINT message = RegisteredBroadcastMessage(message_name);
  if (message == 0) {
    return false;
  }
  DWORD_PTR result = 0;
  return SendMessageTimeoutW(HWND_BROADCAST,
                             message,
                             wparam,
                             lparam,
                             SMTO_ABORTIFHUNG,
                             timeout_ms,
                             &result) != FALSE;
}

}  // namespace fp
