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

## Prepared mod
Local folder: `outputs/specialistforgooditems-switch-prep`. It contains 101 copied files, with Lua 5.1 syntax edits in main.lua and main.lua.original preserved. The mod needs Isaac callbacks, player/room/pickup APIs, item config, costume/music registration, JSON persistence, and optional Mod Config Menu. The user's Switch photo showed 9 passing standalone mock scenarios and explicitly identified the test as not an Isaac mod.

## Evidence and local files
- `ghidra-analysis/SWITCH_CALLFLOW_EVIDENCE.md`
- `outputs/specialistforgooditems-switch-prep/SWITCH_PORT_STATUS.md`
- `outputs/specialistforgooditems-switch-prep/SWITCH_BRIDGE_CONTRACT.md`
- Scanner: `ghidra-analysis/scripts/inspect_nro_mod_calls.py`
- Census output: `ghidra-analysis-symbolized/ModCallCensus.txt`
- Room call scan: `ghidra-analysis-symbolized/RoomUpdateCallerScan.txt`
- Full local handoff: `CONTINUATION_HANDOFF.md`

## Continue from here
1. Inspect indirect references and strings around workshop_update_thread and determine if it is launched or related to mod loading.
2. Identify the actual module load/runtime path; check whether SaltyNX can see this game's Repentance module. NX-FPS reports some NRO-hosted frameworks are unsupported by external plugins; this is a warning, not proof about Isaac.
3. Determine whether LayeredFS can add the mod's XML/assets for this exact build. Resource overrides cannot execute this Lua.
4. Implement and test a verified in-game bridge before producing an installable package.
5. The user was asked for one SD-root photo showing atmosphere and, if present, SaltySD/plugins. They had not answered at handoff; avoid asking for more installations.

## Workspace caveat
The Codex workspace is not a Git checkout. The latest local call-census edits are recorded in this draft PR branch only if verified after pushing; do not assume they are present remotely.
