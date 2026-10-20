# The Modding of Isaac — isolated Lua POC

The project has two deliberately separate parts:

- A host-tested Specialist behavior model driven by synthetic entity records.
- A standalone Lua 5.1 Switch NRO that runs the same model checks, without entering Isaac.

Neither part provides a native game bridge or patches Isaac.

## Source-derived model

- `source/loader_core.lua`: small callback registry with protected dispatch for the isolated behavior model.
- `source/isaac_compat.lua`: Lua 5.1 mod-registration and callback-dispatch runtime foundation. It requires explicit host-supplied Isaac globals; it does not emulate them.
- `mods/specialist_probe.lua`: Specialist trigger model using a mock host API.
- `test.lua`: shared deterministic checks, executed on the host and by the Switch NRO.
- `tests/run.lua`: host entry point for `test.lua`.
- `switch-nro/`: NRO source and build instructions.
- `docs/SPECIALIST_REPENTANCE_SOURCE_REVIEW.md`: source review for the supplied Workshop ZIP.
- `docs/SPECIALIST_API_CONTRACT.md`: modeled behavior and unresolved Isaac API requirements.
- `docs/SPECIALIST_LUA51_PREPARATION.md`: guarded source conversion and its limits.

Run the host suite from this directory with Lua 5.1:

```sh
lua5.1 tests/run.lua
```

## Switch package

The CI artifact contains an NRO plus the same Lua test and model files. On success, the console shows:

```text
PASS: 9 Specialist mock scenarios plus callback dispatch
Lua runtime: Lua 5.1
LIMIT: simulation only; no Isaac API or game integration.
```

SD-card layout and the download workflow are documented in `switch-nro/README.md`.

## Specialist source preparation

`tools/prepare_specialist_lua51.py` creates a guarded Lua 5.1 source candidate from the supplied Specialist 1.5.2 mod tree. It preserves the original `main.lua` and records the remaining integration requirements. The generated folder is still source preparation only; it is not loaded by Isaac. See `docs/SPECIALIST_LUA51_PREPARATION.md`.

## Integration status

`source/isaac_compat.lua` now provides a real Lua-side registration/dispatch boundary for a host adapter. It does not supply native game values or functions. The Switch NRO remains a separate homebrew app. An identity-checked game adapter must still provide real callbacks, globals, entity/player APIs, graphics, music, and resource loading before this can run inside Isaac. Do not create a game patch until the target build, call flow, ABI, and reversible hook are verified.
