// Engine error messages in the log.
//
// Com_Error (TU11 sub_82313280: r3 error code, r4 printf format, r5.. arguments) shows only a short text in the game
// ("Out of memory"); the log gets the full message, for example
// "Hunk_AllocAlign failed on 1048576 bytes (total 120 MB, low 80 MB, high 40 MB)", and the caller.

#include <cstring>
#include <string>

#include <rex/hook.h>
#include <rex/logging.h>

namespace {

// Formats a guest printf string with integer and string arguments from r5 onwards (enough for engine errors).
std::string FormatGuest(PPCContext& ctx, uint8_t* base, uint32_t fmt_ptr, int first_reg) {
  if (!fmt_ptr) return {};
  const char* fmt = reinterpret_cast<const char*>(base + fmt_ptr);
  const uint64_t regs[] = {ctx.r3.u64, ctx.r4.u64, ctx.r5.u64, ctx.r6.u64, ctx.r7.u64, ctx.r8.u64, ctx.r9.u64,
                           ctx.r10.u64};
  int reg = first_reg - 3;
  std::string out;
  for (const char* p = fmt; *p && out.size() < 1024; ++p) {
    if (*p >= 0x01 && *p <= 0x1F) continue;  // engine colour / error-class bytes
    if (*p != '%') {
      out += *p;
      continue;
    }
    ++p;
    while (*p && std::strchr("-+ #0123456789.lh", *p)) ++p;
    if (!*p) break;
    const uint64_t v = reg < 8 ? regs[reg++] : 0;
    switch (*p) {
      case 'i':
      case 'd': out += std::to_string(int32_t(v)); break;
      case 'u': out += std::to_string(uint32_t(v)); break;
      case 'x':
      case 'X': out += fmt::format("{:x}", uint32_t(v)); break;
      case 'p': out += fmt::format("{:08X}", uint32_t(v)); break;
      case 'c': out += char(v); break;
      case 's': {
        const uint32_t s = uint32_t(v);
        if (s >= 0x40000000u && s < 0xA0000000u) out += reinterpret_cast<const char*>(base + s);
        else out += "(null)";
        break;
      }
      case '%': out += '%'; --reg; break;
      default: out += '?'; break;
    }
  }
  return out;
}

}  // namespace

// Guest caller's caller: saved lr 8 bytes below the back chain (prologue mflr r12; stw r12,-8(r1); stwu r1,-N(r1)).
static uint32_t CallerOfCaller(PPCContext& ctx, uint8_t* base) {
  const uint32_t chain = __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + ctx.r1.u32));
  if (chain < 0x40000000u || chain >= 0x80000000u) return 0;
  return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + chain - 8));
}

// Fatal error with relaunch (TU11 sub_823AAA90: r3 message). The 360 stores the message in the launch data and
// restarts the title, which then shows it (Com_Frame); the real cause is only visible here, in the first process.
REX_EXTERN(__imp__sub_823AAA90);
REX_HOOK_RAW(sub_823AAA90) {
  REXLOG_ERROR("[Sys_Error] from {:08X} (caller {:08X}): {}", uint32_t(ctx.lr), CallerOfCaller(ctx, base),
               ctx.r3.u32 ? reinterpret_cast<const char*>(base + ctx.r3.u32) : "");
  rex::FlushLogging();
  __imp__sub_823AAA90(ctx, base);
}

// "Out of memory" (TU11 sub_823AB0B8), the allocators' failure path.
REX_EXTERN(__imp__sub_823AB0B8);
REX_HOOK_RAW(sub_823AB0B8) {
  REXLOG_ERROR("[OutOfMemory] from {:08X} (caller {:08X}), r3 {:08X} r4 {:08X} r5 {:08X}", uint32_t(ctx.lr),
               CallerOfCaller(ctx, base), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
  // PMem (physicalmemory.cpp) global at 0x833FC650: +4 base, +1060 size, +1328 the shortfall of the failed alloc.
  auto u32at = [&](uint32_t a) { return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + a)); };
  constexpr uint32_t kPMem = 0x833FC650;
  REXLOG_ERROR("[OutOfMemory] PMem base {:08X} size {} MB, short by {} KB", u32at(kPMem + 4),
               u32at(kPMem + 1060) >> 20, u32at(kPMem + 1328) >> 10);
  rex::FlushLogging();
  __imp__sub_823AB0B8(ctx, base);
}

REX_EXTERN(__imp__sub_82313280);
REX_HOOK_RAW(sub_82313280) {  // Com_Error(code, fmt, ...)
  REXLOG_WARN("[Com_Error] code {} from {:08X}: {}", ctx.r3.u32, uint32_t(ctx.lr),
              FormatGuest(ctx, base, ctx.r4.u32, 5));
  __imp__sub_82313280(ctx, base);
}
