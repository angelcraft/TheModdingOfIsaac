# ChatGPT continuation handoff

## Goal
Port Steam Workshop **The Specialist for Good Items** (2575911103) so it runs inside The Binding of Isaac: Repentance on the user's Nintendo Switch. Deliver a verified game-installable package. A standalone mock or inert LayeredFS ZIP does not satisfy the goal.

## Safety constraint
Do not create or install an NRO/IPS/native hook until the exact build identity, real call flow, ABI, displaced instructions, and return path are independently verified. Build identity is known; the hook contract is not. Returning true from RunModScript would not implement Lua execution or Isaac APIs.

## Exact target
- Game version shown on Switch: 1.7.9b. Update title ID 010021C000B6A800.
- Repentance NRO: 11,210,752 bytes; SHA-256 CC363208C220F65547EDDA6D8268B3B6C0F7373B14C91AE6FFB378018DE4B66A; internal Build ID 91C73FDD575061318D68886316AFEAC72388B2AB.
- Local NRO path: `C:\Users\User\Desktop\Switch\The Binding of Isaac_ Afterbirth+ 1.7.9b [010021C000B6A800][v524288][UPD]\The Binding of Isaac_ Afterbirth+ 1.7.9b [010021C000B6A800][v524288][UPD]\Program #0\1\.nro\Repentance.nro`.
- DBI screenshot BID B6E5BDB9DC12E1D1 is not mapped to the internal NRO Build ID.
- Update tree includes AfterbirthPlus.nro, .nrr registration files, kage_mount_points.dat, and resource/RP patch trees.

## Findings
- ModManager::RunModScripts at 0x421838 is RET; RunModScript(string&) at 0x42183C returns false; UpdateWorkshopMods at 0x42311C is RET.
- Hash-locked scan of executable text 0x80..0x68D000 found no direct BL callers of those three functions. Indirect dispatch remains unknown.
- workshop_update_thread (0x422D50) has no direct BL caller in that range; its direct callees include XML parsing, ControllerButtonMap::Load, ANM2, and UI helpers. Symbol name alone does not prove a Workshop download path.
- Manager::Update -> Game::Update and Game::Update -> PlayerManager::Update are confirmed. Room::Update -> EntityList::Update and pickup virtual update dispatch are confirmed. A normal gameplay-frame route through Room::Update remains unproved.
- TryRedirectPath (0x41FB04) is an 8-byte trampoline to an unresolved indirect target. A mod resource redirect is not established.
- No game NRO, IPS, plugin, or playable package has been created.

## New lead: generic NRO load callbacks (source verified, target not verified)
- REA is connected in the current agent session but its Windows Ghidra provider rejects this Switch .nro as an unsupported binary format. Use the existing local Ghidra project/scanners for this artifact; do not retry the same rejected REA open.
- The official `ultimate-research/nro-hook-plugin` source advertises centralized hooks for NRO loads and requires cargo-skyline. Its `src/lib.rs` replaces Skyline's `nn::ro::LoadModule`, calls the original, then builds `NroInfo` and invokes registered load callbacks: https://github.com/ultimate-research/nro-hook-plugin/blob/master/src/lib.rs
- This is a concrete candidate for observing a game that dynamically loads its code as an NRO; it avoids assuming SaltyNX's ordinary plugin loader can see NRO-hosted frameworks. It does NOT prove that Isaac's Repentance.nro passes through `ro::LoadModule`, that Skyline can load in this title, or that installing a hook is safe. The target call path, target ABI, hook/displaced-instruction contract, and return path remain unverified; do not build/install it yet.
- A new read-only scanner, `ghidra-analysis/scripts/inspect_nro_loader_imports.py`, verified the exact NRO SHA-256, parsed all 7,442 RELA import entries, and found no imported symbol matching `nn::ro`, `roLoadModule`, `roUnloadModule`, `ldrRo`, `LoadModule`, or `NroInfo`. This is bounded evidence about the NRO's own imported symbols only; it does not cover the outer program loader, indirect service resolution, or runtime module-load behavior.
- SaltyNX's own README recommends ExLaunch for title-specific injection. NX-FPS's archived README lists several games whose framework is inside NROs as unsupported by SaltyNX external plugins. Those are analogous cases, not proof about Isaac: https://github.com/masagrator/SaltyNX and https://github.com/masagrator/NX-FPS
- ExLaunch's config distinguishes load kind `Module` from `AsRtld` and `Kip`, but that setting alone does not establish compatibility with this title's dynamic NRO or runtime: https://github.com/shadowninja108/exlaunch/blob/main/config.mk

## Prepared mod
Local folder: `outputs/specialistforgooditems-switch-prep`. It contains 101 copied files, with Lua 5.1 syntax edits in main.lua and main.lua.original preserved. The mod needs Isaac callbacks, player/room/pickup APIs, item config, costume/music registration, JSON persistence, and optional Mod Config Menu. The user's Switch photo showed 9 passing standalone mock scenarios and explicitly identified the test as not an Isaac mod.

## Evidence and local files
- `ghidra-analysis/SWITCH_CALLFLOW_EVIDENCE.md`
- `outputs/specialistforgooditems-switch-prep/SWITCH_PORT_STATUS.md`
- `outputs/specialistforgooditems-switch-prep/SWITCH_BRIDGE_CONTRACT.md`
- Scanner: `ghidra-analysis/scripts/inspect_nro_mod_calls.py`
- NRO loader import scan: `ghidra-analysis/scripts/inspect_nro_loader_imports.py` (exact-hash, 7,442 relocation entries)
- Census output: `ghidra-analysis-symbolized/ModCallCensus.txt`
- Room call scan: `ghidra-analysis-symbolized/RoomUpdateCallerScan.txt`
- Full local handoff: `CONTINUATION_HANDOFF.md`

## Continue from here
1. Use the existing local Ghidra project (no new analysis setup) to finish inspecting indirect references and any thread-creation call sites around workshop_update_thread. The game NRO itself has no obvious direct RO/NRO loader imports; establish the outer program's load path separately.
2. Establish from the exact game package/runtime evidence whether Repentance.nro is dynamically loaded through nn::ro::LoadModule and whether a Skyline plugin can be loaded in this title. Do not infer this from the plugin's README, .nrr presence, or another game's compatibility.
3. Determine whether LayeredFS can add the mod's XML/assets for this exact build. Resource overrides alone cannot execute this Lua.
4. Implement and test a verified in-game bridge before producing an installable package.
5. The user was asked for one SD-root photo showing atmosphere and, if present, SaltySD/plugins. They had not answered at handoff; avoid asking for more installations.

## Workspace caveat
The Codex workspace is not a Git checkout. The latest local call-census edits are recorded in this draft PR branch only if verified after pushing; do not assume they are present remotely.
