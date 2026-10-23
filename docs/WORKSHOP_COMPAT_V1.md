# Workshop compatibility prototype 0.8.0 (opt-in)

**Keep the verified 0.6.0/0.7.1 SD backups.** This is a separate experimental branch. A successful CI build is not proof of correct Switch execution.

## Design

- SaltyNX/SaltySD injects IsaacLuaBridge ELF and hosts Lua 5.1 in the exact verified Switch Repentance NRO.
- The plugin reads mods.txt plus up to four main.lua files during plugin startup, while SaltySD file IPC is available; no file IPC is used in the game's update callback.
- The working IsaacUpdate hook invokes registered Workshop handlers on the game update thread. MC_POST_NEW_ROOM is currently a candidate synthesized from a change in the already observed room pointer; its timing and reliability still require device validation. No new native call or memory write is used. The Snapshot reports registrations and successes separately for each implemented event. If at least one Workshop callback is registered, it temporarily skips the old Specialist on_update logic to keep the smoke test isolated. Without mod callbacks, legacy behavior is unchanged.
- A small per-mod global namespace omits io/os/package/debug, but this does NOT establish secure sandboxing: only use mods you trust.
- The read-only IsaacModuleSnapshot 0.8.0 reads Workshop counts via a versioned witness address written to bridge-startup.log.
- Audio, animation and sprite assets use Atmosphere LayeredFS. PC Workshop assets are NOT automatically converted or registered by this build.

## Install optional smoke test

Install the experimental artifact ELF at:

    sd:/SaltySD/plugins/010021C000B6A000/IsaacLuaBridge.elf

Preserve your previous ELF outside the SaltySD plugin directory, and keep bridge.lua beside the ELF.
To enable the demo, copy artifact examples to:

    sd:/switch/TheModdingOfIsaac/mods.txt
    sd:/switch/TheModdingOfIsaac/mods/demo/main.lua

mods.txt must contain one lowercase mod folder id per line. Example: demo.
Without mods.txt, the legacy bridge remains the only Lua script. Maximum 4 mod folders, 32 registered callbacks, 64 KiB per main.lua.
Close and restart Isaac fully. Enter a room, let callbacks run, then run the Snapshot 0.8.0 using the existing safe Homebrew method.
Look for workshop_loaded=1, registered_post_update=1 and increasing workshop_callback_success. In Workshop-only test mode bridge_stage may stay at 4, because the old Specialist on_update callback is intentionally skipped. A real device test has not yet occurred.
To disable the demo, rename mods.txt; to return fully to a known installation, restore your saved ELF.

## Specialist event and API inventory

The bundled PC Workshop source registers these six callbacks:

| Callback | Use in Specialist | Bridge status |
| --- | --- | --- |
| MC_POST_UPDATE | Scans room pickups and runs the eligibility/dance logic | Registered by the bridge; smoke test confirmed, Specialist path is not wired to PC APIs |
| MC_POST_NEW_ROOM | Resets item-count state and removes the costume | Candidate synthesized from room-pointer changes; device behavior not confirmed |
| MC_USE_ITEM | Runs room cleanup after item use | Unsupported |
| MC_USE_CARD | Runs room cleanup after card use | Unsupported |
| MC_POST_GAME_STARTED | Loads saved settings | Unsupported |
| MC_PRE_GAME_EXIT | Saves settings | Unsupported |

Specialist's callbacks are declared as methods (for example, `function Epic:OnGameUpdate()`) and are passed to `AddCallback` as `Epic.OnGameUpdate`. The bridge therefore supplies the mod table as the callback's first argument. The demo checks this contract.

Do not call every callback ID with fabricated parameters. That would test our Lua dispatcher, not establish that the game's event maps to that callback or that its arguments are correct. Extend the probe by registering one event at a time, observing independent Snapshot counters during the corresponding natural in-game action, and checking callback arguments against the PC source/API contract. Unknown event IDs stay unsupported until there is a verified trigger and signature.

Before Specialist can work unchanged, its source also needs APIs absent from this prototype, including room entity enumeration and classification, collectible config/quality, costume-path lookup and application, music lookup/playback/restore, save-data methods, `require("json")`, and globals such as `Vector`, `PlayerType`, `EntityPartition`, and `PickupVariant`. Implementing callbacks alone is insufficient.

## Supported PC Lua subset in this build

- RegisterMod(name, 1)
- mod:AddCallback(ModCallbacks.MC_POST_UPDATE, fn)
- mod:AddCallback(ModCallbacks.MC_POST_NEW_ROOM, fn) (candidate room-transition signal; not yet confirmed on hardware)
- Basic Lua 5.1 functions and math/string/table/coroutine libraries.
- Existing IsaacBridge functions still exist, but they are NOT PC Isaac functions.

Unsupported: Game(), Isaac.GetRoomEntities, MusicManager(), Isaac.GetCostumeIdByPath, MC_POST_NEW_LEVEL, MC_POST_PICKUP_INIT, MC_POST_PLAYER_UPDATE, MC_USE_ITEM, MC_USE_CARD, MC_POST_GAME_STARTED, MC_PRE_GAME_EXIT, require, file IO, save data, other callbacks, automatic resource merging and rendering/music imports.
The original PC Specialist main.lua therefore cannot run unmodified. This is deliberately a small loader proof of concept; no claims about native music or animation compatibility.

## Catalog calls from the original NRO

The Snapshot displays real-time counters for our own Lua callbacks. It cannot safely trace every native call of a live game.
Use the offline, read-only extractor for a broad static map of functions and calls from the exact Repentance.nro:

    python isaac-module-snapshot/tools/catalog_nro_calls.py path/to/Repentance.nro --output path/to/isaac-call-catalog

Input NRO is kept on your PC and SHA-256 checked. No game executable, assets or machine code bytes are uploaded to GitHub.
The output directory contains plt_imports.csv (validated imports), dyn_symbols.csv (named dynamic symbols), calls.csv (direct BL sites and indirect BLR sites), and summary.json.
BLR target addresses cannot be determined statically, and the list does not prove signatures, C++ object layouts, runtime execution or PC mod API semantics. Those must be verified individually before issuing native calls.

## Rollback

If Isaac freezes or crashes, restore the saved ELF and reboot/relaunch normally. Do not repeatedly run the same crashing build.

