# Architecture boundary

## Layer A — Lua VM (this POC)

Responsibilities: parse/load `.lua`, execute code, manage registered callbacks, isolate callback failures and produce diagnostics. It has no knowledge of Isaac's entity classes or game memory.

## Layer B — host bridge (not implemented)

A native component must expose a deliberately tiny table/function interface to Lua and dispatch synthetic events. Initially it should be read-only: room transition and snapshots of visible pickup entities. It must be disabled when the target executable identity differs from the validated build.

Possible normalized entity record:

```lua
{ entity_id = 123, type = "pickup", variant = "collectible", subtype = 42 }
```

These strings/IDs are test values only. A future bridge must derive them from verified game structures and version-specific tables, not hardcode assumptions from PC.

## Layer C — Isaac behavior (not implemented)

Separately requires reliable costume/animation registration, player costume mutation, music registration/playback/restoration, and correct handling when the pedestal is picked up, rerolled, removed, or the room changes. Specialist's PC callbacks (`MC_POST_UPDATE`, `MC_POST_NEW_ROOM`) and globals (`Game`, `Isaac`, `EntityType`, `PickupVariant`, `ModCallbacks`, `MusicManager`) do not magically exist in a plain Lua VM.

## Compatibility choice

The previous dump report identifies two candidate executables in one update archive:

- `Program #0/1/.nro/AfterbirthPlus.nro` — 8,523,776 bytes; SHA-256 `705402b213072f67b561d011302c315448f47a16b9187b15b8823e003f6ccdad`.
- `Program #0/1/.nro/Repentance.nro` — 11,210,752 bytes; SHA-256 `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`.

Those values identify files in the previously analysed archive, not what the console is currently launching. Runtime/title/DLC selection remains a precondition before any host bridge work. Treat Afterbirth+ `resources/` and Repentance `rp_patch/resources/` as separate targets.
