# Four-player split-screen Zombies: research

Static study of Black Ops (Xbox 360) **TU11** (`default.xex` 0.0.11.3 for Zombies, `default_mp.xex` for
comparison). Addresses are guest addresses in the TU11 images made by `xexdump`. Nothing has been run yet.

## Verdict: feasible, and the limit is in the menus

On the 360, BO1 Zombies allows 2 local players. The engine in `default.xex` is built for **4**, the same as
Multiplayer. The front-end menus pick 2.

## Evidence

### The engine is 4-wide

| What | Where (TU11 `default.xex`) | Finding |
|---|---|---|
| Script builtin `getmaxlocalclients` | `0x82195470` | `li r3, 4`: returns 4 (MP `0x821F1D88`: also 4) |
| Local-client count loops | `0x8232E0E0`, `0x8232E140` | loop `i < 4` over local client slots |
| Free local slots for the party | `0x8232E1B8` | `min(party_maxlocalplayers - used, ...)`, total capped at a hard-coded 4 |
| Per-local-client memory (`PerLocalClient Memory`) | `0x823D7F88` | sized `count * 0x2312D8` (~2.2 MB each); count forced to **4** when the `splitscreen` dvar (or a second flag at `0x836286B8`) is set |
| Renderer viewports | `r_num_viewports` registered at `0x82580A34` | int, range 0..4; `0x82575038` picks an EDRAM tiling setup per viewport count |

### The limit is a dvar

| Dvar | Registered | Default / min / max |
|---|---|---|
| `party_maxlocalplayers` | `0x823E4E68` (pointer `0x83649AD0`) | **2** / 1 / **4** (MP: same) |
| `party_maxplayers` | `0x823E4E3C` (pointer `0x83649A58`) | 4 / 1 / ... |
| `splitscreen_playerCount` | `0x823D86D8` | 1 / 1 / 4 |
| `splitscreen` (bool) | pointer `0x83633374` | |

### The menus set it to 2

Fastfiles decoded with `tools/inspect/ff_decode_t5.py` (BO1 key from `t5_zone_key.py`):

- **Zombies front end** (`frontend.ff`): each play path (split screen, System Link, private match, online) runs
  `exec "party_maxlocalplayers 2"`. The split-screen path is `splitscreen 1 ; systemlink 0 ; party_maxlocalplayers 2`.
- **Multiplayer** (`ui_mp.ff`): its split-screen path is `splitscreen 1 ; party_maxlocalplayers 4 ; exec
  default_splitscreen.cfg`.

The two flows are the same apart from that value.

### The Zombies scripts handle N local players

All 659 `.gsc` and 145 `.csc` scripts were extracted from the Zombies zones, the DLC zones and TU11 (`zone_scripts.py`).

- The client scripts loop over `getlocalplayers().size`. The few `== 1` checks are single-player special cases.
- The server scripts use `IsSplitScreen()` to lower fog and cull distances and to move HUD text for a top/bottom
  split. With 4 quadrants, expect HUD positions that need small fixes. They are polish, not blockers.
- The `players.size == 2` branches are the normal 1-4 player online logic (score, weapon costs, AI).

## Plan for the port

1. **Force 4 local players** in Zombies split screen and System Link. Use one of these:
   - **Hook** (preferred): in the dvar set path, when the value 2 is written to `party_maxlocalplayers` outside
     ranked online play, write 4 instead. The port setting is `bo1_zm_local_players` (2-4, default 4).
   - **Fallback**: patch the loaded menu strings in memory after the zone loads. `party_maxlocalplayers 2` becomes
     `... 4`, which is the same length, so no layout changes.
2. **Input and profiles**: four controllers, each with its own offline profile. Port this from bo2-360-nx
   (`bo2zm_controllers.cpp` and the SDK's profile code).
3. **Check at run time on the PC**:
   - the 3-way and 4-way viewport layout
   - the per-client memory at 4
   - the texture streaming at 4 views
   - the speed of the Xenos renderer at 4 views
   - the HUD in quadrants
4. **Polish**: fix the HUD offsets in the split-screen script paths, with a GSC loader or a script patch at run
   time. Optional: a `default_splitscreen.cfg`-style HUD scale set, as in MP.

## Open questions (need a running build)

- Does the Zombies client lay out 3 and 4 views itself? The engine values point to yes, but no code path has been
  run yet.
- How fast is it at 4 views with the exact Xenos renderer? bo2-360-nx needed a native renderer for speed on the
  Switch. A PC should run the Xenos renderer at 4 views.
- Fixed pools that scale with the player count, for example HUD elements (`level.hudelem_count`) and client-side
  entities.

## How to reproduce

```
xexdump work/load_tu11 default.xex work/dump_tu11           # default.xex + TU11 default.xexp side by side
python -I tools/inspect/t5_zone_key.py work/dump_tu11/default.image.bin <any.ff> > key.txt
python -I tools/inspect/ff_decode_t5.py frontend.ff --key-file key.txt --out work/ff --list 0 --no-cache
python -I tools/inspect/zone_scripts.py work/scripts work/ff/<zones...>
```

## Kernel and XAM imports (Zombies, TU11)

`tools/inspect/import_audit.py work/dump_tu11/default.summary.json`: 278 imports (99 xam, 179 xboxkrnl), **none
missing**. 237 are implemented and 12 are kernel variables the SDK provides. The 29 stubs are friends / marketplace /
gamer card UI, network QoS and voice, unused I/O manager calls, `ObIsTitleObject` / `ObReferenceObject`,
`RtlUpcaseUnicodeChar`, the HMAC-SHA trio and `XeKeysAesCbc`.

- **XeCryptHmacShaInit/Update/Final** are now real (SDK `xboxkrnl_crypt.cpp`, checked against RFC 2202). BO1 calls
  them from `sub_82551D68`, reached from a "challenge" routine (`sub_8245A378`), probably online challenge/response.
- **XeKeysAesCbc** stays a no-op: it encrypts with the console's own key, which no PC has. Encrypt and decrypt are
  then both identity, so the result is consistent on one machine.
- Split screen: the SDK made 1 offline profile by default. The app now sets `offline_user_count = 4`, as bo2-360-nx
  does. Input maps physical pad N to guest user N (SlotAssignment); keyboard and mouse join user 0.
