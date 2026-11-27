#!/usr/bin/env bash
# Runs a command at low priority with hard limits, so a heavy job (codegen, clang) cannot take over the host:
# nice 19 + idle I/O, inside a systemd scope with CPUQuota (default 100% of one core; the LXC itself is capped at 80% by Proxmox) and MemoryMax (default 4G).
# A runaway process is killed alone at the memory limit. Override: BO1_CPU_QUOTA=80% BO1_MEM_MAX=6G.
# Without systemd-run (for example in a container that lacks it), only nice/ionice apply.
set -euo pipefail
CPU="${BO1_CPU_QUOTA:-100%}"
MEM="${BO1_MEM_MAX:-4G}"
if command -v systemd-run >/dev/null 2>&1 && systemd-run --scope -q true >/dev/null 2>&1; then
  exec systemd-run --scope -q -p "CPUQuota=$CPU" -p "MemoryMax=$MEM" -p MemorySwapMax=0 \
    nice -n 19 ionice -c3 "$@"
fi
echo "lowprio: systemd-run not usable, nice/ionice only" >&2
exec nice -n 19 ionice -c3 "$@"
