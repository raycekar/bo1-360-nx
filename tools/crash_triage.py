"""Turn a bo1zm / bo1mp crash into guest functions, disassembly and a fix hint.

Reads the "[CRASH]" lines that the SDK's crash reporter writes (sdk/src/system/crash_report.cpp) and the
"Call to invalid or unregistered function at guest address 0x..." fatal line. Addresses can also be given by hand,
for example the "Fault offset" from the Windows Event Viewer (Application Error for bo1zm.exe).

usage:
  python -I tools/crash_triage.py zm --log bo1zm.log
  python -I tools/crash_triage.py zm --rva 0x1A2B3C          # Event Viewer fault offset in bo1zm.exe
  python -I tools/crash_triage.py zm --guest 0x8247588C      # any guest address
  python -I tools/crash_triage.py zm --map out/maps/bo1zm-tested-20261127.map --rva 0x1A2B3C

Options: --callers (direct bl callers of each function), --context N (instructions around each address).
Run from anywhere; paths default to this repo layout (work/, out/build-win-<zm|mp>/, zm/generated/default/, mp/generated/default_mp/).
"""
import argparse
import bisect
import re
import struct
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ROOT = REPO.parent
BASE = 0x82000000
LLVM = Path("/mnt/workspace/tools/llvm-20.1.8/bin")
PROLOGUE = 0x7D8802A6  # mflr r12: the first instruction of most non-leaf functions

PROJECTS = {
    "zm": dict(image="work/dump_tu11/default.image.bin", gen="zm/generated/default/bo1zm_init.cpp",
               build="out/build-win-zm", exe="bo1zm.exe", map="bo1zm.map"),
    "mp": dict(image="work/dump_tu11/default_mp.image.bin", gen="mp/generated/default_mp/bo1mp_init.cpp",
               build="out/build-win-mp", exe="bo1mp.exe", map="bo1mp.map"),
}


class GuestFuncs:
    """Guest function starts from the codegen's mapping table ({ 0x82100000, sub_82100000 })."""

    def __init__(self, init_cpp):
        pairs = re.findall(r"\{ 0x([0-9A-Fa-f]{8}), (\w+) \}", Path(init_cpp).read_text())
        pairs = sorted({int(a, 16): n for a, n in pairs}.items())
        self.starts = [a for a, _ in pairs]
        self.names = [n for _, n in pairs]
        self.by_name = {n: a for a, n in pairs}

    def find(self, addr):
        i = bisect.bisect_right(self.starts, addr) - 1
        if i < 0:
            return None, 0
        return self.names[i], addr - self.starts[i]

    def is_start(self, addr):
        i = bisect.bisect_left(self.starts, addr)
        return i < len(self.starts) and self.starts[i] == addr

    def code_range(self):
        return self.starts[0], self.starts[-1] + 0x10000


class NativeMap:
    """lld-link /map: '0001:00060a90  sub_82100000  0000000140061a90  obj' -> sorted (rva, name)."""

    def __init__(self, path):
        text = Path(path).read_text(errors="replace")
        m = re.search(r"Preferred load address is ([0-9a-fA-F]+)", text)
        base = int(m.group(1), 16) if m else 0x140000000
        rows = {}
        for m in re.finditer(r"^ 0001:[0-9a-f]+\s+(\S+)\s+([0-9a-f]{16})\s", text, re.M):
            name, va = m.group(1), int(m.group(2), 16)
            if name.startswith("__imp_"):
                continue
            rows.setdefault(va - base, name)
        self.rvas = sorted(rows)
        self.names = [rows[r] for r in self.rvas]
        self.path = path

    def find(self, rva):
        i = bisect.bisect_right(self.rvas, rva) - 1
        return (self.names[i], rva - self.rvas[i]) if i >= 0 else (None, 0)


def symbolize(exe, rvas):
    """llvm-symbolizer with the build's PDB: rva -> (cpp path, line) or None."""
    if not rvas or not exe.with_suffix(".pdb").exists():
        return {}
    inp = "".join(f"0x{r:X}\n" for r in rvas)
    out = subprocess.run([str(LLVM / "llvm-symbolizer"), f"--obj={exe}", "--relative-address", "--no-inlines"],
                         input=inp, capture_output=True, text=True).stdout
    res = {}
    for r, block in zip(rvas, out.strip().split("\n\n")):
        lines = block.splitlines()
        if len(lines) >= 2:
            m = re.match(r"(.*):(\d+):\d+$", lines[1])
            if m and int(m.group(2)) > 0:
                res[r] = (m.group(1), int(m.group(2)))
    return res


def guest_from_source(path, line_no, funcs):
    """Generated code has one '\t// <ppc insn>' comment per guest instruction, in address order, with 'loc_X:'
    labels as anchors. The guest instruction of a source line is the last comment at or before it."""
    cur = last = None
    with open(path, errors="replace") as f:
        for i, line in enumerate(f, 1):
            if i > line_no:
                break
            m = re.match(r"DEFINE_REX_FUNC\((\w+)\)", line)
            if m:
                cur, last = funcs.by_name.get(m.group(1)), None
                continue
            m = re.match(r"loc_([0-9A-F]{8}):", line)
            if m:
                cur = int(m.group(1), 16)
                continue
            if line.startswith("\t// ") and cur is not None:
                last, cur = cur, cur + 4
    return last


class Image:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        try:
            import capstone
            self.md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
        except ImportError:
            self.md = None

    def word(self, ea):
        off = ea - BASE
        if 0 <= off <= len(self.data) - 4:
            return struct.unpack_from(">I", self.data, off)[0]
        return None

    def dis(self, ea):
        w = self.word(ea)
        if w is None:
            return "(outside image)"
        ins = next(self.md.disasm(struct.pack(">I", w), ea), None) if self.md else None
        return f"{ins.mnemonic} {ins.op_str}" if ins else f".long 0x{w:08X}"

    def words_equal(self, value, limit=12):
        """Addresses of aligned big-endian words equal to value (function pointer tables, vtables)."""
        needle = struct.pack(">I", value)
        out, i = [], self.data.find(needle)
        while i != -1 and len(out) < limit:
            if i % 4 == 0:
                out.append(BASE + i)
            i = self.data.find(needle, i + 1)
        return out

    def bl_callers(self, target, lo, hi, limit=12):
        out = []
        n = (hi - lo) // 4
        words = struct.unpack_from(f">{n}I", self.data, lo - BASE)
        for k, w in enumerate(words):
            if (w >> 26) == 18 and (w & 3) == 1:  # bl
                li = w & 0x03FFFFFC
                if li & 0x02000000:
                    li -= 0x04000000
                if lo + 4 * k + li == target:
                    out.append(lo + 4 * k)
                    if len(out) >= limit:
                        break
        return out


def show_guest(addr, label, funcs, img, args, is_return=False):
    name, off = funcs.find(addr)
    where = f"{name}+0x{off:X}" if name else "(before the first function)"
    print(f"\n{label}: 0x{addr:08X} = {where}")
    if img.word(addr) is None:
        print("   not inside the image: a bad pointer or a data address")
        return
    center = addr - 4 if is_return else addr
    for ea in range(center - 4 * args.context, center + 4 * (args.context + 1), 4):
        mark = ">>" if ea == center else "  "
        print(f"   {mark} {ea:08X}: {img.dis(ea)}")
    if args.callers and name and name.startswith("sub_"):
        start = addr - off
        lo, hi = funcs.code_range()
        hi = min(hi, BASE + len(img.data))
        callers = img.bl_callers(start, lo, hi)
        print(f"   callers of {name}: " + (", ".join(f"{c:08X} ({funcs.find(c)[0]})" for c in callers) or "none by bl"))


def show_invalid_target(target, funcs, img):
    print(f"\nINVALID INDIRECT CALL TARGET 0x{target:08X}")
    lo, hi = funcs.code_range()
    w = img.word(target)
    if target == 0:
        print("   target is NULL: an unset function pointer (a missing init, or a stub that returned 0)")
        return
    if w is None or not (lo <= target < hi):
        print("   target is outside the code: a bad pointer (data corruption, or a wrong struct offset)")
        return
    name, off = funcs.find(target)
    print(f"   inside {name}+0x{off:X}; first instructions:")
    for ea in range(target, target + 16, 4):
        print(f"      {ea:08X}: {img.dis(ea)}")
    refs = img.words_equal(target)
    if refs:
        print("   stored as a pointer at: " + ", ".join(f"{r:08X}" for r in refs))
    hint = "it starts with mflr r12 (a real prologue)" if w == PROLOGUE else "check that it is a real entry"
    print(f"   FIX: the codegen did not make a function here ({hint}). Add to <project>/overrides.toml [functions]:")
    print(f'      "0x{target:08X}" = {{ }}')
    print("   then run codegen.sh and build_windows.sh (only the changed files rebuild).")


def parse_log(path):
    """Returns (invalid_targets, native_entries, guest_entries). Entries: (label, value[, module])."""
    invalid, native, guest = [], [], []
    for line in Path(path).read_text(errors="replace").splitlines():
        m = re.search(r"invalid or unregistered function at guest address 0x([0-9A-Fa-f]+)", line)
        if m:
            invalid.append(int(m.group(1), 16))
        if "[CRASH]" not in line:
            continue
        body = line[line.index("[CRASH]") + 7:].strip()
        m = re.match(r"native (0x[0-9A-F]+) at (\S+?)\+0x([0-9A-Fa-f]+)", body)
        if m:
            native.append((f"native fault {m.group(1)}", int(m.group(3), 16), m.group(2)))
            g = re.search(r"= guest 0x([0-9A-Fa-f]+)", body)
            print(f"log: {body}")
            if g:
                print(f"     (the faulting access was to guest memory 0x{g.group(1)})")
            continue
        m = re.match(r"native frame (\d+): (\S+?)\+0x([0-9A-Fa-f]+)", body)
        if m:
            native.append((f"native frame {m.group(1)}", int(m.group(3), 16), m.group(2)))
            continue
        m = re.match(r"guest lr=0x([0-9A-Fa-f]+)", body)
        if m:
            print(f"log: {body}")
            guest.append(("guest lr (last call's return address)", int(m.group(1), 16), True))
            c = re.search(r"ctr=0x([0-9A-Fa-f]+)", body)
            if c and int(c.group(1), 16) >= BASE:
                guest.append(("guest ctr (last indirect branch target)", int(c.group(1), 16), False))
            continue
        m = re.match(r"guest frame (\d+): 0x([0-9A-Fa-f]+)", body)
        if m:
            guest.append((f"guest frame {m.group(1)} (return address)", int(m.group(2), 16), True))
    return invalid, native, guest


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("project", choices=sorted(PROJECTS))
    ap.add_argument("--log", help="the game's log file")
    ap.add_argument("--rva", action="append", default=[], help="native offset in the exe (hex), repeatable")
    ap.add_argument("--guest", action="append", default=[], help="guest address (hex), repeatable")
    ap.add_argument("--invalid", action="append", default=[], help="invalid indirect call target (hex)")
    ap.add_argument("--map", help="exe map (default: <build>/<exe>.map; use out/maps/... for an older build)")
    ap.add_argument("--dll-map", help="rexruntime.map (default: <build>/rexruntime.map)")
    ap.add_argument("--callers", action="store_true")
    ap.add_argument("--context", type=int, default=3)
    args = ap.parse_args()
    p = PROJECTS[args.project]

    funcs = GuestFuncs(REPO / p["gen"])
    img = Image(ROOT / p["image"])
    maps = {}
    exe_map = Path(args.map) if args.map else ROOT / p["build"] / p["map"]
    dll_map = Path(args.dll_map) if args.dll_map else ROOT / p["build"] / "rexruntime.map"
    for module, path in ((p["exe"], exe_map), ("rexruntime.dll", dll_map)):
        if path.exists():
            maps[module.lower()] = NativeMap(path)
    print(f"guest functions: {len(funcs.starts)}; maps: " + (", ".join(f"{k} <- {v.path}" for k, v in maps.items())
                                                            or "none"))

    invalid, native, guest = parse_log(args.log) if args.log else ([], [], [])
    invalid += [int(x, 16) for x in args.invalid]
    native += [("native offset", int(x, 16), p["exe"]) for x in args.rva]
    guest += [("guest address", int(x, 16), False) for x in args.guest]
    if not (invalid or native or guest):
        print("nothing to look up: give --log, --rva, --guest or --invalid", file=sys.stderr)
        return 1

    for t in invalid:
        show_invalid_target(t, funcs, img)
    exe = ROOT / p["build"] / p["exe"]
    lines = symbolize(exe, [r for _, r, mod in native if mod.lower() == p["exe"].lower()]) if not args.map else {}
    for label, rva, module in native:
        m = maps.get(module.lower())
        if not m:
            print(f"\n{label}: {module}+0x{rva:X} (no map for {module})")
            continue
        sym, off = m.find(rva)
        print(f"\n{label}: {module}+0x{rva:X} = {sym}+0x{off:X}")
        g = re.fullmatch(r"(?:__imp__)?sub_([0-9A-F]{8})", sym or "")
        src = lines.get(rva)
        ins = guest_from_source(*src, funcs) if src and "_recomp." in src[0] else None
        if src:
            print(f"   source {Path(src[0]).name}:{src[1]}")
        if ins is not None:
            show_guest(ins, "   = guest instruction", funcs, img, args)
        elif g:
            print(f"   = recompiled guest function 0x{g.group(1)} (no PDB line: the fault is somewhere in its code)")
            show_guest(int(g.group(1), 16), "   guest function start", funcs, img, args)
    for label, addr, is_return in guest:
        show_guest(addr, label, funcs, img, args, is_return)
    return 0


if __name__ == "__main__":
    sys.exit(main())
