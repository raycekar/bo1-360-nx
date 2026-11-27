#!/usr/bin/env bash
# Runs the code generator on bo1zm_manifest.toml -> zm/generated/default (log: zm/codegen.log).
# New unresolved tail branches: tools/codegen_fix_unresolved.py --project zm --manifest bo1zm_manifest.toml --image <default.image.bin>.
set -euo pipefail
ZM="$(cd "$(dirname "$0")" && pwd)"
BIN="${REXGLUE_BIN:-$ZM/../sdk/out/linux-amd64}"
export LD_LIBRARY_PATH="$BIN${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
(cd "$ZM" && "$BIN/rexglue" codegen --ignore-stamp bo1zm_manifest.toml > codegen.log 2>&1) || { echo "codegen failed, see zm/codegen.log" >&2; exit 1; }
tail -n 2 "$ZM/codegen.log"
