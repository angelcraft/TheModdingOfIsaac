# Repentance integration feasibility — binary inspection

Updated: 2026-10-19

## Selected target

The user confirmed that the Repentance DLC is installed and enabled. Repentance is the primary research target; Afterbirth+ remains a separately versioned target. Do not copy addresses between binaries.

## Supplied NRO files — identity checks

The user supplied both files directly. Both parse as NRO0 and the header's declared image size matches the file length.

| Field | Afterbirth+ | Repentance |
|---|---:|---:|
| File | `AfterbirthPlus.nro` | `Repentance.nro` |
| File size / declared NRO size | 8,523,776 bytes | 11,210,752 bytes |
| SHA-256 | `705402b213072f67b561d011302c315448f47a16b9187b15b8823e003f6ccdad` | `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a` |
| NRO build ID from header (first 20 bytes at 0x40) | `0F65948274CE60ECA59C1E55B09C54D0607F3C57` | `91C73FDD575061318D68886316AFEAC72388B2AB` |

The Repentance build ID above is written as the 20 header bytes, without the 12 zero padding bytes. A matching header identifies the supplied NRO, not necessarily the exact file loaded at runtime by the user's console; runtime identity still needs confirmation.

## Mod-manager entry points inspected in the raw bytes

Symbol values from the prior symbol inventory were checked directly against the supplied NRO files. They are leads for static research, not a safe patch recipe.

| Target | Symbol value | Size | Raw AArch64 bytes | Initial interpretation |
|---|---:|---:|---|---|
| Afterbirth+ `ModManager::RunModScript(std::string&)` | `0x24FA68` | 8 | `E0 03 1F 2A C0 03 5F D6` | `mov w0, wzr; ret` — returns zero immediately |
| Repentance `IsaacRepentance::ModManager::RunModScripts()` | `0x421838` | 4 | `C0 03 5F D6` | `ret` — returns immediately |
| Repentance `IsaacRepentance::ModManager::RunModScript(std::string&)` | `0x42183C` | 8 | `E0 03 1F 2A C0 03 5F D6` | `mov w0, wzr; ret` — returns zero immediately |

A scan for the literal strings `lua_State`, `luaL_newstate`, `lua_pcall`, `RegisterMod`, `ModCallbacks`, `AddCallback`, and `.lua` found no matches in either NRO. This scan is heuristic: missing strings alone does not prove no Lua implementation exists anywhere. The immediate-return instructions, however, directly confirm that these particular mod-script entry points do not do the expected work in the inspected binaries.

### Completed Ghidra reference pass — Repentance (2026-10-19)

The supplied Repentance NRO was imported directly into Ghidra 12.1.4 as raw AArch64 little-endian. Automatic analysis completed successfully; Ghidra logged non-fatal invalid-PNG and register-context warnings.

- The completed static reference inventory reports no incoming code or data references to `RunModScripts` at `0x421838` or `RunModScript` at `0x42183C`.
- An independent instruction scan also found no direct AArch64 `B`/`BL` branches to those addresses.
- This makes both entries unsuitable as assumed hooks in this build. The result is limited to references resolved by this static analysis; dynamic dispatch or unresolved computed references remain possible.
- The analysis did resolve ordinary data references to selected functions such as `Room::PlayMusic` and `Entity_Pickup::Update`, but that alone does not establish their ABI, call context, or suitability as hooks.

The exact supplied file identity is SHA-256 `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`, Build ID `91C73FDD575061318D68886316AFEAC72388B2AB`. This still does not prove the console is loading the same build. No patch was produced.


**Consequence:** copying Workshop mods to the SD card will not by itself enable them. We need a native integration path, a Lua runtime, and an Isaac-API compatibility bridge. None of those is proven by the standalone Lua NRO POC.

## Positive native engine leads in Repentance

The same symbol inventory contains nontrivial native routines useful for a staged Specialist port. Their initial instruction bytes were checked against the supplied Repentance NRO:

| Candidate routine | Symbol value | Size | Initial bytes (hex) |
|---|---:|---:|---|
| `Entity_Pickup::Update()` | `0x26C9B8` | 11,860 | `EB 2B B8 6D E9 23 01 6D FD 7B 02 A9 FD 83 00 91` |
| `Entity_Player::AddNullCostume(eNullItemID)` | `0x291BBC` | 68 | `FD 7B BE A9 F3 0B 00 F9 FD 03 00 91 C9 40 00 F0` |
| `Entity_Player::TryRemoveCostume(eNullItemID)` | `0x2F2170` | 64 | `FD 7B BE A9 F3 0B 00 F9 FD 03 00 91 C9 3D 00 D0` |
| `Music::GetCurrentMusicID()` | `0x426C38` | 8 | `00 C8 43 B9 C0 03 5F D6` |
| `Music::Play(eMusic)` | `0x42721C` | 28 | `29 34 00 B0 29 25 43 F9 29 01 40 F9 08 9D 8C 52` |
| `Room::PlayMusic()` | `0x451834` | 1,732 | `FF C3 01 D1 E9 23 01 6D FD 7B 02 A9 FD 83 00 91` |

These entries show that some gameplay/costume/music implementation remains in the native game. They are **not yet approved hook points**: function semantics, object layout, calling convention, thread/frame context, static/global access and cleanup must be verified before invocation. The Lua-facing function names and native C++ methods are not automatically interchangeable.

## NRO header build IDs and Atmosphère

The official Atmosphère documentation describes NRO IPS/IPS32 patch lookup under:

```text
/atmosphere/nro_patches/<patchset name>/<nro build id>.ips
```

The 20-byte header IDs listed above are the relevant build-ID candidates for the supplied NROs; verify the console's actual loaded target and loader path before relying on them.

Reference: https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/ro.md

Patch delivery is not the same as code injection or API implementation. We have not identified a verified code cave, trampoline, safe executable allocation, or complete call path. Do not generate or install an IPS yet.

## Keep the runtime, bridge, and API separate

1. **Lua VM:** load a script, handle errors and callbacks. The current Switch NRO POC is standalone.
2. **Game bridge:** connect only after the Repentance code path is verified. Begin with a version marker and read-only room/entity data.
3. **Isaac compatibility API:** implement only documented calls required by a target mod and validate calling conventions, object lifetime and cleanup.

For broader vanilla Repentance mod compatibility, investigate Lua 5.3 as the language baseline before treating the current Lua 5.1 Switch port as sufficient. See `STEAM_MOD_COMPATIBILITY.md`.

## Reversible integration gates

- Gate A: build and run the standalone Lua NRO with `test.lua`. This proves only Lua executes in homebrew.
- Gate B: verify actual game identity, NRO build ID, symbol bytes and call flow.
- Gate C: establish whether Atmosphère's NRO patcher observes the exact loaded binary.
- Gate D: a reversible native version-marker/no-op probe; no entity writes.
- Gate E: read-only room/entity snapshot and collectible-pedestal predicate.
- Gate F: costume/animation/music actions for Specialist, one at a time.
- Gate G: a controlled mod directory loader plus a small, explicit vanilla API subset.

If a version check fails, the bridge must refuse to run. Rollback must remain as simple as removing the single experimental patchset.

## Current status

The two uploaded binaries have passed basic identity/header checks, and the immediate-return bytes described above were confirmed by direct inspection. The standalone Lua Switch POC is still separate from Isaac; CI previously showed its jobs queued, and this is not yet a verified hardware build. No Isaac IPS patch has been generated or installed.
