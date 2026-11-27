// Online co-op setting (bo1_online), shared by bo1zm and bo1mp. See bo1_online.cpp.
#pragma once

namespace rex::ui {
class ImGuiDrawer;
}

namespace bo1 {

// Reads bo1_online and starts the background preparation. Returns false when online play is off or the code is bad.
bool StartOnlineFromSettings();
// Opens the short-lived status window (call from OnCreateDialogs, after StartOnlineFromSettings).
void ShowOnlineStatus(rex::ui::ImGuiDrawer* drawer);
// At exit: removes the router's port forwarding.
void ShutdownOnline();

}  // namespace bo1
