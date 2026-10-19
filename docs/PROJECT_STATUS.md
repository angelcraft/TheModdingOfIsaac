# Project status and handoff

Last updated: 2026-10-19

## Current state

- Repository: https://github.com/angelcraft/TheModdingOfIsaac
- Target setup: Nintendo Switch OLED, Atmosphère + Hekate, EmuMMC.
- Repentance DLC is installed and enabled. **Repentance is the primary research target**; Afterbirth+ remains a separate compatibility target.
- Isaac currently launches normally after the previous experimental patch/resource files were removed.
- Branch: `poc/isolated-lua-runtime`.
- The branch now contains a source-derived Specialist trigger model in the mock host, Lua tests, a standalone Switch Lua NRO source/build workflow, a read-only NRO identity preflight tool, and an explicit Steam Workshop compatibility roadmap. The model does not call Isaac.
- The user supplied the exact Repentance Workshop ZIP for ID 2575911103. Its metadata matches the target at version 1.5.2; its complete inventory and source findings are recorded in [SPECIALIST_REPENTANCE_SOURCE_REVIEW.md](../poc/docs/SPECIALIST_REPENTANCE_SOURCE_REVIEW.md). The source-derived trigger model is tracked separately in [SPECIALIST_API_CONTRACT.md](../poc/docs/SPECIALIST_API_CONTRACT.md).
- Both requested NRO files have now been inspected in this session. They match the earlier recorded sizes and SHA-256 hashes.
- The inspected script-runner entry points in both binaries are immediate-return stubs. No installable Isaac patch has been generated.
- GitHub Actions run [39300360076](https://github.com/angelcraft/TheModdingOfIsaac/actions/runs/39300360076) completed successfully: host Lua tests and the standalone Switch NRO build both passed. The run produced the `TheModdingOfIsaac-Lua-Switch-POC` ZIP artifact (expires 2026-11-02). This is only a standalone Lua 5.1 homebrew smoke test, not an Isaac mod or game patch.

## Goal and architecture boundary

Build a reusable Lua mod loader/compatibility layer for The Binding of Isaac on Nintendo Switch. The Specialist dance is the first end-to-end mod; the broader goal is to support a selected subset of vanilla Repentance Steam Workshop mods.

Keep the layers distinct:
1. A Lua VM loads and executes scripts.
2. A native game bridge provides verified entity snapshots and dispatches callbacks.
3. An Isaac compatibility shim exposes a deliberately selected subset of the PC mod API.
4. Specialist-specific animation, costume and music behavior is developed separately.
5. A compatibility checker identifies missing API features, resource files, version mismatches and mods that require REPENTOGON or PC-only components.

A standalone interpreter does **not** imply in-game execution or provide the Isaac API.

## Verified supplied NRO identities

The file hashes and build IDs below were read from the two user-supplied NROs. A matching supplied-file identity does not yet prove which exact binary the console loads at runtime.

### Primary: Repentance

- File: `Repentance.nro`
- Size and declared NRO size: 11,210,752 bytes
- SHA-256: `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`
- NRO build ID from the header: `91C73FDD575061318D68886316AFEAC72388B2AB`
- Resource layer noted in the update package: `Program #0/1/rp_patch/resources/`

### Separate target: Afterbirth+

- File: `AfterbirthPlus.nro`
- Size and declared NRO size: 8,523,776 bytes
- SHA-256: `705402b213072f67b561d011302c315448f47a16b9187b15b8823e003f6ccdad`
- NRO build ID from the header: `0F65948274CE60ECA59C1E55B09C54D0607F3C57`
- Resource layer noted in the update package: `Program #0/1/resources/`

The build IDs are the first 20 bytes of the NRO header's ID field at offset `0x40`. Do not mix offsets or resource IDs between targets.

Other title IDs noted earlier:
- Base: `010021C000B6A000`
- Update: `010021C000B6A800`
- Repentance DLC title: `010021C000B6B001`

The read-only verifier is `poc/tools/nro_identity.py`. It checks NRO0 magic, declared size, segment bounds, build ID and SHA-256; with `--expect repentance` or `--expect afterbirth-plus`, it fails closed unless the file matches the corresponding exact reference hash and header ID.

## Repentance native findings

The raw instructions at the mod-manager symbol locations were checked directly in the uploaded NRO:

- `IsaacRepentance::ModManager::RunModScripts()` at `0x421838`, size 4: `C0 03 5F D6` (`ret`).
- `IsaacRepentance::ModManager::RunModScript(std::string&)` at `0x42183C`, size 8: `E0 03 1F 2A C0 03 5F D6` (`mov w0, wzr; ret`).
- The Afterbirth+ `ModManager::RunModScript(std::string&)` entry at `0x24FA68` similarly contains `mov w0, wzr; ret`.

The supplied Repentance NRO also contains nontrivial native routines for `Entity_Pickup::Update()`, `Entity_Player::AddNullCostume`, `Entity_Player::TryRemoveCostume`, `Music::GetCurrentMusicID()`, `Music::Play()` and `Room::PlayMusic()`. These are research leads, not verified hook points. Their semantics, calling conventions, object layout, frame/thread context and safe integration path still need to be validated before calling them.

A string scan found no matches for selected obvious Lua identifiers (`lua_State`, `luaL_newstate`, `lua_pcall`, `RegisterMod`, `ModCallbacks`, `AddCallback`, `.lua`). This is heuristic; the immediate-return instructions are the direct evidence for the inspected mod-script entry points.

Detailed report: `poc/docs/NATIVE_INTEGRATION_FEASIBILITY.md`.

## Steam Workshop compatibility strategy

- Resource-replacement mods are the best early compatibility target.
- Simple vanilla Repentance Lua mods are a later target once the VM is integrated and a tested subset of the Isaac API/callbacks exists.
- Content mods require resource and ID registration in addition to Lua APIs.
- REPENTOGON-dependent mods and PC-only native/external integrations are out of initial scope and may need separate ports.
- Selected target: [The Specialist for Good Items, Workshop ID 2575911103](https://steamcommunity.com/sharedfiles/filedetails/?id=2575911103), the Repentance version. The supplied ZIP SHA-256 is FE4F89C4108E9977F98AB77A0C85C223CBD198B2F9F0CF87012DD7D39CE04CA7; metadata.xml identifies version 1.5.2. Static source findings are in [SPECIALIST_REPENTANCE_SOURCE_REVIEW.md](../poc/docs/SPECIALIST_REPENTANCE_SOURCE_REVIEW.md).
- Workshop ID 3231114249 and the public [SpecialistABP source](https://github.com/D00NIK/Specialistabp) are a separate Afterbirth+ adaptation. Use only as a limited behavioral reference; do not treat it as the target or install it for Repentance.
- The Repentance modding documentation links Lua 5.3 as the language reference. The current pinned Lua 5.1 Switch port is only a standalone smoke-test candidate; broader compatibility needs a Lua 5.3 evaluation and a native Isaac API bridge.

Roadmap: `poc/docs/STEAM_MOD_COMPATIBILITY.md`.

## Previous failures and lessons

- Loose animation/resource ZIPs did not trigger the dance because the logic and game API were not available.
- Previous native hook/diagnostic attempts crashed the game and were removed.
- Do not label anything tested, working or install-ready based on static checks.
- The current POC does not access Isaac. The user reports that its standalone Lua NRO displayed Lua OK on Switch; this does not validate loading mods inside Isaac.

## Next milestones

1. [x] Obtain and inventory the original Repentance Workshop bundle for ID 2575911103; source review is documented in `poc/docs/SPECIALIST_REPENTANCE_SOURCE_REVIEW.md`.
2. [x] Confirm the standalone Lua NRO displays Lua OK on Switch; this confirms only homebrew Lua execution and does not patch Isaac.
3. Confirm the console's actual loaded Repentance NRO identity and whether the loader path goes through Atmosphère's NRO patcher.
4. Verify native call flow, ABI and a safe integration route before designing a reversible, build-identity-checked probe.
5. Expose a read-only room/entity snapshot and validate collectible-pedestal detection.
6. Add a controlled mod-folder loader and callback dispatcher.
7. Port the actual Repentance Specialist in isolated steps: trigger detection, costume, animation assets, music, cleanup.
8. Add tiered compatibility checks for more vanilla Workshop mods, starting with resource-only mods and small mods with narrow API dependencies.

## Safety rules

- Keep game dumps/NROs and proprietary game assets out of GitHub.
- Preserve backups and make experiments reversible.
- No installable game patch until the source bytes, mapping, calling conventions and validation are understood.
- No claim of success until the build and device tests have actually completed.
