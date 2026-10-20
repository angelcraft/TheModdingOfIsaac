# Architecture boundary

## Layer A — Lua VM and mod dispatcher

`source/isaac_compat.lua` now provides a Lua 5.1 runtime boundary with:

- `RegisterMod` and per-mod `AddCallback` registration.
- Isolated script globals populated from an explicit host API table.
- Callback dispatch that passes the mod object as `self`, captures every callback result, and reports errors without silently inventing engine behavior.
- Delegated `SaveData`, `LoadData`, and `HasData` calls.

The host must pass already-read script text to `load_mod`. Callback filters are rejected until their rules are implemented. Callback return aggregation is left to the host because engine callback semantics vary.

Example host shape:

```lua
local Runtime = require("source.isaac_compat")
local runtime = Runtime.new({
    globals = {
        ModCallbacks = verified_callback_values,
        Game = verified_game_function,
        Isaac = verified_isaac_api,
        -- Include other exact-build globals required by the mod.
    },
    log = host_log,
    save_data = host_save,
    load_data = host_load,
    has_data = host_has_data
})
local mod, err = runtime:load_mod("The Specialist for Good Items", source_text)
```

This layer does not define fake `Isaac`, `Game`, entity, player, costume, music, or resource APIs. If the host leaves one out, the script fails visibly when it reaches that global.

## Layer B — game host bridge (not implemented)

A native component must expose real globals and call `runtime:dispatch` with callback IDs and arguments from the exact target game. It must refuse to run when the loaded executable identity differs from the verified build. The runtime does not determine callback values or arguments.

The existing `source/loader_core.lua` and `mods/specialist_probe.lua` remain the isolated mock model; they are not connected to game state.

Possible normalized entity record for the mock model:

```lua
{ entity_id = 123, type = "pickup", variant = "collectible", subtype = 42 }
```

These strings and IDs are test values only. A future bridge must derive data from verified game structures and version-specific tables, not hardcode assumptions from PC.

## Layer C — Isaac behavior (not implemented)

Still requires reliable costume/animation registration, player costume mutation, music registration/playback/restoration, and cleanup when the pedestal is picked up, rerolled, removed, or the room changes. Specialist's PC callbacks (`MC_POST_UPDATE`, `MC_POST_NEW_ROOM`) and globals (`Game`, `Isaac`, `EntityType`, `PickupVariant`, `ModCallbacks`, `MusicManager`) do not exist in a plain Lua VM.

## Chosen Workshop mod: The Specialist for Good Items (2575911103)

The selected `main.lua` registers `MC_POST_UPDATE`, `MC_POST_NEW_ROOM`, `MC_USE_ITEM`, and `MC_USE_CARD` to scan collectible pedestals, check item quality/curse visibility, apply character-specific null costumes, and switch music. It also uses game-start/game-exit callbacks for JSON settings persistence and optionally integrates Mod Config Menu. The port needs an in-process host that supplies these game observations and APIs; changing a stub's boolean return does not create a Lua state or dispatch callbacks.

## Compatibility choice

The extracted update archive contains two candidate executables:

- `Program #0/1/.nro/AfterbirthPlus.nro` — 8,523,776 bytes; SHA-256 `705402b213072f67b561d011302c315448f47a16b9187b15b8823e003f6ccdad`.
- `Program #0/1/.nro/Repentance.nro` — 11,210,752 bytes; SHA-256 `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`; internal NRO Build ID `91C73FDD575061318D68886316AFEAC72388B2AB`.

The `Repentance.nro` was extracted from the version 1.7.9b update package with ProgramId `010021C000B6A800`. DBI shows the installed base application `010021C000B6A000`, version 1.7.9b, with an Update and Addon 0001. Switch update ProgramIds use the application ProgramId with mask `0x800` set, so the local update package corresponds to the installed base title ([technical reference](https://switchbrew.org/wiki/Title_list/Games)). Nintendo lists Repentance as DLC requiring Afterbirth+, which matches the Addon entry ([Nintendo listing](https://www.nintendo.com/en-gb/DLC/The-Binding-of-Isaac-Repentance-2081104.html)).

Ghidra analysis of the extracted `Repentance.nro` found:

- `ModManager::RunModScripts` at `0x421838` is an immediate return.
- `ModManager::RunModScript` at `0x42183c` sets the return value to zero and returns.
- The analysis found no incoming code/data references or direct branch/call sites for either stub.

These functions are not confirmed hooks. Returning true from `RunModScript` alone would only change a return value; it would not load source, initialize Lua, or invoke game callbacks. Do not patch until a real in-game call path and ABI are verified.
