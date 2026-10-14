# Ace Combat 8 — Role-Aware Wingmen Native v0.2.1

**UE4SS-free C++ conversion / native integration developer preview** (2026-10-14)

**No compiler on your PC?** See `HOW_TO_BUILD_WITHOUT_COMPILER.md` for GitHub-hosted Windows compilation. Compiling still produces a library, not a playable AC8 mod.

**Important: Not an installable gameplay mod yet.** This archive contains a testable native DLL project and header, not a game-compatible, hooked DLL. Dropping its DLL into the Ace Combat 8 folder **will not** change wingman behavior. Ace Combat 8's wingman class, faction/team checks, target-setting API and missile fire API have not been verified; there is no honest way to supply a safe, working drop-in mod without those details. No UE4SS Lua runtime or binary is used anywhere in this version.

## What changed from v0.1

- Replaced the UE4SS Lua entry point, discovery script, and bridge with **native C++17** + a stable, exported C API (see `include/ac8rw.h`).
- Aircraft-role prediction, target scoring, formation deconfliction, defense priority, target stickiness, and missile launch suggestions all work as an independent native library.
- Included adjustable `wingmen.ini` and native tests. No dependency on UE4SS, Unreal headers, MinHook, other third-party libraries, or any game files for building/testing the planner.
- Unlike the previous read-only Lua diagnostic, this project **cannot inspect the game**. It must be integrated with a verified game-specific runtime bridge or converted to an asset override from verified data.

## Role behavior

| Role | Priorities | Missile decisions |
| --- | --- | --- |
| Fighter | Aircraft, bombers and threats to the player | Launch more readily at good air locks |
| Attacker | SAMs, ground facilities, surface combatants | Choose strike targets; fire only with valid solutions |
| Multirole | Air or ground as mission urgency changes | Adapt the fire cadence to the selected engagement |
| Electronic warfare | Protect friendly aircraft, intercept imminent threats | Conserves offensive missiles for defensive engagements |

All decisions require positive wingman/hostile identification provided by the **game adapter**. A missing lock, firing safety flag, readiness flag, hit estimate or ammunition prevents launch suggestions. Suggestions do not conjure EW effects or change missile tracking physics.

## Build on Windows 11

1. Install **Visual Studio 2022** with the *Desktop development with C++* workload and **CMake**.
2. Extract the ZIP anywhere **outside the game folder**.
3. Run `build_windows.cmd`. It creates `build\Release\AC8RoleWingmen.dll` and runs `ac8rw_test.exe`.
4. This DLL is a **planning library**, not an injection DLL and not yet loadable by Ace Combat 8 on its own. Do not copy it into `Game\Binaries\Win64` expecting a mod to activate.

For any CMake toolchain:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The native planner builds and passes tests on Linux (the environment used for this preview). A Windows DLL was **not** compiled or game-tested here, because a Windows compiler and Ace Combat 8 installation were unavailable.

## What a real native, no-UE4SS AC8 adapter needs

The game-specific adapter is **not implemented** yet. Its minimum responsibilities are:

1. **Offline-only startup**: integrate with a known legitimate mod-loading/offline process; refuse to run when anti-cheat or multiplayer is active. This archive contains no anti-cheat workaround, executable patcher, proxy DLL, signature bypass, or injector.
2. **Read objects**: identify **actual friendly wingmen** (never the player's plane, enemy AI, or scripted actors) and enumerate hostile aircraft/vehicles/ships using game-confirmed identifiers. Populate `AC8RW_Wingman` / `AC8RW_Target` snapshots.
3. **Validate weapon status**: read a real lock flag, missile readiness and ammo, lock strength and hit chance if the game exposes them. If values are unknown, pass `NAN` so missiles are *not* falsely recommended. Use safe-to-fire flags grounded in verified game logic.
4. **Call native planner**: call `AC8RW_Plan` at a sane interval and interpret `AC8RW_Command` as a recommendation, not a forced weapon launch.
5. **Act through validated in-game APIs only**: update AI objectives using the game's genuine AI target commands; request missile launch only through a correctly identified, safe, offline single-player in-game interface; call `AC8RW_ConfirmMissileLaunch` only after the game reports the launch.
6. **Preserve missions**: honor mission scripting, target restrictions, cease-fire states, and AI deactivation. Change no player or hostile aircraft instances by mistake.

See `include/ac8rw.h` for a precise, documented ABI. The C API intentionally has no game-specific addresses or offset guesses.

## Why not a simple `.pak`?

Ace Combat 8 uses Unreal Engine 5 / IoStore. Community modders report that unsigned custom IoStore containers are not automatically mounted simply by putting them in a `Paks` or `~mods` folder. Some mods now use a native DLL without UE4SS; others require UE4SS merely to bootstrap a loader. A static asset mod could be viable if the correct AI assets are identified and the game's verified loader handles them. Without those assets, generating a fake `.pak` would not work.

For background, not code dependencies:
- [Ace Combat 8 official website](https://www.bandainamcoent.com/games/ace-combat-8)
- [AC8ProportionalGuidanceMissiles](https://www.nexusmods.com/acecombat8wingsoftheve/mods/14) (reports UE4SS-free native implementation from version 2.0)
- [AC8 PGM / IoStore container limitations](https://www.nexusmods.com/acecombat8wingsoftheve/mods/24?tab=posts)
- [Ace Pilots - Enemy AI Difficulty](https://www.nexusmods.com/acecombat8wingsoftheve/mods/40) (uses a native hook DLL but is bootstrapped by UE4SS)

**No source or assets were copied from external mods.** All project code is original or an independent C++ port of our previous v0.1 Lua decision logic. MIT terms are in `LICENSE`.

## Uninstall

This package does not install into Ace Combat 8; delete the extracted project directory. If you used the old v0.1 Lua preview, remove `AC8RoleWingmen` from the `ue4ss\Mods` folder. Removing UE4SS itself is a separate decision if other mods need it.
