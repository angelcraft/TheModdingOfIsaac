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

## Host-side test

Requires Lua 5.1+ installed on the development PC. From this directory, run:

```sh
lua tests/run.lua
```

No Lua interpreter is bundled. These tests have **not** been executed here because no Lua 5.1 executable is available in the current environment. ZIP structure and integrity were checked. Static inspection alone is not a passing runtime test.

Passing these tests means only that the isolated logic behaves as specified on a host Lua runtime. It does **not** prove the Switch Lua port builds, an NRO launches, code runs inside Isaac, entity values are mapped correctly, or animation/music works.

## Next milestone

Build Lua 5.1.5 using the Switch homebrew toolchain, pin its source commit, then make a separate minimal libnx NRO that loads `/switch/TheModdingOfIsaac/test.lua` from SD and shows/logs `Lua OK`. No game process access in that milestone.
