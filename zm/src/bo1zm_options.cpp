// Graphics presets (the port's Options > Graphics, as in bo2-360-nx).
//
// bo1zm_graphics_preset picks a set of the game's own render dvars. "360" leaves the game alone. The lower presets
// trade shadows, dynamic lights and model detail for speed: each shadow-casting light is another pass over the scene,
// so fewer of them also means fewer draws for the GPU command thread, the limit at 4 split-screen views. The higher
// presets add what the engine allows above the 360 (it caps dynamic and shadow lights at 4 and the LOD scale at 1):
// more model detail at distance, full-resolution spot shadows and a larger dynamic-light radius.
//
// Maps and their scripts set some of these dvars when they load, so the values are checked about twice a second and
// set again when they differ (a console "set" on the main thread, bo1zm_console.cpp).

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "bo1zm_console.h"

REXCVAR_DEFINE_STRING(bo1zm_graphics_preset, "360", "BO1 graphics",
                      "lowest, low, 360 (the Xbox 360's own settings, untouched), high or ultra")
    .allowed({"lowest", "low", "360", "high", "ultra"});

namespace {

struct Setting {
  const char* dvar;
  double value;
};

const std::vector<Setting>& PresetSettings(const std::string& preset) {
  // Shadows: sm_enable all, sm_maxLights shadow-casting lights (max 4). (The game keeps sm_sunEnable and sm_spotEnable
  // at 1 whatever the console sets.)
  // Dynamic lights: r_dlightLimit (max 4), r_dlightMax*Radius. Detail: r_lodScale* (>= 1, larger = simpler sooner),
  // r_lodBias* (negative = finer). Effects: r_dof_enable (depth of field).
  static const std::vector<Setting> lowest = {
      {"sm_enable", 0},
      {"sm_maxLights", 0},    {"r_dlightLimit", 0},        {"r_lodScaleRigid", 2},
      {"r_lodScaleSkinned", 2}, {"r_lodBiasRigid", 0},     {"r_lodBiasSkinned", 0},
      {"r_dof_enable", 0},
  };
  static const std::vector<Setting> low = {
      {"sm_enable", 1},
      {"sm_maxLights", 1},      {"r_dlightLimit", 2},      {"r_lodScaleRigid", 1.5},
      {"r_lodScaleSkinned", 1.5}, {"r_lodBiasRigid", 0},   {"r_lodBiasSkinned", 0},
      {"r_dof_enable", 0},
  };
  static const std::vector<Setting> high = {
      {"sm_enable", 1},          {"sm_sunEnable", 1},      {"sm_spotEnable", 1},
      {"sm_maxLights", 4},       {"r_dlightLimit", 4},     {"sm_fullResSpotShadowEnable", 1},
      {"r_lodScaleRigid", 1},    {"r_lodScaleSkinned", 1}, {"r_lodBiasRigid", -200},
      {"r_lodBiasSkinned", -200}, {"r_dlightMaxFullScreenRadius", 300}, {"r_dlightMaxNonFullScreenRadius", 300},
  };
  static const std::vector<Setting> ultra = {
      {"sm_enable", 1},          {"sm_sunEnable", 1},      {"sm_spotEnable", 1},
      {"sm_maxLights", 4},       {"r_dlightLimit", 4},     {"sm_fullResSpotShadowEnable", 1},
      {"r_lodScaleRigid", 1},    {"r_lodScaleSkinned", 1}, {"r_lodBiasRigid", -500},
      {"r_lodBiasSkinned", -500}, {"r_dlightMaxFullScreenRadius", 400}, {"r_dlightMaxNonFullScreenRadius", 400},
  };
  static const std::vector<Setting> none;
  if (preset == "lowest") return lowest;
  if (preset == "low") return low;
  if (preset == "high") return high;
  if (preset == "ultra") return ultra;
  return none;
}

std::string Number(double v) {
  if (std::floor(v) == v) return std::to_string(int64_t(v));
  std::string s = std::to_string(v);
  s.erase(s.find_last_not_of('0') + 1);
  return s;
}

}  // namespace

namespace bo1zm {
void OptionsTick() {
  static uint32_t frame = 0;
  if (++frame % 30 != 0) return;  // about twice a second at 60 fps
  // A value the game keeps refusing (out of its range) is tried 3 times in a row, then left alone with a warning.
  static std::unordered_map<std::string, int> refused;
  for (const auto& s : PresetSettings(REXCVAR_GET(bo1zm_graphics_preset))) {
    double current = 0;
    if (!DvarNumber(s.dvar, &current)) continue;  // not registered yet (front end before the renderer)
    int& tries = refused[s.dvar];
    if (std::fabs(current - s.value) <= 1e-4) {
      tries = 0;
      continue;
    }
    if (tries > 3) continue;
    if (++tries == 4) {
      REXLOG_WARN("bo1zm: the game refuses {} = {} (stays {})", s.dvar, Number(s.value), Number(current));
      continue;
    }
    QueueCommand(std::string("set ") + s.dvar + " " + Number(s.value));
  }
}
}  // namespace bo1zm
