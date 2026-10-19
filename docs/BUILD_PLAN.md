# Build plan

## Milestone 0 — environment and target identity
- [ ] Record Atmosphère, firmware, Hekate and EmuMMC setup (only versions relevant to compatibility).
- [ ] Identify launched title and target NRO from the user's own update dump.
- [ ] Compute SHA-256, NRO ModuleId/build ID and section layout.
- [ ] Keep AB+ and Repentance variants as separate named targets.

## Milestone 1 — standalone Lua
- [ ] Select and pin a Lua 5.1 source/port compatible with devkitPro/libnx.
- [ ] Build minimal NRO which loads `/switch/TheModdingOfIsaac/test.lua` or a project-specific SD path.
- [ ] Print a visible "Lua OK" marker and append stdout/results to a log.
- [ ] Test a syntax error, missing file, and simple table/callback script.
- [ ] No Isaac process injection/hook in this milestone.

## Milestone 2 — host integration feasibility
- [ ] Research supported homebrew/overlay/sysmodule approaches and what process-memory access they can actually provide on this setup.
- [ ] Select an approach based on documented facilities, not assumed symbol offsets.
- [ ] Inspect target NRO's ELF-like segments/ModuleId and disassemble functions around validated references.
- [ ] Use read-only instrumentation first; no costume/music/entity mutation.

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
