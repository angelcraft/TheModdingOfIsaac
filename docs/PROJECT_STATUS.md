# Project status and handoff

Last updated: 2026-10-19

## Current state
- Repository created by the user: https://github.com/angelcraft/TheModdingOfIsaac
- Nintendo Switch OLED, Atmosphère + Hekate, EmuMMC.
- User confirms Repentance DLC is installed and enabled. **Repentance is now the primary research target**; Afterbirth+ is kept as a separate compatibility target.
- User reports Isaac launches normally after removing the previous experimental patch/resource files.
- Do not reuse or install the prior native POC/diagnostic ZIPs. Those caused crashes.
- No working Lua loader inside Isaac and no verified native hook exists yet.
- New branch: `poc/isolated-lua-runtime`; includes a host-side Specialist trigger simulation, tests, a standalone Switch Lua NRO source/build workflow, and Repentance integration notes.
- GitHub Actions workflow run #1 is queued as of this status snapshot; the host tests and Switch build have not yet been reported passing.

## Goal and architecture boundary
Build a reusable Lua mod loader/compatibility layer for The Binding of Isaac on Nintendo Switch, eventually supporting the Specialist dance mod when a qualifying collectible is on a pedestal before pickup.

Keep the layers distinct:
1. A Lua VM loads/executes scripts.
2. A native game bridge provides verified entity snapshots and dispatches callbacks.
3. An Isaac compatibility shim exposes a small, deliberate subset of the PC mod API.
4. Specialist-specific animation, costume, and music behavior is a separate phase.

A standalone interpreter does **not** imply in-game execution or provide the Isaac API.

## Known source package and targets
Earlier work inspected the update archive:
`The Binding of Isaac_ Afterbirth+ 1.7.9b [010021C000B6A800][v524288][UPD].zip`.
The continuity package includes symbol CSVs for both candidate NROs, not the NRO binaries themselves.

### Primary: Repentance
- Candidate: `Program #0/1/.nro/Repentance.nro`
- Reported size: 11,210,752 bytes
- Reported SHA-256: `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`
- Resource layer: `Program #0/1/rp_patch/resources/`
- The user confirms DLC installation/enabling; the exact file hash/build ID loaded by the console still needs confirmation.

### Separate target: Afterbirth+
- Candidate: `Program #0/1/.nro/AfterbirthPlus.nro`
- Reported size: 8,523,776 bytes
- Reported SHA-256: `705402b213072f67b561d011302c315448f47a16b9187b15b8823e003f6ccdad`
- Resource layer: `Program #0/1/resources/`

These size/hash values are from a prior read-only report and have not been recalculated from the binary in this session. Before byte-level integration work, obtain the exact Repentance NRO, hash it, inspect its header/ModuleId/build ID, and remap its sections. Do not mix its addresses or resource IDs with the Afterbirth+ variant.

Other IDs noted earlier:
- Base: `010021C000B6A000`
- Update: `010021C000B6A800`
- Repentance DLC title: `010021C000B6B001`

## Repentance native research leads
The continuity package includes `REPENTANCE_SYMBOLS.csv` with 17,249 symbol entries. It records:
- `ModManager::RunModScripts()`: size 4 bytes.
- `ModManager::RunModScript(string&)`: size 8 bytes.
- `Entity_Pickup::Update()`: size 11,860 bytes.
- Costume and music methods are present.

Function size alone does not prove a stub or safe hook. The raw machine instructions for the Repentance NRO are missing from the continuity package, so these are leads only. Verify bytes, disassembly, offsets, ABI, displaced instructions, branches, registers, stack and return path afresh. Do not reuse offsets or patch bytes from earlier attempts.

Atmosphère's official `ro` documentation describes NRO IPS/IPS32 patching at `/atmosphere/nro_patches/<patchset>/<nro build id>.ips`. This is a possible patch-delivery mechanism, not an integration solution. Confirm the real loading path and exact Build ID before considering it.

## Previous failures and lessons
- Loose animation/resource ZIPs did not trigger the dance because Specialist's callbacks and game API were not present.
- Prior native hook and diagnostic patch both crashed the game and were removed.
- Do not label anything tested/working/install-ready based on static checks.
- The current POC has not yet been run on the Switch and does not access Isaac.

## Next milestones
1. Execute the isolated host Lua tests via CI.
2. Compile the standalone Lua 5.1.5 Switch NRO and obtain the CI artifact.
3. Verify the NRO on the user's Switch with only `test.lua` mounted separately from Isaac.
4. Obtain and re-inspect the exact Repentance NRO bytes; verify ModuleId/build ID and whether Atmosphère's NRO patcher sees the game module.
5. Perform one minimal, reversible, read-only native probe with fail-closed build identity checks.
6. Snapshot room entities, identify collectible pedestals, and verify the predicate before any game-state change.
7. Implement the minimal Specialist compatibility behavior and separately validate resource/costume/music loading.

## Safety rules
- Keep game dumps/NROs and proprietary game assets out of GitHub.
- Preserve backups and keep all experiments reversible.
- No installable game patch until source bytes, mapping and host tests are verified; no claim of success until device test.
