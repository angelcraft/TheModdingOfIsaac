# Repentance integration feasibility — research only

Updated: 2026-10-19

## Selected target

The user confirmed the Repentance DLC is installed and enabled. Repentance is the primary research target; Afterbirth+ remains a separately versioned target. See `REPLICATION_TARGET.md`.

The continuity ZIP includes `REPENTANCE_SYMBOLS.csv` (17,249 symbols) but **does not include the Repentance NRO bytes**. Its symbol inventory exposes these leads:

| Symbol | Recorded size | What this tells us |
|---|---:|---|
| `IsaacRepentance::ModManager::RunModScripts()` | 4 bytes | Tiny entry point; inspect raw instruction bytes/disassembly before drawing a conclusion. |
| `IsaacRepentance::ModManager::RunModScript(string&)` | 8 bytes | Tiny entry point; likely important, but size alone does not prove it's a stub. |
| `IsaacRepentance::Entity_Pickup::Update()` | 11,860 bytes | Candidate gameplay path for later investigation, not an approved hook. |
| `IsaacRepentance::Entity_Player::AddNullCostume(...)` | 68 bytes | Candidate costume operation; calling convention/object layout still unknown. |
| `IsaacRepentance::ItemConfig::LoadCostumes(...)` | 3,756 bytes | Candidate costume registration path, not verified for external assets. |
| `IsaacRepentance::Music::Play(...)` | 28 / 392 bytes | Candidate music operations; signatures and state restoration still need verification. |

Values/offsets from the CSV are leads only. Do not convert them into IPS file offsets or use them to patch the game. We need the exact NRO and must derive the module/build ID, section mapping, instruction bytes, file-offset mapping, calling conventions and return flow afresh.

## Atmosphère patch-delivery option

Official Atmosphère documentation says its `ro` reimplementation can apply IPS or IPS32 patches to an NRO when that NRO is loaded. The convention is:

```text
/atmosphere/nro_patches/<patchset name>/<nro build id>.ips
```

Reference: https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/ro.md

This makes an NRO patch **a possible delivery mechanism**, not an integration solution by itself. It does not automatically provide an extension point for arbitrary new code, allocate a safe executable code cave, embed a Lua VM, or implement Isaac callbacks. First we need to confirm that the exact game binary is loaded via a path where Atmosphère's NRO patcher actually sees it, and compute the correct NRO build ID (SHA-256 alone is not the patch filename).

## Separate the layers

1. **Lua VM:** interpret script bytes, report errors, and keep the callback core alive. The new Switch NRO POC is separate from Isaac.
2. **Game bridge:** acquire read-only room/pickup data from a validated Repentance build. No entity writes during the first instrumentation stage.
3. **Isaac compatibility API:** only after the bridge works, expose a narrow API and Specialist-specific adapter. A plain Lua VM does not have `Game()`, `Isaac`, `EntityType`, `PickupVariant`, `ModCallbacks`, `MusicManager()`, or costume/music registration.

## Reversible integration gates

- Gate A: compile and run the standalone Lua NRO with `test.lua`; this proves only Lua executes in homebrew.
- Gate B: inspect the exact Repentance NRO. Verify the reported symbol locations and inspect `RunModScripts`/ `RunModScript` bytes and disassembly; no inherited offsets.
- Gate C: confirm the NRO loading path and build ID / Atmosphère patch naming.
- Gate D: use a minimal host-side no-op/read-only probe; don't change costumes or music.
- Gate E: read-only list of room entities and a verified collectible-pedestal predicate.
- Gate F: Specialist action layer and asset/resource strategy.
- Any mismatch in target identity disables the experimental bridge; rollback remains removing the single experimental patchset/folder.

## Current limitation

The Lua POC CI run is queued. The current environment has no `lua`, `lua5.1`, or `luajit` executable, so its host tests could not be run locally. The Switch NRO has not been built or tested on hardware yet. No Isaac patch has been generated or installed.
