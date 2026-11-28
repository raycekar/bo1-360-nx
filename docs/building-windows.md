# Building and testing on Windows (native)

The Linux cross-build ([building.md](building.md)) still works; this page is the native Windows setup used since
2026-11-27. A full build takes ~3 minutes on an 8-core laptop, an incremental one 5-60 s.

## Layout

```
<game>\                   game\  update\  user\  logs\   (the PC game folder, see building.md step 6)
<game>\bo1zm.exe ...      deployed here by build.ps1 -Deploy
<game>\dev\               "<dev>"
<dev>\bo1-360-nx\         this repo
<dev>\work\               load_tu11\ (default.xex, default_mp.xex), dump_tu11\, ff\, scripts\   (game-derived)
<dev>\tools\llvm\         LLVM/Clang (20.x or newer; 23.1.5 used)
<dev>\tools\cmake\        CMake 4.x
<dev>\tools\ninja.exe
<dev>\tools\xwin-sdk\     MSVC CRT + Windows SDK splat (see building.md "Tools"); copy it from a Linux
                          splat with every case variant of the file names (Windows keeps one per name)
<dev>\out\build-win-zm\   build folders
```

`<dev>\bo1-360-nx\zm\generated\default` (and `mp\generated\default_mp`) come from codegen (building.md step 4); the
code generator still runs on Linux.

## Build

```
.\tools\win\build.ps1 zm -Deploy        # Zombies; -Deploy closes a running game and copies exe, DLLs and PDBs
.\tools\win\build.ps1 mp -Deploy        # Multiplayer
  -Jobs N  -March x86-64-v3|x86-64-v2  -Reconfigure  -Game <folder>
```

PDBs (line tables) are built for the exe and the DLLs, so the host profiler and crash tools can name functions.

## Run and drive the game without a person

```
.\tools\win\run.ps1 zm -Pads 4 [-Pipe bo1ctl] [-Extra '--cvar=value', ...] [-Keep]
```

`-Pads N` adds N virtual controllers (`input_script_pads`); they are players 1..N, ahead of any real pad. Each copy
listens on `\\.\pipe\<Pipe>`. `tools\win\bo1ctl.py` talks to it:

```
py -I tools\win\bo1ctl.py [--pipe NAME] press <pad> A|B|X|Y|START|BACK|UP|DOWN|LEFT|RIGHT|LB|RB|LS|RS[+...] [ms]
                                         stick <pad> L|R <x> <y> <ms>      trig <pad> L|R <ms>
                                         run <script.txt>                   raw <pipe command>
```

Pipe commands (`raw ...`):

| Command | What it does |
|---|---|
| `pad <i> <hex buttons> <lt> <rt> <lx> <ly> <rx> <ry>` | Set a virtual pad |
| `shot <file.png>` | Save the last game frame (works with the desktop locked; overlays not included) |
| `cmd <console line>` | Run a game console line on the main thread (Zombies) |
| `dvar <name>` | A dvar's value, reset value and flags (Zombies) |
| `find <text>` / `findptr <hex>` / `peek <hex> <n>` | Search and dump guest memory (Zombies, research) |
| `ping` | `pong` |

Test scripts in `tools\win\tests\`: `zm_solo_kino.txt` (launch to a Kino match), `zm_move_shoot.txt`, `zm_3p_kino.txt`,
`zm_4p_kino.txt`, `zm_systemlink.txt`, `mp_split.txt`, and PowerShell drivers `measure_4p.ps1` (4-player frame
rate), `zm_map_tour.ps1` (every Zombies map, god mode, 5 frames each), `zm_link_two.ps1` (two copies over System
Link on one PC).

The first start with a new user folder shows the safe-area / brightness wizard; the scripts expect it done.
