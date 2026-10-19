# The Modding of Isaac — isolated Lua POC (Milestone 1A)

This is a **host-side logic prototype**, not an NRO and not a game patch. It deliberately has no Nintendo Switch, Atmosphère, game-memory, or Isaac API integration. It exercises a Specialist-style predicate with mock entities only.

## Goal

When a *new* qualifying collectible pedestal is observed, emit one `specialist_start` action marked `before_pickup`; when that pedestal disappears or the room changes, emit `specialist_stop`.

## Files

- `source/loader_core.lua`: small callback registry with protected dispatch.
- `mods/specialist_probe.lua`: Specialist-style test mod consuming only the mock host API.
- `tests/run.lua`: deterministic host-side tests.
- `test.lua`: tiny script intended for the later standalone Switch-runtime milestone.
- `docs/ARCHITECTURE.md`: strict separation between VM, host bridge, and Isaac behavior.
- `docs/NATIVE_INTEGRATION_FEASIBILITY.md`: verified static findings from the supplied Afterbirth+ and Repentance NROs, including build IDs and the inspected stub entry points.
- `docs/STEAM_MOD_COMPATIBILITY.md`: compatibility tiers and a roadmap for supporting a selected subset of Steam Workshop mods.

## Host-side test

Requires Lua 5.1+ installed on the development PC. From this directory, run:

```sh
lua tests/run.lua
```

The host tests have not been confirmed passing yet. Static inspection is not a passing runtime test.

Passing these tests means only that the isolated logic behaves as specified on a host Lua runtime. It does **not** prove the Switch Lua port builds, an NRO launches, code runs inside Isaac, entity values are mapped correctly, or animation/music works.

## Standalone Switch runtime smoke test

The current NRO experiment uses a pinned Lua 5.1 Switch port to establish a standalone Lua execution path on homebrew. It is intentionally separate from Isaac. A successful build or `Lua OK` message will not demonstrate mod compatibility.

For broader vanilla Repentance mod compatibility, the project must evaluate a Lua 5.3 runtime baseline and implement the game's API/callback bridge. See `docs/STEAM_MOD_COMPATIBILITY.md`. No game process access is part of this milestone.
