# PC port notes (2026-11-27)

What runs, the settings the port adds, and what was learned on the way. Hardware for the numbers: Ryzen 7 5800H,
RTX 3060 Laptop, Windows 11, 1280x720 windowed.

## What runs

| | Result |
|---|---|
| Zombies solo, every map | ~57-60 fps with zombies (Kino, Five, Nacht, Verrückt, Shi No Numa, Der Riese, Ascension, Call of the Dead, Shangri-La, Moon) |
| Zombies 3 players (Kino) | ~50 fps |
| Zombies 4 players (Kino) | ~43-45 fps |
| Multiplayer solo (Array) | 58-60 fps |
| Multiplayer 4 players | ~25 fps |
| System Link, two copies on one PC | Zombies host + joiner in the same match |
| Not tested | internet play with a join code; two PCs |

## Settings the port adds (`bo1zm.toml` / `--name=value`)

| Setting | Default | What it does |
|---|---|---|
| `bo1zm_local_players` | 4 | Split-screen players in Zombies (2-4) |
| `bo1zm_pmem_extra_mb` | 48 | Extra MB for the game's 427 MB physical-memory pool (4 players on Kino ran 576 KB short) |
| `bo1zm_graphics_preset` | 360 | lowest / low / 360 / high / ultra (below) |
| `bo1zm_god` | false | God mode (script patch of player_damage_override) |
| `bo1zm_splitscreen_fog` | true | false = full view distance in split screen |
| `show_shader_progress` | true | "BUILDING SHADERS" at the top while pipelines are built |
| `readback_memexport` | false (app default) | CPU readback of shader memexport; on cost a GPU sync per draw |
| `texture_cache_memory_limit_*` | 128 / 1024 / 2048 MB | Render-to-texture / soft / hard (app defaults) |
| `d3d12_adapter` | -1 | -1 now prefers the high-performance GPU (hybrid laptops ran on the iGPU before) |
| `net_lan_address`, `net_system_link_peers` | "" | Two copies on one PC: 127.0.0.1 / 127.0.0.2, each other as peers |
| `kernel_poll_yield` | false | SwitchToThread after an empty zero-timeout wait (BO1 polls ~200,000 times/s) |

Graphics presets (the game's own dvars; the engine caps dynamic and shadow-casting lights at 4 and the LOD scale at
1, so "above the 360" means detail and sharpness, not more lights):

| Preset | Shadows | Dynamic lights | Models | DOF |
|---|---|---|---|---|
| lowest | off (`sm_enable 0`) | 0 | LOD scale 2 | off |
| low | 1 light | 2 | LOD scale 1.5 | off |
| 360 | untouched | | | |
| high | full-res spot shadows | radius 300 | LOD bias -200 | |
| ultra | full-res spot shadows | radius 400 | LOD bias -500 | |

Lower presets do not raise 4-player fps on this PC (~44 with all): the GPU command thread is the limit, not shadows.

## Speed: what mattered

1. Hybrid laptops: DXGI listed the iGPU first; `d3d12_adapter = -1` now asks for the high-performance GPU.
2. BO1's job system polls an event with a zero timeout ~200,000 times a second; each poll also did SwitchToThread.
   Not yielding (`kernel_poll_yield = false`) took solo Kino to 60. (A lock-free "poll hint" that skipped the host
   wait broke rendering: some signal path bypasses XEvent::Set; `kernel_fast_event_poll` stays off.)
3. Memexport readback fell back to a full GPU sync per draw (~800/s with zombies): off for BO1.
4. 1 ms timer resolution and no power throttling for the process.

Tried without gain: back-off in the job workers, eliding the reservation write-back of lwarx/stwcx (tools/reservation_writeback.py),
D3D12 submit-on-primary-buffer-end off, lower graphics presets for 4 players.

### 4 players

At 4 views the GPU command processor thread (t3) is ~95 % busy; the GPU itself 50-65 %. The guest main thread and
job workers only look busy: blocked jobs are re-queued ~2.7 million times a second while they wait for it. t3's time:
NVIDIA driver ~23 %, WriteRegister ~10 %, UpdateBindings ~5 %, NtProtectVirtualMemory ~4 % (~3,600 write-watch faults
a second), per-draw heap allocations ~3 %, shader lookups ~3 %, texture lookups ~4 %. 60 fps needs ~1/3 less t3 time.

## Other fixes

- 4-player "Out of memory": BO1's PMem pool (`bo1zm_memory.cpp`).
- XamLoaderLaunchTitle (the 360 relaunches the title to start a split-screen match and after a fatal error) keeps the
  command line and hands over the launch data (`launch_data_file`).
- System Link on one PC (`net_lan_address`, `net_system_link_peers`).
- "64.0.-1" in the menu corner is the offline `ffotdversion` (the Live patch version): authentic, left as is.

## Diagnostics (all off by default unless noted)

| Setting / log | What it shows |
|---|---|
| `[frames]`, `[hitch]` (on) | fps every 10 s; frames of 100 ms or more |
| `log_host_profile` (+ `_lines`, `_thread`) | hottest host functions per thread (PDBs), busy share per thread, `[hitch-prof]` per slow frame |
| `log_wait_profile` | where guest threads block, with caller and caller's caller |
| `log_job_counts` | job queue push / pop / take-job counts, sampled push callers |
| `[gpu-alloc]` (on) | D3D12 resource creations per call site |
| `[gpu-wait]` (on) | CPU waits on the GPU of 10 ms or more, with the caller offset |
| `[watch]` (on) | write-watch access-violation faults per 10 s |
| `[waitout]`, `[sleep]` (on) | guest waits that time out after >= 50 ms; guest sleeps >= 50 ms |
| `[Com_Error]`, `[Sys_Error]`, `[OutOfMemory]` (on) | engine errors with callers (the 360 relaunch hides them otherwise) |
| `net_system_link_trace`, `net_system_link_dump` | System Link packets (the dump with their first bytes) |

## Work in progress

In-game Options/Cheats pages: see [menus-research.md](menus-research.md).
