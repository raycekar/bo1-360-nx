// Job-system spin back-off.
//
// BO1's job workers (TU11 worker loop sub_825334A0) ask for work in a tight loop (sub_82533240, returns 0 when there
// is none). On the 360 the six hardware threads share one L2 and the loop is cheap; on a PC every empty try reads the
// lock-free queue's head and tail lines, which the main thread is writing to push jobs, so at 4 split-screen views the
// main thread and all workers spent 50-60 % of their time in the queue code. After an empty try a worker now pauses
// briefly (x86 PAUSE, bo1zm_job_backoff of them; 0 = off). Measured: no gain at 4 views (2026-11-27), so off by
// default; the real limit there is the GPU command processor thread (see STATUS.md).

#include <cstdint>

#include <immintrin.h>

#include <rex/cvar.h>
#include <rex/hook.h>

REXCVAR_DEFINE_INT32(bo1zm_job_backoff, 0, "BO1",
                     "PAUSE instructions a job worker spins after finding no job (0 = none, as on the 360)")
    .range(0, 4096);

REXCVAR_DEFINE_INT32(bo1zm_job_requeue_backoff, 0, "BO1",
                     "PAUSE instructions before a worker puts a still-blocked job back on the queue (0 = none)")
    .range(0, 4096);

// Call counts for the queue functions (log_job_counts): calls per 10 s, to tell op cost from op volume.
REXCVAR_DEFINE_BOOL(log_job_counts, false, "Log", "Every 10 s, log how often the job queue functions run");
#include <atomic>
#include <chrono>
#include <rex/logging.h>
namespace {
std::atomic<uint64_t> g_push{0}, g_pop{0}, g_take{0};
void MaybeLogCounts() {
  static std::atomic<int64_t> last{0};
  const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::steady_clock::now().time_since_epoch()).count();
  int64_t l = last.load();
  if (now - l >= 10 && last.compare_exchange_strong(l, now) && l) {
    REXLOG_INFO("[jobs] 10 s: push {} freelist-pop {} take-job {}", g_push.exchange(0), g_pop.exchange(0),
                g_take.exchange(0));
  }
}
}  // namespace

REX_EXTERN(__imp__sub_82534638);
REX_HOOK_RAW(sub_82534638) {
  if (REXCVAR_GET(log_job_counts)) {
    const uint64_t n = g_push.fetch_add(1, std::memory_order_relaxed);
    if ((n & 4095) == 0) {  // sample callers: caller and caller's caller
      const uint32_t chain = __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + ctx.r1.u32));
      const uint32_t up = (chain >= 0x40000000u && chain < 0x80000000u)
                              ? __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + chain - 8)) : 0;
      REXLOG_INFO("[jobs] push from {:08X} / {:08X} queue {:08X}", uint32_t(ctx.lr), up, ctx.r3.u32);
    }
    MaybeLogCounts();
  }
  // A worker putting back a job that is still blocked (it re-checks it at most every 50,000 ticks, 1 ms): ~2.7 million
  // such pushes a second at 4 views. Pause before it, so the circling jobs leave the queue to real pushes.
  const uint32_t lr = uint32_t(ctx.lr);
  if (lr == 0x825336D4 || lr == 0x82533934) {
    for (int i = REXCVAR_GET(bo1zm_job_requeue_backoff); i > 0; --i) _mm_pause();
  }
  __imp__sub_82534638(ctx, base);
}
REX_EXTERN(__imp__sub_825344C0);
REX_HOOK_RAW(sub_825344C0) {
  if (REXCVAR_GET(log_job_counts)) g_pop.fetch_add(1, std::memory_order_relaxed);
  __imp__sub_825344C0(ctx, base);
}

REX_EXTERN(__imp__sub_82533240);
REX_HOOK_RAW(sub_82533240) {  // job worker: try to take a job
  if (REXCVAR_GET(log_job_counts)) g_take.fetch_add(1, std::memory_order_relaxed);
  __imp__sub_82533240(ctx, base);
  if ((ctx.r3.u32 & 0xFF) == 0) {
    for (int i = REXCVAR_GET(bo1zm_job_backoff); i > 0; --i) _mm_pause();
  }
}
