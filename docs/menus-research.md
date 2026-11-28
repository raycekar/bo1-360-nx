# In-game menu pages for the port: research notes

Goal (user's choice): the port's Graphics / Cheats pages inside BO1's own menus, as bo2-360-nx does in BO2's Lua menus.

## What is in place (zm)

- `bo1zm_console.cpp`: `QueueCommand(line)` runs a console line on the main thread at the start of the next frame
  (hook on Com_Frame TU11 `sub_82315590` -> Cmd_ExecuteSingleCommand `sub_823112E0(localClient, controller, text)`).
  `FindDvar` / `DvarNumber` read dvars (registry from Dvar_RegisterVariant `sub_8237AD78`). Test pipe commands:
  `cmd <line>`, `dvar <name>`.
- A console `set <name> <value>` creates a dvar that does not exist yet, so menu rows can be bound to port dvars
  (`bo1zm_ui_*`) that `bo1zm_options.cpp` reads every half second and applies.
- `bo1zm_options.cpp`: graphics presets (lowest/low/360/high/ultra) as a cvar; the menu will drive it.

## The Options menu (Zombies front end)

Rows: GRAPHIC CONTENT, LOOK INVERSION, CONTROLLER VIBRATION, AIM ASSIST, STICK LAYOUT, BUTTON LAYOUT, LOOK
SENSITIVITY, PLAYER NAME INDICATOR, SUBTITLES, SAFE AREA, AUDIO SETTINGS, BRIGHTNESS, STEREOSCOPIC 3D SETTINGS, SELECT
SAVE DEVICE, CREDITS. STEREOSCOPIC 3D SETTINGS opens `options_3d` (2 rows under a long caution text): too small to
reuse, but the row itself is the natural place to open the port's page (3D means nothing on a PC).

## Compiled menus (360, T5)

Menus are compiled assets (menuDef / itemDef structures) in `frontend.zone`, overridden by `patch_ui.zone`
(MP: `ui_mp.zone`, `patch_ui_mp.zone`); there is no .menu text to patch at load as with GSC.

From `work/ff/patch_ui.zone` (decoded zone, big-endian, pointers to inline data are 0xFFFFFFFF):
- An item's window name is inline text (`options_2` at 0xff6e2), followed by window fields (rect floats such as
  0x41b00000 = 22.0, 0x3ea147ae = 0.315).
- The label is inline text: `@MENU_LOOK_INVERSION_CAPS` (localized reference).
- Event handlers are **text scripts**, e.g. onFocus:
  `play "uin_navigation_over" ; setLocalVarInt ui_highlight 2 ; setLocalVarString ui_choicegroup "options" ;
   execnow ui_animate options "options_2" highlightOn 0 ; ; execNow set ui_hint_text "@MENU_LOOK_INVERSION_DESC" ;
   execNow set ui_show_arrow 1 ;`
  and the matching onLeave text. So a cloned row can get new behaviour by pointing its script strings at new text.

## Plan

1. In memory, after the UI zone loads: find the Options menuDef (by its name / by walking the UI's menu list), map
   menuDef_t / itemDef_t field offsets on the live structures (compare against the zone dump above).
2. Clone it as `bo1zm_options` (guest heap): new title, rows rewritten (label, bound dvar, choice list, hint text),
   unused rows hidden. Register it so `open bo1zm_options` works.
3. Point STEREOSCOPIC 3D SETTINGS (label and action) at the new page.
4. Pages: GRAPHICS (preset, then per-item), CHEATS (god mode, infinite ammo, speed, jump, FOV, third person, game
   speed, ...). Values in port dvars applied by bo1zm_options.cpp; saved in user/bo1zm_settings.txt.

Open questions: where the dvar binding and the choice list (dvarStrList) sit in itemDef_t; how the UI finds a menu by
name (menu list in uiInfo / the loaded MenuList assets); whether the front end and the in-match pause menu use the
same Options asset.
