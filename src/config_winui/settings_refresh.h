#pragma once

namespace fp::config_winui {

void RequestToolbarHostRefresh();
void RequestToolbarHostShutdown();
void RequestApplyInputConfig();
void RequestInputStateRefresh();
void RequestCandidateWindowVisualRefresh();

void RequestApplyInputConfigDeferred(unsigned long delay_ms = 360);
void RequestInputStateRefreshDeferred(unsigned long delay_ms = 260);
void RequestCandidateWindowVisualRefreshDeferred(unsigned long delay_ms = 120);

}  // namespace fp::config_winui
