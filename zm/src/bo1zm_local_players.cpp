// Four-player split-screen Zombies.
//
// The engine in default.xex is built for 4 local clients (getmaxlocalclients returns 4, the per-client memory is
// sized for 4 in split screen, r_num_viewports goes to 4), but the Zombies front-end menus run
// `party_maxlocalplayers 2` on every play path (split screen, System Link, online, private match). Multiplayer's
// split-screen menu runs the same commands with 4. See docs/4p-zombies-research.md.
//
// Dvar_RegisterInt (TU11 sub_8237AE80): r3 name, r4 default, r5 min, r6 max, r7 flags, r8 description.
// party_maxlocalplayers is registered as 2 / 1 / 4, flags 0 (at 0x823E4E68). We register it with the chosen
// count and the read-only flag (0x40, checked by the dvar change test at 0x82379220: "%s is read only."), so the
// menus' `party_maxlocalplayers 2` is refused and the value stays. bo1zm_local_players = 2 keeps the 360 limit.

#include <algorithm>
#include <cstring>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

REXCVAR_DEFINE_INT32(bo1zm_local_players, 4, "BO1",
                     "Local split-screen players allowed in Zombies (2-4). 2 keeps the Xbox 360 limit.")
    .range(2, 4);

namespace {
constexpr uint32_t kDvarReadOnly = 0x40;
}

REX_EXTERN(__imp__sub_8237AE80);
REX_HOOK_RAW(sub_8237AE80) {  // Dvar_RegisterInt
  const uint32_t name_ptr = ctx.r3.u32;
  if (name_ptr != 0 &&
      std::strcmp(reinterpret_cast<const char*>(base + name_ptr), "party_maxlocalplayers") == 0) {
    const int32_t players = std::clamp<int32_t>(REXCVAR_GET(bo1zm_local_players), 2, 4);
    if (players != 2) {
      ctx.r4.u64 = static_cast<uint32_t>(players);
      ctx.r7.u64 = ctx.r7.u32 | kDvarReadOnly;
      REXLOG_INFO("bo1zm: party_maxlocalplayers = {} (read-only; menus' 2 is ignored)", players);
    }
  }
  __imp__sub_8237AE80(ctx, base);
}
