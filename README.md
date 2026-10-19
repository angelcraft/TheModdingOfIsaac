# The Modding of Isaac

Experimental research project to investigate a Lua mod loader and modding compatibility layer for the Nintendo Switch version of The Binding of Isaac.

## Status

**Research/POC only — no working Lua loader inside Isaac exists yet.** Earlier experimental IPS/NRO patches caused crashes and were removed. The user reports that Isaac launches normally again.

A separate branch `poc/isolated-lua-runtime` now contains:
- a source-derived mock Specialist trigger model with host-side tests,
- source for a standalone Lua 5.1 Switch NRO,
- a CI workflow to run host tests and build/package that NRO,
- target-specific Repentance integration notes,
- a static source review of the actual Repentance Workshop ZIP ([report](poc/docs/SPECIALIST_REPENTANCE_SOURCE_REVIEW.md)), without uploading its third-party assets.

GitHub Actions passed the host tests and built the standalone NRO; the user reports that it displayed Lua OK on Switch. This confirms only standalone Lua execution, not Isaac compatibility. This work does not patch Isaac.

## Target selection

The user confirmed the Repentance DLC is installed and enabled. **Repentance is the primary research target**; Afterbirth+ remains a separate compatibility target.

The previously analysed update archive included:
- `Program #0/1/.nro/AfterbirthPlus.nro`
- `Program #0/1/.nro/Repentance.nro`
- `Program #0/1/resources/` (Afterbirth+ resources)
- `Program #0/1/rp_patch/resources/` (Repentance resource layer)

Prior reported file hashes/section notes are leads; re-hash and inspect the exact Repentance NRO before byte-level work. Never mix NRO offsets, resource IDs or item IDs across versions.

## Architecture boundary

1. **Lua VM:** reads/executes Lua scripts and reports errors.
2. **Native game bridge:** must independently provide verified, preferably read-only room/pedestal data.
3. **Isaac compatibility API:** must implement only the callbacks/globals the chosen mod actually uses.
4. **Specialist behavior:** costume/animation and music/resource management.

A standalone Lua runtime does not include the game's `Game()`, `Isaac`, `EntityType`, `PickupVariant`, `ModCallbacks`, `MusicManager()`, or resource registration API.

## References

- Specialist AB+: https://github.com/D00NIK/Specialistabp
- Lua 5.1.5 Switch port: https://github.com/carstene1ns/lua51-switch
- Atmosphère official NRO patching documentation: https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/ro.md

## Safety
- Never install previous diagnostic/native POC ZIPs or reuse their bytes/offsets.
- No native patch until raw executable bytes, section mapping, Build ID, ABI, displaced instructions, branch/return behavior and loader path are checked.
- Keep game dumps and proprietary assets out of GitHub; keep tests separate and reversible.
- Do not describe static inspection as runtime validation.
