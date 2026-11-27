# Four-player HUD: what to check and fix

From the TU11 + DLC Zombies scripts (`zone_scripts.py` output) and the menu names in the zones. Nothing here has been
seen on screen yet; use it as the checklist for the first 4-player test.

## Geometry

| Players | Each view (of 1280x720) | Aspect |
|---|---|---|
| 1 | 1280x720 | 16:9 |
| 2 (360) | 1280x360, top/bottom | 32:9 |
| 3 | players 1-2: 640x360 quadrants on top; player 3: 1280x360 across the bottom | 16:9, 32:9 |
| 4 | 640x360, quadrants | 16:9 |

Source: the static rect table at TU11 `0x8262C1A0` (32 rects `{x,y,w,h}` as fractions, `[wideScreen][count-1][slot]`;
both `wideScreen` halves are the same). The init function `sub_821AEBF0` copies it to `0x82800088` (and two work
copies). `sub_821AEED0(localClient, slot, count)` reads it and writes the client's viewport; the dvars
`zombietron` and `zombieStopSplitScreen` force the full-screen rect. `sub_823D6EA0` keeps a focus stack and lays
out all 4 local clients again after a change (`cl_viewport_lerptime`).

A quadrant has the **same height** as a 2-player view and **half the width**. The scripts' split-screen tweaks are
almost all vertical (`.y`) offsets, so they still fit. Horizontally centered elements stay centered. The risks are
wide text and elements placed by absolute x.

## Script HUD tweaks under IsSplitScreen() / level.splitscreen

| Script | What it does in split screen | 4-way risk |
|---|---|---|
| `_zombiemode.gsc:3025`, `zombie_pentagon_elevators.gsc:170` | downed / elevator text `y = -137` | low |
| `_zombiemode.gsc:3613/3627`, `zombie_moon_teleporter.gsc:271`, `_zombietron_main.gsc:85/102` | game over / "survived" text `y += 40` (`+30`) | low; check text width |
| `_zombiemode.gsc:552` | `zombie_timer_offset = 280` | check the round timer |
| `_laststand.gsc:472`, `_zombiemode_equip_hacker.gsc:527` | revive / hacker prompt `y = -107` | low |
| `_laststand.gsc:25` | progress bar moves to y 280 | check: absolute y in a 360-tall view |
| `_zombiemode_score.gsc:284-416` | score popups only in local split screen; score `y = -95`; team pools not stacked | check the score column |
| `_hud_message.gsc:48-481` | notify messages: `yOffset 30`, `xOffset 0`, no icons | check that long messages fit 640 wide |
| `_hud_util.gsc:298-333` | bars: no border, anchored TOP instead of CENTER | low |
| `zombie_cod5_factory.gsc:210`, `zombie_cod5_asylum.gsc:190` | intro text `fontScale 2.75` in SD | check the width |

## Fog and draw distance (performance, not layout)

`_zombiemode.gsc:125`, `_zombiemode_load.gsc:500`, `_zombietron_load.gsc:457`, `_zombietron_main.gsc:67` and
`createart/zombie_coast_art.gsc:29` call `set_splitscreen_fog(...)`, which pulls the fog in to save the 360's GPU. A PC
does not need that, so we can make it optional later (a port setting that skips the split-screen fog).

## Engine menus with a split-screen version

`ui/hud_splitscreen.menu` (`hud_splitscreen.txt`), `miniscoreboard_splitscreen`, `pausedmenu_splitscreen`,
`ingameoptions_splitscreen`, `ui/hud_spectator_splitscreen_zombie.menu`, `splitscreen_lobby`, and the co-op
quit/leave popups. They are laid out for 1280x360. The engine places menus inside each player's view, so in a
quadrant they get half the width: check the ammo, perk and score corners and the pause menu.

## How to fix what the test shows

1. Script positions: patch the script text in memory when the zone loads (bo2-360-nx's GSC loader does this for T6;
   BO1's scripts are plain text, which makes it easier), or use a small GSC override per file.
2. Menus: patch the menu asset rectangles after load, or pick the full-screen HUD (`hud.menu`) for quadrants. Their
   16:9 aspect matches it.
