#pragma once
#include <string>

namespace bo1zm {
// Runs a console line ("set cg_fov 90", "god", ...) at the start of the game's next frame, on its main thread.
void QueueCommand(const std::string& line);
// The guest dvar_t* registered under this name (case-insensitive), 0 if none yet.
uint32_t FindDvar(const std::string& name);
// A bool/int/float/enum dvar's current value; false if unknown or another type.
bool DvarNumber(const std::string& name, double* out);
// Called at the start of every game frame on the main thread (bo1zm_options.cpp: keeps settings applied).
void OptionsTick();
}  // namespace bo1zm
