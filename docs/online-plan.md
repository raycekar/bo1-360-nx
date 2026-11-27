# Online co-op: research and plan

Goal: play BO1 Zombies (and later Multiplayer) with friends over the internet, the way bo2-360-nx does: an ordinary
**System Link** game, made reachable over the internet with an 8-character join code.

## What BO1 does (TU11, both executables)

- Classic IW-engine networking: `getinfo` broadcasts, `infoResponse` replies, `xnaddr` in the connect string, and
  `net_port` / `net_ip` dvars. The menus have a System Link lobby (`menu_systemlink_lobby`), and the party code has
  `party_host`, `party_hostname`, `party_connectToOthers` and `party_connectTimeout`.
- `net_port` is registered with **default 1000**, range 0..65535 (Zombies `0x823ACE4C`, MP `0x824091D4`), the same
  as BO2. Discovery broadcasts go out from the game's System Link socket.
- Zombies System Link existed on the 360, so no Live-only gate is expected. `party_maxplayers` is 4 in Zombies.

## What the SDK already does (no game code needed)

From bo2-360-nx's SDK, which this repo uses (`sdk/src/system/xsocket.cpp`, `online_link.cpp`, `online_relay.cpp`):

- **System Link on a LAN**: each PC gets an XNADDR built from its LAN address and its own XUID. Guest ports below
  1024 move up by `net_system_port_offset` (30000), so BO1 uses **UDP 31000** (and the ones next to it).
- **Internet**: `rex::system::online::Start(Role::kHost | kJoin, ip)` prepares a session in the background:
  1. UPnP port forwarding
  2. a STUN public address
  3. hole punching through public MQTT brokers
  4. manual forwarding of UDP 31000-31002 as the last resort

  While online, XNADDRs carry the public address, and a joiner's System Link search broadcast also goes to the host
  of the join code (`xsocket.cpp` ~604). So **the game's own System Link lobby finds the remote host**.
- Join code: 8 Crockford base32 characters (the host's public IPv4 address and a CRC-8), `rex/system/join_code.h`.

## What BO1 still needs (the game side)

bo2-360-nx drives this from new pages in BO2's Lua menus. BO1's menus are `.menu` files, and editing them is more
work, so the plan uses the port's own UI:

1. **Settings, first version (done, `common/bo1_online.cpp`, both games)**: `bo1_online = "host"` or
   `bo1_online = "XXXX-XXXX"` in `bo1zm.toml` / `bo1mp.toml`. `OnPostSetup` calls `online::Start`; a status window
   (top left, no input) shows the state, the router result and, for the host, the join code, and closes 30 s after
   the session is ready (a dialog that stays open makes the presenter paint from the UI thread: see rex_app.cpp).
   The log has the same lines (`bo1 online: ...`, `JOIN CODE ...`). `OnShutdown` removes the UPnP forwarding.
   The players then use the game's normal **System Link** menu.
2. **Then an in-game panel**: an ImGui window from the SDK (`OnCreateDialogs`), opened by a key, with Host / Join /
   Stop, the code, and the checklist that `online::Current()` reports (router, public address, relay peers).
3. Optional: menu items in the System Link lobby (patch the `.menu` assets after load, as the script patcher does for
   GSC).

## Game-side findings (static, 2026-11-27)

- Zombies System Link runs `xsignin ; systemlink 1 ; splitscreen 0 ; party_maxlocalplayers 2` and opens
  `menu_systemlink_lobby` (`xstartpartyhost`, start with `xpartygo`). With the 4-player hook, `party_maxlocalplayers`
  stays at the port's value, so a PC can bring 2+ local players into a System Link lobby; `party_maxplayers` (4) caps
  the total (free-slot code `sub_8232E1B8` / `sub_8232E268`).
- The online menus run `xsigninlivenoguests`: Xbox Live play is not a target; System Link is.

## Risks and checks

- The order on both sides: the host starts online mode **before** it opens the System Link lobby. The joiner starts
  it before searching.
- Cross-play with bo2-360-nx is not possible (a different game).
- The per-console XUIDs come from `user\console_id.txt`, a random id made on the first start (`kernel_state.cpp`
  ConsoleId). The sync never copies it (it is not in the manifest), so each PC gets its own. Copying a whole game
  folder from one PC to another would copy it too: delete it on the second PC.
- Test order: first two PCs on one LAN (System Link only), then over the internet with the join code.

## Network imports and stubs (static, 2026-11-27)

ZM imports 20 `NetDll_*` functions. All are real in the SDK except three stubs. A stub leaves r3 as it is, and the
game's wrappers (`0x8252CB88`..`0x8252CCD0`) pass `caller = 1` in r3, so a stub "returns" 1 (`ERROR_INVALID_FUNCTION`).

| Import | Caller | What the game does with a stub | Fix |
|---|---|---|---|
| `XNetServerToInAddr` | `0x821F1880` | checks `== 0`, takes the failure path | none needed |
| `XNetQosGetListenStats` | `0x82461944` (before `XNetQosListen`) | ignores the result; the stats stay unset | zero the counters, return 0 |
| `XNetQosLookup` | `0x8247385C` (party probe; checks the result) and `0x82314F44` (ZM `xstart`) | `xstart` ignores the result, reads `*ppxnqos` (stack garbage) and polls `count_pending` | allocate an `XNQOS`: every target COMPLETE, not contacted; set the event; return 0 |

`XNetQosLookup` has 13 arguments; the last 5 are on the guest stack at `r1 + 0x54 + 8*(i-8)` (checked in the wrapper
`0x8252CC30`, which copies them from its caller's frame).

## Zombies <-> Multiplayer switch

On the 360 the menus relaunch the other executable: ZM calls `XamLoaderLaunchTitle("default_mp.xex")` at
`0x823D6754` (menu path) and in `xstart` (`0x82314FF8`, after the QoS lookup above); MP launches `default.xex`
(`0x82242DDC`, `0x82574110`). The SDK only stored the path and ended the title, so the game stopped with an open
window. Fix: SDK cvar `launch_title_map` (`default_mp.xex=bo1mp.exe;default.xex=bo1zm.exe`, set by both apps) starts the
other PC exe from the same folder and ends this process. Launch data (party state) is not carried over.
