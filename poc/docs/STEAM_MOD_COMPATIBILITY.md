# Steam Workshop compatibility strategy

Updated: 2026-10-19

## Goal

The project should grow beyond one port. Specialist Dance is the first end-to-end test because it exercises several important parts of the engine: mod discovery, Lua execution, game callbacks, entity inspection, player costume changes, custom animation resources, and music.

The intended result is a **compatibility layer for a selected subset of vanilla Repentance mods**, not a promise that every Workshop mod will work unchanged. Mods vary widely in their API use and some depend on native extensions that this project does not provide.

## What the uploaded game binaries tell us

The user supplied both target NROs, and their sizes and SHA-256 hashes match the earlier research records. See `NATIVE_INTEGRATION_FEASIBILITY.md` for the build IDs and inspected mod-manager entry points.

In both binaries the inspected mod-script entry points are tiny immediate-return stubs. The native integration is therefore a prerequisite; putting `main.lua` onto the SD card alone will not activate Steam mods.

## Compatibility tiers

| Tier | Mod class | Feasibility / requirements |
|---|---|---|
| 1 — Resource replacement | Replaces existing sprites, music, graphics, or supported resource files | Best early target. Resource formats, paths and loading order still need validation on Switch. |
| 2 — Simple vanilla Lua | Uses `main.lua`, vanilla `RegisterMod`, ordinary callbacks and a limited set of documented game APIs | Plausible after a game bridge and callback dispatcher exist. Each API function must be implemented and tested. |
| 3 — Content mods | Adds items, characters, costumes, animations, entities, or custom music | Requires resource/ID registration and safe game-facing APIs; not just a Lua interpreter. |
| 4 — Repentogon-dependent mods | Requires REPENTOGON or native engine extensions | Out of initial scope. Would require a separate, researched native compatibility layer, and might remain unsupported. |
| 5 — External/native integrations | Requires external executables, DLLs, platform-specific integrations or PC-only services | Not directly portable to Switch. Must be replaced or omitted by a mod-specific port. |

A Workshop subscription is not itself a Switch installation. A user's chosen mod would need to be exported/copied into a normal folder, checked for dependencies, then packaged for the selected Switch game version. The tool must not try to fetch or install Workshop content automatically.

## Lua language baseline

The community Repentance API documentation links the Lua 5.3 manual as its language reference. The current pinned Lua 5.1 Switch runtime is only a small runtime-build experiment; passing its smoke test would **not** demonstrate source compatibility with Repentance mods.

Before implementing broad mod support, compare a Lua 5.3 build on devkitA64 with the existing 5.1 port. Prefer matching the game's documented Lua baseline where practical. Regardless of interpreter version, game-specific globals and objects such as `RegisterMod`, `Game()`, `Isaac`, `EntityPlayer`, `ModCallbacks`, `MusicManager()`, and callback semantics must be recreated separately.

Reference: https://wofsauge.github.io/IsaacDocs/rep/

## Selected Repentance target

The actual target is [The Specialist for Good Items, Steam Workshop ID 2575911103](https://steamcommunity.com/sharedfiles/filedetails/?id=2575911103). The official listing identifies Repentance as its required DLC. Its description and changelog describe the Specialist dance for quality-4 items, support for tainted and non-tainted characters, configurable quality/Blind-curse/item/music behavior, reduced flashing, and the 1.5.2 `SpecialistModAPI:AddMusicAlias()` extension for custom-character tracks.

The user has now supplied the original Repentance Workshop ZIP. Its metadata identifies The Specialist for Good Items, ID 2575911103, version 1.5.2. A complete static inventory found 101 files and 11 directories, including main.lua, 42 ANM2 files, 43 PNG files, six OGG tracks and the costumes/music XML. The ZIP SHA-256 and a source-derived API checklist are recorded in `SPECIALIST_REPENTANCE_SOURCE_REVIEW.md`. No assets or source bundle were uploaded to GitHub.

The separate [SpecialistABP adaptation](https://github.com/D00NIK/Specialistabp) is for Afterbirth+ and corresponds to Workshop ID 3231114249. It is not the selected target and must not be installed as the Repentance mod. Its source can still serve as a limited behavioral reference: its `main.lua` uses `RegisterMod`, `Game()`, `MC_POST_UPDATE`, `MC_POST_NEW_ROOM`, room entity enumeration, collectible-pickup checks, per-run pickup bookkeeping, costume APIs, music lookup/playback, and room-music restoration. Those calls have not been verified against the original Repentance source.

The user reports that the standalone Switch NRO displayed Lua OK; it is Lua 5.1 and does not run inside Isaac. The supplied main.lua contains Lua 5.3 operators (`//` and `&`) and depends on Isaac callbacks, entity/item configuration, costume/music APIs, JSON save data and optionally Mod Config Menu. Therefore the existing NRO cannot load this source unchanged. A real port still needs a Lua 5.3-compatible runtime choice plus a verified game bridge, callbacks, entity values, resource mapping, and cleanup. The latest official mod changelog is [here](https://steamcommunity.com/sharedfiles/filedetails/changelog/2575911103).

## Proposed milestone sequence

1. Verify each game's identity with its NRO header build ID before enabling anything.
2. Build and test a standalone Lua runtime. Keep it separate from the game until proven.
3. Establish the actual loading/patching path for the Repentance NRO.
4. Create a minimal, reversible native bridge that logs a version marker only.
5. Expose a read-only room/entity snapshot to Lua; verify collectible-pedestal detection.
6. Add a small callback dispatcher and load a controlled mod folder containing `metadata.xml` and `main.lua`.
7. Port Specialist in layers: trigger detection, costume, animation resources, music, cleanup.
8. Add a per-mod compatibility report that flags unavailable globals, callbacks, Repentogon dependencies, missing files and likely Lua-version issues.
9. Test resource-only mods first, then a few small vanilla Lua mods selected for narrow dependencies.

## Definition of “compatible”

A mod is not marked compatible merely because its script parses. It must load deterministically, report API/dependency errors clearly, behave correctly in-game, clean up after room transitions, and pass repeat-run/save/rollback checks on the exact target version.

No Switch installation package or Isaac patch is produced by this planning document.
