#pragma once
#include <string>

namespace bo1zm {
// Runs a console line ("set cg_fov 90", "god", ...) at the start of the game's next frame, on its main thread.
void QueueCommand(const std::string& line);
// The guest dvar_t* registered under this name (case-insensitive), 0 if none yet.
uint32_t FindDvar(const std::string& name);
}  // namespace bo1zm
