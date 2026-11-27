# Building (Linux host, Windows output)

Everything runs on a Linux x86-64 machine; the result runs on Windows x64. Paths assume the layout
`<work>/bo1-360-nx` (this repo), `<work>/data`, `<work>/work`, `<work>/out`. Nothing game-derived goes into the repo.

## Tools
- LLVM/Clang 20, CMake 4.x, Ninja (`BO1_TOOLS` folder with `llvm-20.1.8/`, `cmake-4.5.1/`, `ninja-1.13.1/`).
- Linux build deps for the host tools: see the header of `tools/build_host_linux.sh`.
- Windows CRT + SDK for the cross-build (you accept Microsoft's license): `xwin --accept-license splat` downloads
  them; xwin 0.10.0's own splat crashed here, so lay them out with `msiextract` (msitools) + `tools/xwin_splat.py`:
  ```
  xwin --accept-license --arch x86_64 --cache-dir <cache> splat --output <tmp>   # downloads; may crash at the end
  (cd <cache>/msix/<pkg> && msiextract <cache>/dl/<pkg>.msi)                     # each Win11SDK_*.msi and ucrt.msi, cabs beside it
  python3 tools/xwin_splat.py --msix=<cache>/msix --dl=<cache>/dl - <xwin-sdk> sdk/src sdk/include sdk/thirdparty
  ```
- Python venv with `capstone` for the inspect tools.

## Steps
1. Game files: `tools/xiso_extract.py <disc.iso> data/game`; `tools/inspect/stfs_extract.py <TU11 package> --out
   data/update`; DLC: `tools/install_dlc.py <folder of packages> --out data/content --all`.
2. Host tools: `tools/build_host_linux.sh` (rexglue + xexdump).
3. Decrypt (optional, for research): `xexdump work/load_tu11 default.xex work/dump_tu11` with `default.xex` and
   TU11's `default.xexp` side by side in `work/load_tu11`.
4. C++: `zm/codegen.sh` (manifest `zm/bo1zm_manifest.toml`, overrides `zm/overrides.toml`).
5. Windows build: `JOBS=2 zm/build_windows.sh` -> `out/build-win-zm/bo1zm.exe`, `rexruntime.dll`, `rexgpu-xenos.dll`.
6. PC folder: `tools/package_win.py --build out/build-win-zm --data data --out out/win` (game data hard-linked,
   `manifest.json` with SHA-256 of each file).
7. Test queue: add `--build out/build-win-mp --queue NN-name --notes "what to test"`. Only the port (~80 MB: exes,
   DLLs, toml, TEST.txt) goes to `out/win/builds/NN-name/`; the exe uses `game\`, `update\`, `user\` two levels up
   (`common/bo1_paths.h`). The top-level build stays as it is. `.map`/`.pdb` go to `out/maps/NN-name/`. To drop a
   tested build, move its folder out and package again: the sync removes it on the PC.

## On the PC
Copy `out/win/sync.cmd` to any folder, set `SOURCE` in it to the network path of `out/win`, run it. The first run
copies everything (~10 GB), later runs only what changed; `bo1zm.toml` is copied once and then left alone. Needs the
Microsoft Visual C++ 2015-2022 x64 runtime. Start `bo1zm.exe`.

Settings in `bo1zm.toml`: `bo1zm_local_players = 4` (2-4; 2 keeps the 360 limit), folder overrides.
