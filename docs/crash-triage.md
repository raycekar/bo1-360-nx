# Crash triage

The game must not run here, so a PC crash comes back as a log file (or a Windows Event Viewer entry). The SDK writes
crash lines, and `tools/crash_triage.py` turns them into guest functions, PPC disassembly and a fix hint.

## What the game writes

`sdk/src/system/crash_report.cpp` (since 2026-11-27, after the first test build):

- A Windows unhandled-exception filter (installed in `Runtime::Setup`) logs `[CRASH] native <code> at
  <module>+<rva>`, the access address (shown as a guest address when it is inside guest memory), the guest
  registers (`lr`, `ctr`, `r1`, `r3`-`r5`, `r31`), the guest back-chain stack (saved LR of each frame) and 24 native
  frames (`RtlVirtualUnwind`). Then it flushes the log and lets Windows Error Reporting run.
- A call to a guest address without a recompiled function (`Call to invalid or unregistered function at guest
  address 0x...`) logs the same guest lines first.

## What the build keeps (not shipped)

- `bo1zm.map` / `bo1mp.map` / `rexruntime.map` (lld-link `/map`): native RVA -> symbol (`sub_XXXXXXXX`).
- `bo1zm.pdb` / `bo1mp.pdb` (`/debug`, with `-gline-tables-only` on the generated code): native RVA -> generated
  source line -> the exact guest instruction. Generated code has one `\t// <insn>` comment per guest instruction in
  address order, with `loc_XXXXXXXX:` labels as anchors (checked against 93,530 `ctx.lr` constants).
- `out/maps/bo1zm-tested-20261127.map`: the map of the first test build (out/win of 2026-11-27, no PDB, no crash
  lines). For that build, use the Event Viewer "Fault offset" with `--rva` and this `--map`.

## Use

```
.venv/bin/python -I bo1-360-nx/tools/crash_triage.py zm --log bo1zm.log --callers
.venv/bin/python -I bo1-360-nx/tools/crash_triage.py zm --rva 0x1A2B3C --map out/maps/bo1zm-tested-20261127.map
.venv/bin/python -I bo1-360-nx/tools/crash_triage.py zm --guest 0x8247588C --invalid 0x821F1F14
```

For an invalid indirect target, it says whether the target is NULL, outside the code (a bad pointer), or inside a
function (the codegen missed an entry point) and prints the `overrides.toml` line to add. A rebuild of the exe then
needs only `codegen.sh` + `build_windows.sh` (ninja rebuilds the changed files).

Map and PDB match only the build that made them: keep a copy (`out/maps/`) of each build sent to the PC.
