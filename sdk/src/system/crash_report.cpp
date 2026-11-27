/**
 * @file        system/crash_report.cpp
 * @brief       Crash lines for the log (see rex/system/crash_report.h).
 *
 * @license     BSD 3-Clause License
 */

#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/ppc/context.h>
#include <rex/runtime.h>
#include <rex/system/crash_report.h>
#include <rex/system/thread_state.h>

#include <cstring>

#if REX_PLATFORM_WIN32
#include <windows.h>
#endif

namespace rex::runtime {

namespace {

// Guest stacks are committed memory; a frame more than 1 MB above the first one is garbage.
constexpr uint32_t kMaxStackSpan = 1u << 20;
constexpr int kMaxGuestFrames = 32;

bool GuestReadable(uint8_t* base, uint32_t addr) {
  if (!base || addr == 0 || (addr & 3)) return false;
#if REX_PLATFORM_WIN32
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(base + addr, &mbi, sizeof(mbi))) return false;
  if (mbi.State != MEM_COMMIT) return false;
  if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
#endif
  return true;
}

uint32_t LoadBE32(uint8_t* base, uint32_t addr) {
  uint32_t v;
  std::memcpy(&v, base + addr, sizeof(v));
  return __builtin_bswap32(v);
}

void FlushLog() {
  if (auto l = ::rex::GetLogger()) l->flush();
}

}  // namespace

void LogGuestCrashState(const PPCContext& ctx, uint8_t* base) {
  REXLOG_CRITICAL(
      "[CRASH] guest lr=0x{:08X} ctr=0x{:08X} r1=0x{:08X} r3=0x{:08X} r4=0x{:08X} r5=0x{:08X} r31=0x{:08X} "
      "last_indirect=0x{:08X}",
      uint32_t(ctx.lr), ctx.ctr.u32, ctx.r1.u32, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r31.u32,
      ctx.last_indirect_target);
  // Xbox 360 ABI: 0(sp) is the back chain; a function that saved LR stored it at -8 of its caller's sp.
  const uint32_t sp0 = ctx.r1.u32;
  uint32_t sp = sp0;
  for (int i = 0; i < kMaxGuestFrames && GuestReadable(base, sp); ++i) {
    uint32_t next = LoadBE32(base, sp);
    if (next <= sp || next - sp0 > kMaxStackSpan || !GuestReadable(base, next - 8)) break;
    uint32_t saved_lr = LoadBE32(base, next - 8);
    REXLOG_CRITICAL("[CRASH] guest frame {}: 0x{:08X}", i, saved_lr);
    sp = next;
  }
  FlushLog();
}

#if REX_PLATFORM_WIN32

namespace {

void FormatModuleOffset(uint64_t addr, char* out, size_t out_size) {
  HMODULE mod = nullptr;
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(addr), &mod) &&
      mod) {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(mod, path, MAX_PATH);
    const char* name = std::strrchr(path, '\\');
    name = name ? name + 1 : path;
    snprintf(out, out_size, "%s+0x%08llX", name,
             static_cast<unsigned long long>(addr - reinterpret_cast<uint64_t>(mod)));
  } else {
    snprintf(out, out_size, "0x%016llX", static_cast<unsigned long long>(addr));
  }
}

LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep) {
  static volatile LONG entered = 0;
  if (InterlockedExchange(&entered, 1)) return EXCEPTION_CONTINUE_SEARCH;  // one report only

  const EXCEPTION_RECORD* rec = ep->ExceptionRecord;
  char where[MAX_PATH + 32];
  FormatModuleOffset(reinterpret_cast<uint64_t>(rec->ExceptionAddress), where, sizeof(where));
  if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
    const char* kind = rec->ExceptionInformation[0] == 0 ? "read" : rec->ExceptionInformation[0] == 1 ? "write"
                                                                                                        : "exec";
    uint64_t fault = rec->ExceptionInformation[1];
    uint8_t* base = Runtime::instance() ? Runtime::instance()->virtual_membase() : nullptr;
    uint64_t b = reinterpret_cast<uint64_t>(base);
    if (base && fault >= b && fault - b < (1ull << 32)) {
      REXLOG_CRITICAL("[CRASH] native 0x{:08X} at {} ({} 0x{:016X} = guest 0x{:08X})", rec->ExceptionCode, where,
                      kind, fault, uint32_t(fault - b));
    } else {
      REXLOG_CRITICAL("[CRASH] native 0x{:08X} at {} ({} 0x{:016X})", rec->ExceptionCode, where, kind, fault);
    }
  } else {
    REXLOG_CRITICAL("[CRASH] native 0x{:08X} at {}", rec->ExceptionCode, where);
  }
  FlushLog();
  // Guest state first: the native unwind below can fault on a broken stack.
  if (ThreadState* ts = ThreadState::Get()) {
    REXLOG_CRITICAL("[CRASH] guest thread {}", ThreadState::GetThreadID());
    LogGuestCrashState(*ts->context(), Runtime::instance() ? Runtime::instance()->virtual_membase() : nullptr);
  }
  if (rec->ExceptionCode == EXCEPTION_STACK_OVERFLOW) return EXCEPTION_CONTINUE_SEARCH;  // no stack to unwind on

  // Native stack: unwind a copy of the faulting context with the .pdata tables.
  CONTEXT c = *ep->ContextRecord;
  for (int i = 0; i < 24 && c.Rip; ++i) {
    FormatModuleOffset(c.Rip, where, sizeof(where));
    REXLOG_CRITICAL("[CRASH] native frame {}: {}", i, where);
    DWORD64 image_base = 0;
    PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &image_base, nullptr);
    if (!fn) {  // leaf function: the return address is at [rsp]
      if (!c.Rsp) break;
      c.Rip = *reinterpret_cast<DWORD64*>(c.Rsp);
      c.Rsp += 8;
      continue;
    }
    PVOID handler_data = nullptr;
    DWORD64 establisher = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, c.Rip, fn, &c, &handler_data, &establisher, nullptr);
  }

  FlushLog();
  return EXCEPTION_CONTINUE_SEARCH;  // let Windows Error Reporting record it too
}

}  // namespace

void InstallCrashReporter() {
  SetUnhandledExceptionFilter(CrashFilter);
}

#else

void InstallCrashReporter() {}

#endif

}  // namespace rex::runtime
