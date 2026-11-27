# DLC on PC (static check, 2026-11-27)

Nothing here has been run yet. It is what the TU11 code and the SDK show, and what to check on the first PC run.

## What is installed
`tools/install_dlc.py` writes the 4 packs in the SDK (Xenia) layout; `package_win.py` hard-links them to `out/win/user/`:
- `user/0000000000000000/41560855/00000002/<STFS file name>/` – the pack files (`dlcN.id`, `*.ff`, `img_dlcN.pak`, `snd_dlcN.*.pak`)
- `user/0000000000000000/41560855/Headers/00000002/<name>.header` – 332 B: `XCONTENT_AGGREGATE_DATA` (0x148) + license mask.
  The size matches `ContentManager::ReadContentHeaderFile`.

| Pack | id | Zombies map | MP maps |
|---|---|---|---|
| First Strike | dlc2 | zombie_cosmodrome (Ascension) | berlinwall2, discovery, kowloon, stadium |
| Escalation | dlc3 | zombie_coast (Call of the Dead) | hotel, gridlock, outskirts, zoo |
| Annihilation | dlc4 | zombie_temple (Shangri-La) | area51, drivein, golfcourse, silo |
| Rezurrection | dlc5 (+dlc1) | zombie_moon + the 4 WaW maps (prototype, asylum, sumpf, factory) | – |

## How the game mounts DLC (ZM, same in MP)
- `sub_823A6540` enumerates: `XamContentCreateEnumerator(user, device 0 = all, type 2, flags 0, 7 items)`, then `XamEnumerate`.
  At most 7 packs (slot stride 0x134 = `XCONTENT_DATA`, list ends at +0x86C).
- `sub_823A6018` mounts each pack: `XamContentCreateEx` with root `dlc%i` ("Couldn't get free content pack index" if none is
  free), checks `dlc%i:\<id>.id`, then sound `dlc%i:\snd_dlc%i.<lang>.pak`. Zones load through `%s:\%s` (root + name).
  `0x3E5` (ERROR_IO_PENDING) is treated as success, so the overlapped path of the SDK is fine.
- The game imports only `XamContentCreateEnumerator`, `XamEnumerate`, `XamContentCreateEx`, `XamContentClose`,
  `XamContentGetDeviceState`, `XamContentGetDeviceData` (+ `XamShowMarketplace*UI`). The SDK implements all of them.
  It does not import `XamContentGetLicenseMask`, so the license mask in the header does not matter.
- The SDK enumerator lists the user's content and the shared (xuid 0) content; marketplace content always resolves to
  xuid 0 (`EffectiveContentOwner`), which is where the packs are. The root `dlcN` becomes a `dlcN:` symbolic link.
- Map menus: `ui_showDLCMaps`, `MENU_DLC2_MAPS`..`MENU_DLC6_MAPS`, gated by the `dlc1`..`dlc6` content bits that the mount sets.

Verdict: no code change is needed for DLC. The enumerator runs for a signed-in user; the 4 offline profiles cover this.

## Check on the first PC run
- Log: `[save] list type 00000002 in ...\user\0000000000000000\41560855\00000002: 4 entry(ies)`.
- No "Xbox 360 Error ... XContentCreate()" or "Couldn't get free content pack index" in the log.
- Zombies map select shows Ascension, Call of the Dead, Shangri-La, Moon and the WaW maps.
- If the list is 0 entries: check the `user_data_root` in the toml and that `user\` sits beside the exe.
