"""Turn "store back what lwarx/ldarx just loaded" into a plain compare, in the generated C++.

PowerPC lock-free code (the 360 SDK's InterlockedCompareExchange idiom) clears its reservation on the
compare-failed path by storing the loaded value back:

    loop: ldarx  r9,0,r11      ; load and reserve
          cmpd   r9,r10
          bne    fail
          stdcx. r7,0,r11      ; the real store
          bne    loop
          b      done
    fail: stdcx. r9,0,r11      ; write back the same value: only clears the reservation

The recompiler emits every stdcx./stwcx. as a compare-and-swap. On x86 that write-back is a locked cmpxchg
that takes the cache line exclusively although nothing changes, so threads that spin on a busy queue (BO1's
job system: the main thread and four workers on one lock-free list) fight over the line on every failed try.
There is no reservation to clear on x86. This pass replaces such a write-back with a plain load and compare,
which gives the same cr0.eq (memory still holds the reserved value) without a write.

Rewritten only when, inside one generated function:
  - the stcx stores the register the closest preceding larx loaded (same width),
  - nothing writes that register between the two,
  - every goto into a label between them comes from after the larx (no other path reaches the stcx).
Idempotent. usage: python -I tools/reservation_writeback.py --gen zm/generated/default [--dry-run]
"""
import argparse
import glob
import os
import re
import sys

LARX = re.compile(r"^\s*// (lwarx|ldarx) (r\d+),")
STCX = re.compile(r"^\s*// (stwcx|stdcx)\. (r\d+),")
LABEL = re.compile(r"^(loc_[0-9A-F]+):")
FUNC = re.compile(r"^DEFINE_REX_FUNC\(")
CAS = re.compile(
    r"^(\s*)cr0\.eq = __sync_bool_compare_and_swap\(reinterpret_cast<uint(32|64)_t\*>\(REX_RAW_ADDR\(ea\)\), "
    r"ctx\.reserved\.s(32|64), __builtin_bswap(32|64)\((?:ctx\.)?(r\d+)\.s(32|64)\)\);\s*$")
MARK = "// reservation write-back elided"


def writes(line, reg):
    return re.search(r"(?<![\w.])(?:ctx\.)?%s\.(?:u64|s64|u32|s32|u8|u16)\s*=[^=]" % reg, line) is not None


def process(lines):
    changed = 0
    func_start = 0
    for i, line in enumerate(lines):
        if FUNC.match(line):
            func_start = i
            continue
        m = STCX.match(line)
        if not m:
            continue
        width = 64 if m.group(1) == "stdcx" else 32
        reg = m.group(2)
        # The CAS line follows within a few lines.
        cas_i = None
        for j in range(i + 1, min(i + 8, len(lines))):
            if CAS.match(lines[j]):
                cas_i = j
                break
            if MARK in lines[j]:
                break
        if cas_i is None:
            continue
        cm = CAS.match(lines[cas_i])
        if cm.group(5) != reg or int(cm.group(2)) != width:
            continue
        # Closest preceding larx in the same function.
        larx_i = None
        for j in range(i - 1, func_start, -1):
            lm = LARX.match(lines[j])
            if lm:
                larx_i = j
                break
        if larx_i is None:
            continue
        lm = LARX.match(lines[larx_i])
        if lm.group(2) != reg or (lm.group(1) == "ldarx") != (width == 64):
            continue
        # The larx's own assignment is "ctx.rN.u64 = __builtin_bswapNN(ctx.reserved...)": skip it, then no writes.
        ok = True
        assigned = False
        labels = []
        for j in range(larx_i + 1, i):
            if not assigned and "ctx.reserved" in lines[j] and writes(lines[j], reg):
                assigned = True
                continue
            if writes(lines[j], reg):
                ok = False
                break
            lab = LABEL.match(lines[j])
            if lab:
                labels.append(lab.group(1))
        if not ok or not assigned:
            continue
        # Every goto to those labels must sit between the larx and the stcx.
        func_end = len(lines)
        for j in range(i + 1, len(lines)):
            if FUNC.match(lines[j]):
                func_end = j
                break
        for lab in labels:
            needle = "goto %s;" % lab
            for j in range(func_start, func_end):
                if needle in lines[j] and not (larx_i < j < i):
                    ok = False
                    break
            if not ok:
                break
        if not ok:
            continue
        indent = cm.group(1)
        t = "int64_t" if width == 64 else "int32_t"
        lines[cas_i] = ("%scr0.eq = (*reinterpret_cast<volatile %s*>(REX_RAW_ADDR(ea)) == ctx.reserved.s%d);  %s\n"
                        % (indent, t, width, MARK))
        changed += 1
    return changed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gen", required=True)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    total = 0
    files = 0
    for path in sorted(glob.glob(os.path.join(a.gen, "*.cpp"))):
        with open(path, encoding="utf-8") as f:
            lines = f.readlines()
        n = process(lines)
        if n:
            files += 1
            total += n
            if not a.dry_run:
                with open(path, "w", encoding="utf-8", newline="") as f:
                    f.writelines(lines)
    print("reservation_writeback: %d write-backs elided in %d files%s" % (total, files, " (dry run)" if a.dry_run else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
