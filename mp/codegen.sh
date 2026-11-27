#!/usr/bin/env bash
# Runs the code generator on bo1mp_manifest.toml -> mp/generated/default_mp (log: mp/codegen.log).
# New unresolved tail branches: tools/codegen_fix_unresolved.py --project zm --manifest bo1mp_manifest.toml --image <default.image.bin>.
set -euo pipefail
MP="$(cd "$(dirname "$0")" && pwd)"
BIN="${REXGLUE_BIN:-$MP/../sdk/out/linux-amd64}"
export LD_LIBRARY_PATH="$BIN${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
(cd "$MP" && "$BIN/rexglue" codegen --ignore-stamp bo1mp_manifest.toml > codegen.log 2>&1) || { echo "codegen failed, see mp/codegen.log" >&2; exit 1; }
tail -n 2 "$MP/codegen.log"
