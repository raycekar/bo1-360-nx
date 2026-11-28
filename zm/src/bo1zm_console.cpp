// Console commands from the port (settings, cheats, tests), run on the game's main thread.
//
// TU11 Zombies:
//   sub_823112E0  Cmd_ExecuteSingleCommand(localClient, controller, text): runs one console line at once
//                 (forwards to sub_823110A8(localClient, controller, 0, text); found from the "exec %s\n" caller
//                 sub_823A8B10).
//   sub_82315590  the per-frame function on the main thread (Com_Frame: it also raises the error a relaunched
//                 title left in its launch data). Queued lines run at its start.
// Commands must not run on another thread (dvars and the command buffer are not locked), hence the queue.
// Test pipe command: "cmd <console line>", e.g. "cmd set cg_fov 90".

#include "bo1zm_console.h"

#include <cstring>
#include <deque>
#include <mutex>
#include <string>

#include <rex/hook.h>
#include <rex/input/script/script_input_driver.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>

namespace {
std::mutex g_mutex;
std::deque<std::string> g_queue;
// Guest scratch for the command text (the game reads it from guest memory).
uint32_t g_scratch = 0;
constexpr uint32_t kScratchSize = 1024;

struct RegisterPipe {
  RegisterPipe() {
    rex::input::script::RegisterCommand("cmd", [](const std::string& line) -> std::string {
      if (line.empty()) return "error usage: cmd <console line>";
      bo1zm::QueueCommand(line);
      return "ok queued";
    });
  }
} g_register_pipe;
}  // namespace

namespace bo1zm {
void QueueCommand(const std::string& line) {
  std::lock_guard lock(g_mutex);
  g_queue.push_back(line);
}
}  // namespace bo1zm

REX_EXTERN(__imp__sub_823112E0);
REX_EXTERN(__imp__sub_82315590);
REX_HOOK_RAW(sub_82315590) {  // Com_Frame
  std::deque<std::string> lines;
  {
    std::lock_guard lock(g_mutex);
    lines.swap(g_queue);
  }
  if (!lines.empty()) {
    if (!g_scratch) {
      g_scratch = REX_KERNEL_MEMORY()->SystemHeapAlloc(kScratchSize);
    }
    if (g_scratch) {
      PPCContext saved = ctx;
      for (const auto& line : lines) {
        const size_t n = std::min<size_t>(line.size(), kScratchSize - 1);
        std::memcpy(base + g_scratch, line.data(), n);
        base[g_scratch + n] = 0;
        ctx.r3.u64 = 0;  // local client 0
        ctx.r4.u64 = 0;  // controller 0
        ctx.r5.u64 = g_scratch;
        __imp__sub_823112E0(ctx, base);
        REXLOG_INFO("bo1zm: ran \"{}\"", line);
      }
      ctx = saved;
    }
  }
  __imp__sub_82315590(ctx, base);
}
