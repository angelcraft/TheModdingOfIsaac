# Switch runtime findings: Repentance 1.7.9b

Date: 2026-10-20

## Exact files inspected

The user's update dump contains two NRO modules. These identities are from the files in the supplied update folder; hashes identify the inspected files independently of the Switch's displayed title version.

| File | Bytes | SHA-256 | Internal NRO Build ID |
| --- | ---: | --- | --- |
| `Repentance.nro` | 11,210,752 | `CC363208C220F65547EDDA6D8268B3B6C0F7373B14C91AE6FFB378018DE4B66A` | `91C73FDD575061318D68886316AFEAC72388B2AB` |
| `AfterbirthPlus.nro` | 8,523,776 | `705402B213072F67B561D011302C315448F47A16B9187B15B8823E003F6CCDAD` | `65948274CE60ECA59C1E55B09C54D0607F3C5700` |

The user's DBI image shows The Binding of Isaac: Afterbirth+ v1.7.9b and update BID `B6E5BDB9DC12E1D1`. The mapping between that BID and the two internal NRO Build IDs has not been established. The base title ID shown in the project notes is `010021C000B6A000`; the update content directory is `010021C000B6A800`.

## Static observations

- `Manager::Update` at `0x3F8DB8` directly calls `Game::Update` at `0x351884` (`0x3F9070`). `Game::Update` directly calls `PlayerManager::Update` at `0x43F83C` (`0x3521F8`).
- The exact `PlayerManager::Update` symbol range (`0x43F83C..0x43FA1B`) has seven direct imported calls: `Game::IsPaused`, `Entity_Player::IsFullSpriteRendering`, `TemporaryEffects::AddEffect`, two `ANM2::Update` calls, `Camera::smooth_focus_samples`, and `SoundEffects::GetAmbientSoundVolume`. It has no direct call or indirect `BLR` dispatch to `Room::Update` or `EntityList::Update`.
- A full NRO scan decoded direct AArch64 `BL` instructions and resolved their PLT imports. Direct calls to `Room::Update` appear in `Level::place_room` and `LevelGenerator` functions; none appear in `Manager::Update`, `Game::Update`, or `PlayerManager::Update`. This narrows the missing frame path but does not rule out indirect dispatch from another routine.
- The complete direct-call graph does contain transitive paths from `Game::Update` to `Room::Update`, including `Game::Update -> Level::Init -> Level::place_rooms -> LevelGenerator::BlockPosition -> Room::Update`, and a separate path through Dogma's boss-intro/room-generation code. These paths show that room setup can be reached from game update logic, but call-graph reachability does not prove that room/entity updates run every gameplay frame; the initialization and transition guards still need to be traced.
- `Room::Update` at `0x46D788` calls `EntityList::Update(bool)` through PLT at `0x46E6E4`; its indirect entity dispatch includes the pickup update path. This proves the room-to-entity path, not that it runs every gameplay frame.
- `ModManager::RunModScripts` at `0x421838` is a single `RET`; `ModManager::RunModScript(string&)` at `0x42183C` returns false. The base module's `RunModScript` also returns false.
- `UpdateWorkshopMods` at `0x42311C` is a single `RET`. `TryRedirectPath` is a trampoline to an unresolved indirect target, so a working mod-resource overlay path has not been demonstrated.
- Neither module contains checked Lua/Isaac-mod-runtime strings such as `luaL_newstate`, `luaL_loadfile`, `lua_pcall`, `RegisterMod`, or `MC_POST_UPDATE`. This is negative string evidence only; stripped or statically linked code cannot be excluded from strings alone.
- The Switch standalone Lua test shown by the user proves that its Lua interpreter can run independently. It does not prove a Lua state, callback API, or Isaac object access exists inside the game.

REA's Windows Ghidra provider rejected these NROs as unsupported. Static analysis was done with the installed local Ghidra against copies/projects; neither game NRO was modified.

## Integration options

- Atmosphere LayeredFS can replace files under a title's RomFS, but it cannot implement the missing Lua VM, callbacks, Isaac API, or the mod's pedestal-triggered dance logic.
- SaltyNX documents plugins for 64-bit games and recommends ExLaunch for game-specific code injection. Its plugin ecosystem also has known games where the relevant framework is contained in an NRO and external plugins are unsupported. This target is an NRO module, so SaltyNX plugin compatibility must be demonstrated before building or installing a plugin; no NRO-specific ExLaunch route is verified here. See [SaltyNX](https://github.com/masagrator/SaltyNX), [ExLaunch](https://github.com/shadowninja108/exlaunch), and [NX-FPS's NRO compatibility note](https://github.com/masagrator/NX-FPS).
- The environment has no local Switch AArch64 compiler/build tools. A hosted build would only solve compilation, not the missing NRO injection, call-flow, ABI, and return-path evidence.

## Porting decision

Changing `RunModScript` to return true would only change a result flag. It would not create a Lua VM, implement the callbacks and Isaac API used by Specialist, or register costume/music resources. A native plugin would be a separate port, not a conversion of this Lua package. Do not create an IPS or modified game NRO until the exact target identity, a real execution path, ABI, displaced instructions, and return path are independently verified.

No playable Switch package has been produced.

## Evidence files

- Local call-flow notes: `ghidra-analysis/SWITCH_CALLFLOW_EVIDENCE.md`
- Prepared Lua status and API requirements: `outputs/specialistforgooditems-switch-prep/SWITCH_PORT_STATUS.md`
- Bridge API inventory: `outputs/specialistforgooditems-switch-prep/SWITCH_BRIDGE_CONTRACT.md`
