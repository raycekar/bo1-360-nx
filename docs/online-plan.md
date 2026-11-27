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

1. **Settings, for a first version** (no UI work): `bo1zm_online = "host"` or `bo1zm_online = "XXXX-XXXX"` in
   `bo1zm.toml`. The app calls `online::Start` at boot. The log (and a one-line ImGui overlay) shows the state and,
   for the host, the join code to send to friends. The players then use the game's normal **System Link** menu.
2. **Then an in-game panel**: an ImGui window from the SDK (`OnCreateDialogs`), opened by a key, with Host / Join /
   Stop, the code, and the checklist that `online::Current()` reports (router, public address, relay peers).
3. Optional: menu items in the System Link lobby (patch the `.menu` assets after load, as the script patcher does for
   GSC).

## Risks and checks

- The order on both sides: the host starts online mode **before** it opens the System Link lobby. The joiner starts
  it before searching.
- Cross-play with bo2-360-nx is not possible (a different game).
- The per-console XUIDs come from `user\console_id.txt`, a random id made on the first start (`kernel_state.cpp`
  ConsoleId). The sync never copies it (it is not in the manifest), so each PC gets its own. Copying a whole game
  folder from one PC to another would copy it too: delete it on the second PC.
- Test order: first two PCs on one LAN (System Link only), then over the internet with the join code.
