// Four local players in Multiplayer System Link.
//
// Multiplayer's menus set party_maxlocalplayers per mode: split screen 4, System Link 2, theater 1 (ui_mp.ff /
// patch_ui_mp.ff: `systemlink 1" ; exec "party_maxlocalplayers 2"`). The engine is 4-wide in all of them. So that a
// PC can bring 3 or 4 local players into a System Link (and online) game, the value 2 becomes the port's setting;
// the other values (1 for theater, 4 for split screen) pass unchanged.
//
// Dvar_SetVariant (TU11 default_mp.xex sub_823E2418, same shape as Zombies' sub_823792F8): r3 dvar, r4:r5 the new
// 16-byte value passed by value (an int is the first word, so the high half of r4), r6 the source. The dvar's first
// field is its name.

#include <algorithm>
#include <cstring>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

REXCVAR_DEFINE_INT32(bo1mp_systemlink_local_players, 4, "BO1",
                     "Local players allowed per PC in Multiplayer System Link / online (2-4). 2 keeps the Xbox 360 "
                     "limit. Split screen always allows 4.")
    .range(2, 4);

REX_EXTERN(__imp__sub_823E2418);
REX_HOOK_RAW(sub_823E2418) {  // Dvar_SetVariant
  const uint32_t dvar = ctx.r3.u32;
  if (dvar != 0 && static_cast<uint32_t>(ctx.r4.u64 >> 32) == 2) {
    uint32_t name_ptr;
    std::memcpy(&name_ptr, base + dvar, 4);
    name_ptr = __builtin_bswap32(name_ptr);
    if (name_ptr != 0 && std::strcmp(reinterpret_cast<const char*>(base + name_ptr), "party_maxlocalplayers") == 0) {
      const uint32_t players = static_cast<uint32_t>(std::clamp<int32_t>(REXCVAR_GET(bo1mp_systemlink_local_players), 2, 4));
      if (players != 2) {
        ctx.r4.u64 = (uint64_t(players) << 32) | (ctx.r4.u64 & 0xFFFFFFFFull);
        REXLOG_INFO("bo1mp: party_maxlocalplayers 2 -> {} (System Link)", players);
      }
    }
  }
  __imp__sub_823E2418(ctx, base);
}
