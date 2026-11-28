# bo1-360-nx

**Call of Duty: Black Ops (Xbox 360, Title Update 11), statically recompiled for Windows PC.**
Focus: Zombies with **four-player local split screen**, and Multiplayer. Campaign is out of scope for now.

> [!IMPORTANT]
> **No game data is included, and none ever will be.** You need your own Xbox 360 copy of Black Ops, its Title
> Update 11 and (optionally) your DLC packs. Everything game-derived is built on your machine from your files.

## Status

- **Zombies** (`zm/`, TU11 `default.xex`): every map plays; solo ~57-60 fps with zombies. **Split screen for 3 and 4
  players works** (~50 and ~43 fps on an RTX 3060 laptop; the 360 allowed 2).
- **Multiplayer** (`mp/`, `default_mp.xex`): plays; solo 58-60 fps, 4-player split screen ~25 fps.
- **System Link** works (tested with two copies on one PC); internet play with a join code is in, not yet tested.
- **Port extras**: graphics presets (lowest to ultra), god mode, "BUILDING SHADERS" text. In-game options/cheats
  pages are in progress.

Details, settings and findings: [docs/pc-port-notes.md](docs/pc-port-notes.md). Research:
[docs/4p-zombies-research.md](docs/4p-zombies-research.md).

Building: [docs/building-windows.md](docs/building-windows.md) (native Windows) or [docs/building.md](docs/building.md)
(Linux cross-build).

## Based on

- [bo2-360-nx](https://github.com/AlvaroEPena/bo2-360-nx) by AlvaroEPena: the approach, its modified ReXGlue SDK
  (`sdk/`) and tools. GPL-3.0.
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), built on [Xenia](https://github.com/xenia-project).

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Call of Duty and Black Ops are trademarks of Activision
Publishing, Inc. This project is not affiliated with or endorsed by Activision or Microsoft.

## License

GPL-3.0 ([LICENSE](LICENSE)).
