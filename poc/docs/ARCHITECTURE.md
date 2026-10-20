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

## Compatibility choice

The previous dump report identifies two candidate executables in one update archive:

- `Program #0/1/.nro/AfterbirthPlus.nro` — 8,523,776 bytes; SHA-256 `705402b213072f67b561d011302c315448f47a16b9187b15b8823e003f6ccdad`.
- `Program #0/1/.nro/Repentance.nro` — 11,210,752 bytes; SHA-256 `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`.

The Switch evidence below identifies the installed configuration as Afterbirth+ with the Repentance add-on, so Repentance's `rp_patch/resources/` is the relevant resource candidate for this mod. These archive hashes still do not prove that either extracted NRO is byte-for-byte the executable currently launched by the console. Keep the runtime bridge and any binary patch disabled until that executable identity and a real call path/ABI are verified.

## Switch package evidence

The DBI title details photo supplied on 2026-10-20 reports:

- Title: `The Binding of Isaac: Afterbirth+`
- Version: `1.7.9b`
- Title ID: `010021C000B6A000`
- Displayed BID: `B6E5BDB9DC12E1D1`
- Content entries: `Application`, `Update`, and `Addon 0001`

On Switch, Repentance is distributed as add-on content that requires Afterbirth+; Nintendo's listing identifies it as `The Binding of Isaac: Repentance` DLC and names Afterbirth+ as the required game: https://www.nintendo.com/en-gb/DLC/The-Binding-of-Isaac-Repentance-2081104.html. Together with the DBI `Addon 0001` entry, this identifies the user's installed configuration as Afterbirth+ plus Repentance, rather than Afterbirth+ alone. The title label in DBI is the base game's name.

The version and title IDs are package-manager metadata. They identify the installed title/update/add-on configuration, but are not a cryptographic hash of the executable bytes. Match the running executable to the analysed NRO before enabling a bridge or patch.
