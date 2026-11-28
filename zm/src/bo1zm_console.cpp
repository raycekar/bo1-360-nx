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

#include <cctype>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

#include <fmt/format.h>

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

// Dvar registry from the registration path (TU11 sub_8237AD78: r3 name, r4 type, ... returns the dvar), for
// reading values back ("dvar <name>" on the test pipe; the retail build prints nothing on the console).
// dvar_t (360, T5): +0 name, +12 flags, +16 type, +24 current (16 bytes), +56 reset.
namespace {
std::mutex g_dvar_mutex;
std::unordered_map<std::string, uint32_t> g_dvars;  // lowercase name -> guest dvar_t*
uint8_t* g_base = nullptr;

std::string Lower(std::string s) {
  for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string DvarValueText(uint8_t* base, uint32_t dvar, uint32_t offset) {
  auto u32 = [&](uint32_t a) { return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + a)); };
  const uint32_t type = u32(dvar + 16);
  const uint32_t v = dvar + offset;
  switch (type) {
    case 0: return base[v] ? "1" : "0";
    case 1: { uint32_t bits = u32(v); float f; std::memcpy(&f, &bits, 4); return fmt::format("{}", f); }
    case 5: case 6: return std::to_string(int32_t(u32(v)));
    case 7: { const uint32_t s = u32(v); return s ? std::string("\"") + reinterpret_cast<const char*>(base + s) + "\"" : "\"\""; }
    default: return fmt::format("(type {})", type);
  }
}

struct RegisterDvarPipe {
  RegisterDvarPipe() {
    rex::input::script::RegisterCommand("dvar", [](const std::string& name) -> std::string {
      std::lock_guard lock(g_dvar_mutex);
      auto it = g_dvars.find(Lower(name));
      if (it == g_dvars.end() || !g_base) return "error unknown dvar " + name;
      const uint32_t flags = __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_base + it->second + 12));
      // Domain at +72: int min/max (+72/+76), float min/max (+72/+76).
      auto u32 = [&](uint32_t a) { return __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_base + a)); };
      const uint32_t type = u32(it->second + 16);
      std::string domain;
      if (type == 5) {
        domain = fmt::format(", range {}..{}", int32_t(u32(it->second + 72)), int32_t(u32(it->second + 76)));
      } else if (type == 1) {
        uint32_t lo = u32(it->second + 72), hi = u32(it->second + 76);
        float flo, fhi;
        std::memcpy(&flo, &lo, 4);
        std::memcpy(&fhi, &hi, 4);
        domain = fmt::format(", range {}..{}", flo, fhi);
      }
      return fmt::format("{} = {} (reset {}, flags {:X}{})", name, DvarValueText(g_base, it->second, 24),
                         DvarValueText(g_base, it->second, 56), flags, domain);
    });
  }
} g_register_dvar_pipe;
}  // namespace

namespace bo1zm {
uint32_t FindDvar(const std::string& name) {
  std::lock_guard lock(g_dvar_mutex);
  auto it = g_dvars.find(Lower(name));
  return it == g_dvars.end() ? 0 : it->second;
}

bool DvarNumber(const std::string& name, double* out) {
  const uint32_t dvar = FindDvar(name);
  if (!dvar || !g_base) return false;
  auto u32 = [&](uint32_t a) { return __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_base + a)); };
  switch (u32(dvar + 16)) {
    case 0: *out = g_base[dvar + 24] ? 1.0 : 0.0; return true;
    case 1: { uint32_t bits = u32(dvar + 24); float f; std::memcpy(&f, &bits, 4); *out = f; return true; }
    case 5: case 6: *out = int32_t(u32(dvar + 24)); return true;
    default: return false;
  }
}
}  // namespace bo1zm

REX_EXTERN(__imp__sub_8237AD78);
REX_HOOK_RAW(sub_8237AD78) {  // Dvar_RegisterVariant
  const uint32_t name_ptr = ctx.r3.u32;
  __imp__sub_8237AD78(ctx, base);
  if (name_ptr && ctx.r3.u32) {
    std::lock_guard lock(g_dvar_mutex);
    g_base = base;
    g_dvars[Lower(reinterpret_cast<const char*>(base + name_ptr))] = ctx.r3.u32;
  }
}

REX_EXTERN(__imp__sub_823112E0);
REX_EXTERN(__imp__sub_82315590);
REX_HOOK_RAW(sub_82315590) {  // Com_Frame
  bo1zm::OptionsTick();
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
