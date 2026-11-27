/**
 * @file        system/crash_report.h
 * @brief       Crash lines for the log: native fault address and guest call stack.
 *
 * @license     BSD 3-Clause License
 *
 * @remarks     Added for bo1-360-nx. Every line starts with "[CRASH]" so tools/crash_triage.py can read it:
 *                [CRASH] native 0xC0000005 at bo1zm.exe+0x0001A2B3C (read 0x00000000)
 *                [CRASH] native frame 3: bo1zm.exe+0x0001A2B3C
 *                [CRASH] guest lr=0x82101EA4 ctr=0x... r1=0x... r3=... last_indirect=0x...
 *                [CRASH] guest frame 2: 0x8247588C
 */

#pragma once

#include <cstdint>

struct PPCContext;

namespace rex::runtime {

/** Logs the guest registers and the back-chain call stack of ctx (saved LR of each frame). */
void LogGuestCrashState(const PPCContext& ctx, uint8_t* base);

/** Windows: installs an unhandled-exception filter that logs the crash lines and flushes the log. */
void InstallCrashReporter();

}  // namespace rex::runtime
