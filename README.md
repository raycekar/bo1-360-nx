# bo1-360-nx

**Call of Duty: Black Ops (Xbox 360, Title Update 11), statically recompiled for Windows PC.**
Focus: Zombies with **four-player local split screen**, and Multiplayer. Campaign is out of scope for now.

> [!IMPORTANT]
> **No game data is included, and none ever will be.** You need your own Xbox 360 copy of Black Ops, its Title
> Update 11 and (optionally) your DLC packs. Everything game-derived is built on your machine from your files.

## Status

- **Research**: four-player Zombies is feasible; the limit is the menus' `party_maxlocalplayers 2`
  ([docs/4p-zombies-research.md](docs/4p-zombies-research.md)).
- **Zombies program** (`zm/`, TU11 `default.xex`): recompiled (18,436 functions) and cross-built for Windows with
  the exact Xenos GPU renderer. The 4-player change is in (`zm/src/bo1zm_local_players.cpp`). **Not yet run.**
- Multiplayer (`default_mp.xex`): not started.

Building: [docs/building.md](docs/building.md).

## Based on

- [bo2-360-nx](https://github.com/AlvaroEPena/bo2-360-nx) by AlvaroEPena: the approach, its modified ReXGlue SDK
  (`sdk/`) and tools. GPL-3.0.
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), built on [Xenia](https://github.com/xenia-project).

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Call of Duty and Black Ops are trademarks of Activision
Publishing, Inc. This project is not affiliated with or endorsed by Activision or Microsoft.

## License

GPL-3.0 ([LICENSE](LICENSE)).
