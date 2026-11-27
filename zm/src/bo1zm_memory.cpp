// A bigger physical-memory pool for 4-player split screen.
//
// BO1 takes one 427 MB block of physical memory at boot for its own allocator (physicalmemory.cpp, "PMem"; init
// TU11 sub_82382AA0, global at 0x833FC650). On the 360 that is nearly all a title gets. Four local players need a bit
// more than the 360's two: Kino der Toten at 4 players stopped with "Out of memory", 576 KB short. The PC's guest has
// physical memory left over, so the pool asks for bo1zm_pmem_extra_mb more (halving it until the allocation fits) and
// then records the larger size in the two fields the init sets from the size: +1060 (end of the low stack / start of
// the high one) and +1320. 0 keeps the 360's size.

#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

REXCVAR_DEFINE_INT32(bo1zm_pmem_extra_mb, 48, "BO1",
                     "Extra MB for the game's physical-memory pool (4-player split screen needs more than the 360's "
                     "427 MB). 0 = the 360 size")
    .range(0, 64);

namespace {
constexpr uint32_t kPMemGlobal = 0x833FC650;
constexpr uint32_t kPMemRequest = 0x1AB00000;  // 427 MB, the size PMem_Init asks for
uint32_t g_extra_bytes = 0;
bool g_in_pmem_init = false;

uint32_t LoadU32(uint8_t* base, uint32_t a) { return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + a)); }
void StoreU32(uint8_t* base, uint32_t a, uint32_t v) { *reinterpret_cast<uint32_t*>(base + a) = __builtin_bswap32(v); }
}  // namespace

// XPhysicalAlloc (TU11 sub_824BD358: r3 size, r4 max address, r5 alignment, r6 protect/flags).
REX_EXTERN(__imp__sub_824BD358);
REX_HOOK_RAW(sub_824BD358) {
  if (g_in_pmem_init && ctx.r3.u32 == kPMemRequest && g_extra_bytes == 0) {
    const uint64_t r4 = ctx.r4.u64, r5 = ctx.r5.u64, r6 = ctx.r6.u64;
    for (uint32_t extra_mb = uint32_t(REXCVAR_GET(bo1zm_pmem_extra_mb)); extra_mb >= 1; extra_mb /= 2) {
      ctx.r3.u64 = kPMemRequest + (extra_mb << 20);
      ctx.r4.u64 = r4;
      ctx.r5.u64 = r5;
      ctx.r6.u64 = r6;
      __imp__sub_824BD358(ctx, base);
      if (ctx.r3.u32) {
        g_extra_bytes = extra_mb << 20;
        REXLOG_INFO("bo1zm: physical-memory pool {} MB (360: 427 MB + {} MB)", (kPMemRequest >> 20) + extra_mb,
                    extra_mb);
        return;
      }
    }
    ctx.r3.u64 = kPMemRequest;
    ctx.r4.u64 = r4;
    ctx.r5.u64 = r5;
    ctx.r6.u64 = r6;
  }
  __imp__sub_824BD358(ctx, base);
}

// PMem_Init (TU11 sub_82382AA0).
REX_EXTERN(__imp__sub_82382AA0);
REX_HOOK_RAW(sub_82382AA0) {
  g_in_pmem_init = true;
  __imp__sub_82382AA0(ctx, base);
  g_in_pmem_init = false;
  if (g_extra_bytes && LoadU32(base, kPMemGlobal + 1060) == kPMemRequest &&
      LoadU32(base, kPMemGlobal + 1320) == kPMemRequest) {
    StoreU32(base, kPMemGlobal + 1060, kPMemRequest + g_extra_bytes);
    StoreU32(base, kPMemGlobal + 1320, kPMemRequest + g_extra_bytes);
    g_extra_bytes = 0xFFFFFFFFu;  // applied once
  }
}
