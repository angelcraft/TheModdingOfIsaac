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
- `tools/nro_identity.py`: read-only NRO0/build-ID/SHA-256 preflight; it can fail closed if the file is not the expected reference build.

## Host-side test

Requires Lua 5.1+ installed on the development PC. From this directory, run:

```sh
lua tests/run.lua
```

GitHub Actions run 39301289065 passed the host Lua tests and standalone NRO build. The user reports that the standalone NRO displayed Lua OK on Switch. This verifies only the isolated runtime; it does not verify Isaac integration.

Passing these tests means only that the isolated logic behaves as specified on a host Lua runtime and that the NRO preflight rejects malformed synthetic headers. It does **not** prove the Switch Lua port builds, an NRO launches, code runs inside Isaac, entity values are mapped correctly, or animation/music works.

To inspect a game NRO without modifying it:

```sh
python3 tools/nro_identity.py /path/to/Repentance.nro --expect repentance
python3 tools/nro_identity.py /path/to/AfterbirthPlus.nro --expect afterbirth-plus
```

The `--expect` check succeeds only for the exact reference hash plus matching header build ID; an updated or modified NRO is intentionally reported as unknown until researched separately.

## Standalone Switch runtime smoke test

The current NRO experiment uses a pinned Lua 5.1 Switch port to establish a standalone Lua execution path on homebrew. It is intentionally separate from Isaac. The reported Lua OK message does not demonstrate mod compatibility.

The Repentance source review is in `docs/SPECIALIST_REPENTANCE_SOURCE_REVIEW.md`. For broader mod compatibility, the project must evaluate a Lua 5.3 runtime baseline and implement the game's API/callback bridge. See `docs/STEAM_MOD_COMPATIBILITY.md`. No game process access is part of this milestone.
