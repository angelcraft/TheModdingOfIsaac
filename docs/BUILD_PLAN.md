# Build plan

## Milestone 0 — environment and target identity
- [x] Record console/CFW basics: Switch OLED, Atmosphère + Hekate, EmuMMC.
- [x] User confirms Repentance DLC installed and enabled; choose Repentance as the primary research target.
- [ ] Identify the exact launched title/NRO on device rather than infer from the update archive.
- [ ] Recalculate SHA-256, NRO ModuleId/build ID and section layout from the exact Repentance NRO.
- [ ] Keep AB+ and Repentance targets, resource trees and reports separate.

## Milestone 1A — host-side Lua logic
- [x] Add isolated callback core and mock Specialist trigger test.
- [x] Add tests for irrelevant pickups, a single start, same-pedestal deduplication, removal/new-room stop, and callback errors.
- [ ] Run tests under Lua 5.1 in GitHub Actions.
- [ ] Add syntax-error/missing-file cases to the later runtime smoke test.

## Milestone 1B — standalone Lua Switch runtime
- [x] Pin the Lua 5.1.5 Switch-port commit used by CI.
- [x] Add a libnx NRO that loads `/switch/TheModdingOfIsaac/test.lua`, prints errors/success, and has a bounded script read.
- [x] Add CI workflow to compile the NRO and package the SD layout.
- [ ] Confirm CI host tests pass.
- [ ] Confirm Switch NRO build passes and obtain artifact.
- [ ] Test `Lua OK`, missing script, syntax error, and callback script on the user's Switch.
- [ ] No Isaac process access in this milestone.

## Milestone 2 — host integration feasibility
- [x] Research Atmosphère's official NRO IPS/IPS32 patch path.
- [ ] Verify the exact game NRO loading path and calculate the matching NRO Build ID.
- [ ] Re-inspect Repentance `RunModScripts` and `RunModScript` from raw bytes and disassembly; symbol size alone is not proof of a stub.
- [ ] Assess the safest supported means of running native code in the game process without assuming overlay/sysmodule process-memory permissions.
- [ ] Select an approach based on observed loader behavior, exact build identity and reversible host-side tests.
- [ ] No game-state mutation until a read-only probe is validated.

## Milestone 3 — minimal Isaac API bridge
- [ ] Read current room entity list safely for the exact Repentance build.
- [ ] Detect collectible pickup by verified type, variant and subtype.
- [ ] Dispatch a Lua callback on a verified update path.
- [ ] Fail closed if target build ID differs or any validation fails.

## Milestone 4 — Specialist subset
- [ ] Verify Repentance item IDs for the configured Specialist list; do not copy AB+ IDs blindly.
- [ ] Add costume/animation registration.
- [ ] Add music play/restore.
- [ ] Verify repeated pickup, reroll, pedestal removal and room transition cases.
- [ ] Maintain separate compatibility tables for Repentance and AB+.

## Acceptance and safety
- [ ] Clean game boot works with all experimental files removed.
- [ ] Removing one experimental patchset/folder fully disables the integration.
- [ ] Tests/logs include exact target Build ID.
- [ ] No patch called tested until actually built and device-tested.
