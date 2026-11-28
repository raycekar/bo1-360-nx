#pragma once
#include <string>

namespace bo1zm {
// Runs a console line ("set cg_fov 90", "god", ...) at the start of the game's next frame, on its main thread.
void QueueCommand(const std::string& line);
}  // namespace bo1zm
