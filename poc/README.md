# The Modding of Isaac — isolated Lua POC

The project has two deliberately separate parts:

- A host-tested Specialist behavior model driven by synthetic entity records.
- A standalone Lua 5.1 Switch NRO that runs the same model checks, without entering Isaac.

Neither part provides a native game bridge or patches Isaac.

## Source-derived model

- `source/loader_core.lua`: small callback registry with protected dispatch.
- `mods/specialist_probe.lua`: Specialist trigger model using a mock host API.
- `test.lua`: shared deterministic checks, executed on the host and by the Switch NRO.
- `tests/run.lua`: host entry point for `test.lua`.
- `switch-nro/`: NRO source and build instructions.
- `docs/SPECIALIST_REPENTANCE_SOURCE_REVIEW.md`: source review for the supplied Workshop ZIP.
- `docs/SPECIALIST_API_CONTRACT.md`: modeled behavior and unresolved Isaac API requirements.

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

## Integration status

A Switch-side pass checks that the source-derived mock logic runs under the pinned Lua 5.1 Switch runtime. It still cannot prove real Isaac entity mapping, callbacks, graphics, music, or game integration. The exact target build, native call flow, ABI and a safe integration route must be verified before creating a game patch.
