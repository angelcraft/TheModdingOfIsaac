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

- In the Repentance module, `Manager::Update` directly calls `Game::Update`; `Room::Update` directly calls `EntityList::Update(bool)`.
- The `EntityList::Update(bool)` virtual dispatch at object-vtable offset `+0x20` resolves through the `Entity` and `Entity_Pickup` vtables to `Entity::Update` and `Entity_Pickup::Update`. This confirms that pickups have a native update path. A complete per-frame path from `Game::Update` to `Room::Update` is still not established.
- `ModManager::RunModScripts` at `0x421838` is a single `RET`; `ModManager::RunModScript(string&)` at `0x42183C` returns false. The base module's `RunModScript` also returns false.
- `UpdateWorkshopMods` at `0x42311C` is a single `RET`. `TryRedirectPath` is a trampoline to an unresolved indirect target, so a working mod-resource redirection path has not been demonstrated.
- Neither module contains checked Lua/Isaac-mod-runtime strings such as `luaL_newstate`, `luaL_loadfile`, `lua_pcall`, `RegisterMod`, or `MC_POST_UPDATE`. This is negative string evidence only; stripped or statically linked code cannot be excluded from strings alone.
- The Switch standalone Lua test shown by the user proves that its Lua interpreter can run independently. It does not prove a Lua state, callback API, or Isaac object access exists inside the game.

REA's Windows Ghidra provider rejected these NROs as unsupported. Static analysis was done with the installed local Ghidra against copies/projects; neither game NRO was modified.

## Porting decision

Changing `RunModScript` to return true would only change a result flag. It would not create a Lua VM, implement the callbacks and Isaac API used by Specialist, or register costume/music resources. LayeredFS can supply replacement files, but no working mod-resource path is verified here. Therefore the prepared Lua source is not an installable or playable Isaac Switch mod.

The alternative is a separate native plugin that reimplements the relevant Specialist behavior. That requires a Switch AArch64 homebrew toolchain, a verified plugin/hook interface and target-build guard, then on-device testing. This environment currently has no AArch64 Switch compiler/build tools. No IPS, modified game NRO, or inert ZIP presented as playable has been produced.

## Evidence files

- Local call-flow notes: `ghidra-analysis/SWITCH_CALLFLOW_EVIDENCE.md`
- Prepared Lua status and API requirements: `outputs/specialistforgooditems-switch-prep/SWITCH_PORT_STATUS.md`
- Bridge API inventory: `outputs/specialistforgooditems-switch-prep/SWITCH_BRIDGE_CONTRACT.md`
