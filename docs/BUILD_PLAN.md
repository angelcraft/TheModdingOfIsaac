# Build plan

## Milestone 0 — environment and target identity
- [ ] Record Atmosphère, firmware, Hekate and EmuMMC setup (only versions relevant to compatibility).
- [x] Identify the launched title and target NRO from the user's update dump (Afterbirth+ 1.7.9b / Repentance update).
- [x] Compute SHA-256 and internal NRO Build IDs for both `AfterbirthPlus.nro` and `Repentance.nro`; see [SWITCH_RUNTIME_FINDINGS.md](SWITCH_RUNTIME_FINDINGS.md). The screenshot's DBI update BID is not mapped to those internal IDs yet.
- [x] Keep AB+ and Repentance modules as separate named targets.

## Milestone 1 — standalone Lua
- [ ] Select and pin a Lua 5.1 source/port compatible with devkitPro/libnx.
- [x] Run a standalone NRO which loads `/switch/TheModdingOfIsaac/test.lua` (user-confirmed Switch photo).
- [x] Show visible Lua/test output on the Switch (photo reports 9 specialist-mock checks passing).
- [ ] Test a syntax error and missing-file case. The prior screenshot does not verify these.
- [x] Keep this milestone outside Isaac; the standalone interpreter result does not imply process injection or Isaac APIs.

## Milestone 2 — host integration feasibility
- [ ] Research supported homebrew/overlay/sysmodule approaches and what process-memory access they can provide on this setup.
- [ ] Select an approach based on documented facilities, not assumed symbol offsets.
- [x] Inspect both target NROs and disassemble selected manager, game, room, entity, and mod-manager paths; see [SWITCH_RUNTIME_FINDINGS.md](SWITCH_RUNTIME_FINDINGS.md).
- [ ] Find and validate a minimal non-invasive game-host probe; don't mutate game state initially.

## Milestone 3 — minimal Isaac API bridge
- [ ] Read current room entity list safely.
- [ ] Detect a collectible pickup by type, variant and subtype.
- [ ] Dispatch a Lua callback on a verified game update path.
- [ ] Keep the bridge disabled if target build ID differs or validation fails.

## Milestone 4 — Specialist subset
- [ ] Map the mod's item IDs for the exact target game/DLC.
- [ ] Add costume/animation operations.
- [ ] Add music play/restore operations.
- [ ] Verify repeated pickup/room transition/removal cases.
- [ ] Add compatibility table and docs.

## Acceptance and safety
- [ ] A clean boot works without the mod loader.
- [ ] Removing one folder/file fully disables the experiment.
- [ ] Test logs include exact target build ID.
- [ ] No patch described as tested until the user confirms actual device test.
- No game patch or purportedly playable Specialist package is produced yet: script execution, Isaac callback/API support, and resource redirection are unverified.
