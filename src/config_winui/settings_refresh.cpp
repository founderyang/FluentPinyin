#include "config_winui/settings_refresh.h"

#include "common/broadcast_messages.h"
#include "common/constants.h"

#include <windows.h>

#include <atomic>
#include <thread>

namespace fp::config_winui {
namespace {

using RequestCallback = void (*)();

void RequestDeferred(std::atomic<unsigned long>& generation,
                     unsigned long delay_ms,
                     RequestCallback callback) {
  const unsigned long request_generation = ++generation;
  std::thread([request_generation, delay_ms, callback, &generation]() {
    Sleep(static_cast<DWORD>(delay_ms));
    if (generation.load() != request_generation) {
      return;
    }
    callback();
  }).detach();
}

}  // namespace

void RequestToolbarHostRefresh() {
  fp::PostRegisteredBroadcastMessage(fp::kToolbarRefreshMessageName);
}

void RequestToolbarHostShutdown() {
  fp::PostRegisteredBroadcastMessage(fp::kToolbarHostShutdownMessageName);
}

void RequestApplyInputConfig() {
  fp::PostRegisteredBroadcastMessage(fp::kApplyInputConfigMessageName);
}

void RequestInputStateRefresh() {
  fp::PostRegisteredBroadcastMessage(fp::kRefreshInputStateMessageName);
}

void RequestCandidateWindowVisualRefresh() {
  fp::PostRegisteredBroadcastMessage(fp::kRefreshCandidateWindowVisualsMessageName);
  RequestInputStateRefresh();
}

void RequestApplyInputConfigDeferred(unsigned long delay_ms) {
  static std::atomic<unsigned long> generation{0};
  RequestDeferred(generation, delay_ms, RequestApplyInputConfig);
}

void RequestInputStateRefreshDeferred(unsigned long delay_ms) {
  static std::atomic<unsigned long> generation{0};
  RequestDeferred(generation, delay_ms, RequestInputStateRefresh);
}

void RequestCandidateWindowVisualRefreshDeferred(unsigned long delay_ms) {
  static std::atomic<unsigned long> generation{0};
  RequestDeferred(generation, delay_ms, RequestCandidateWindowVisualRefresh);
}

}  // namespace fp::config_winui
