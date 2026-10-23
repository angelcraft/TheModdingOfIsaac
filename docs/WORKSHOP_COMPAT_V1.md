# Workshop compatibility prototype 0.8.0 (opt-in)

**Keep the verified 0.6.0/0.7.1 SD backups.** This is a separate experimental branch. A successful CI build is not proof of correct Switch execution.

## Design

- SaltyNX/SaltySD injects IsaacLuaBridge ELF and hosts Lua 5.1 in the exact verified Switch Repentance NRO.
- The plugin reads mods.txt plus up to four main.lua files during plugin startup, while SaltySD file IPC is available. Mods may use `require("folder.module")` for local `.lua` files only; module source is loaded and cached in that mod's private environment. No file IPC is used in the game's update callback.
- The working IsaacUpdate hook invokes registered Workshop handlers on the game update thread. MC_POST_NEW_ROOM is currently a candidate synthesized from a change in the already observed room pointer; its timing and reliability still require device validation. No new native call or memory write is used. The Snapshot reports registrations and successes separately for each implemented event. If at least one Workshop callback is registered, it temporarily skips the old Specialist on_update logic to keep the smoke test isolated. Without mod callbacks, legacy behavior is unchanged.
- Each mod gets a distinct `_G` plus shallow copies of mutable `math`, `string`, `table`, `coroutine`, `ModCallbacks`, and `IsaacBridge` tables. Lua functions and VM-level state such as the pseudorandom generator remain shared; this is namespace separation, NOT a secure sandbox. Only use mods you trust.
- The read-only IsaacModuleSnapshot 0.8.0 reads Workshop counts via a versioned witness address written to bridge-startup.log.
- Audio, animation and sprite assets use Atmosphere LayeredFS. PC Workshop assets are NOT automatically converted or registered by this build.

## Install optional smoke test

For the partial ItemConfig API, copy your own extracted `resources/items_metadata.xml` to `sd:/switch/TheModdingOfIsaac/items_metadata.xml`. The XML is read at plugin startup; if it is missing or invalid, `GetCollectible(id)` returns `nil`. This metadata file is intentionally not included in the repository or build artifact.

Install the experimental artifact ELF at:

    sd:/SaltySD/plugins/010021C000B6A000/IsaacLuaBridge.elf

Preserve your previous ELF outside the SaltySD plugin directory, and keep bridge.lua beside the ELF.
To enable the demo, copy artifact examples to:

    sd:/switch/TheModdingOfIsaac/mods.txt
    sd:/switch/TheModdingOfIsaac/mods/demo/main.lua

mods.txt must contain one lowercase mod folder id per line. Example: demo.
The demo also checks `Isaac.GetItemConfig()` during startup and should log `Workshop ItemConfig: PASS (id 4 quality 4; id 502 quality 1)` in `bridge-startup.log` when the metadata file is read correctly.

The Switch log from 2026-10-23 confirms `ItemConfig: loaded 721 collectible qualities` and the demo returned quality 4 for ID 4 and quality 1 for ID 502. The Specialist bridge Lua now queries this same ItemConfig for each pedestal and selects only quality-4 items; the temporary any-pedestal mode and the ID 502 exception are removed. To test legacy Specialist behavior, disable `mods.txt` first: when Workshop callbacks are active, the loader intentionally suppresses the legacy Specialist `on_update` path.
Without mods.txt, the legacy bridge remains the only Lua script. Maximum 4 mod folders, 32 registered callbacks, 64 KiB per main.lua/module, 16 required modules and 256 KiB total Lua source per mod. The bundled two-mod test mutates one mod's globals and API/library tables, then requires the second mod to log `Workshop isolation: PASS`; Snapshot should show `registered_post_update=2` and `registered_post_new_room=2`.
Close and restart Isaac fully. Enter a room, let callbacks run, then run the Snapshot 0.8.0 using the existing safe Homebrew method.
Look for workshop_loaded=1, registered_post_update=1 and increasing workshop_callback_success. In Workshop-only test mode bridge_stage may stay at 4, because the old Specialist on_update callback is intentionally skipped. A real device test has not yet occurred.
To disable the demo, rename mods.txt; to return fully to a known installation, restore your saved ELF.

## Specialist event and API inventory

This inventory comes from the user's local original Workshop `main.lua`; the proprietary Workshop source and assets are not copied into this repository. It registers six event IDs:

| Event | What the original handler does | Loader status |
| --- | --- | --- |
| `MC_POST_UPDATE` | Counts collectible pedestals and runs eligibility/dance logic when the count changes | Registered and dispatched every update; hardware evidence exists for the earlier single-mod demo, not for this current two-mod build or for unmodified Specialist |
| `MC_POST_NEW_ROOM` | Resets the pedestal count and removes the dance costume | Registered, but synthesized from a room-pointer change; exact timing and device behavior remain unverified |
| `MC_USE_ITEM` | Re-scans after an active item is used | Unsupported |
| `MC_USE_CARD` | Re-scans after a card is used | Unsupported |
| `MC_POST_GAME_STARTED` | Loads saved configuration | Unsupported |
| `MC_PRE_GAME_EXIT` | Saves configuration | Unsupported |

The original declares handlers with colon syntax, such as `function Epic:OnGameUpdate()`, and registers the resulting functions as `Epic.OnGameUpdate`. The loader stores the mod table and supplies it as argument 1 (the Lua `self`). That narrow calling convention is implemented. The start/exit callbacks also need their expected lifecycle and event arguments; they are not made compatible just by accepting their IDs.

### Static evidence for collectible configuration

The local Repentance NRO used for the current catalogue has SHA-256 `CC363208C220F65547EDDA6D8268B3B6C0F7373B14C91AE6FFB378018DE4B66A`, matching the NRO in `inputs/update-romfs/Repentance.nro`. Its dynamic symbol table contains a defined `IsaacRepentance::ItemConfig::GetCollectible(int)` symbol at file offset `0x3C0C50` (76 bytes); the static catalogue reports 517 direct BL sites to its PLT import, mostly from game-owned functions such as `Entity_Player::EvaluateItems`.

This proves the method exists and is used internally in this exact binary. It does **not** prove a safe way to obtain the `ItemConfig` instance, its ABI contract for an injected caller, or the object's lifetime on the update thread. The bridge therefore continues to read the user's extracted collectible quality metadata from SD and does not call this native method. No offset or call target should be used until the object acquisition path and call contract are independently verified.

### Original Specialist API requirements

| Original API or value | Use in the source | Current 0.8.0 status |
| --- | --- | --- |
| `RegisterMod(name, 1)`, `mod:AddCallback(id, fn)` | Create `Epic`; register handlers | Narrow subset implemented. Callback methods receive the mod table as `self`. |
| `Game()` | Cached as `game` | Unsupported; no game object is exposed |
| `game:GetNumPlayers()`, `game:GetPlayer(index)` | Apply/remove costumes and choose character music | Unsupported |
| `player:GetPlayerType()`, `GetCollectibleNum(id, true)` | Character selection and item-costume checks | Unsupported |
| `player:AddNullCostume(id)`, `TryRemoveNullCostume(id)` | Start/stop the dance and item costume | Unsupported; animation must be applied using verified player/costume behavior |
| `game:GetRoom()`, `room:GetType()`, `room:PlayMusic()` | Treasure-room blind-item exception; restore room music | Unsupported |
| `game:GetLevel():GetStageType()`, `GetCurses()` | Repentance-stage and Curse of the Blind checks | Unsupported |
| `Isaac.FindInRadius(Vector(...), radius, EntityPartition.PICKUP)` | Enumerate pickups in the room | Unsupported; the current custom pedestal reader is not the PC entity-enumeration API |
| Entity `Variant`, `SubType`, `Position` | Filter collectible pedestals and test alt-path visibility | Unsupported as PC-compatible entity objects |
| `PickupVariant.PICKUP_COLLECTIBLE`, `EntityPartition.PICKUP` | Pickup filtering and search mask | Unsupported |
| `Isaac.GetItemConfig():GetCollectible(id).Quality` | Quality threshold and saved per-item overrides | Partial: reads exact `items_metadata.xml` qualities; `Name` is not exposed until a localization source is verified |
| `CollectibleType.NUM_COLLECTIBLES` | Bound a config scan | Unsupported |
| `Isaac.GetCostumeIdByPath(path)` | Resolve the character/item `.anm2` costume | Unsupported; LayeredFS asset installation is a separate requirement |
| `Isaac.GetMusicIdByName(name)` | Resolve base and character-specific songs | Unsupported |
| `MusicManager():GetCurrentMusicID()`, `Play(id, 1)`, `UpdateVolume()` | Replace current track and adjust volume | Unsupported; no compatible music manager is exposed |
| `require("json")`, `json.encode/decode` | Serialize saved settings and item overrides | Unsupported |
| `Epic:HasData()`, `LoadData()`, `SaveData(data)` | Persist configuration across runs | Unsupported |
| `PlayerType.*`, `StageType.STAGETYPE_REPENTANCE`, `RoomType.ROOM_TREASURE` | Character/stage/room branches | Unsupported constants |
| `Vector(x, y)` | Search origin and position comparisons | Unsupported |

The source also uses standard Lua libraries (`pairs`, `ipairs`, `table.insert`, `math.random`, `tostring`) and optional `ModConfigMenu`. The base-library calls are not Isaac bindings; the settings UI is optional, but `require("json")` is unconditional.

### What the bridge actually exposes

The prototype implements `RegisterMod`, `mod:AddCallback`, the two callback constants above, Lua 5.1 base libraries, a partial `Isaac.GetItemConfig():GetCollectible(id)` (fields `ID` and `Quality` only), and a custom `IsaacBridge` table. Its `GetRoomCollectibles`, `SetSpecialistDance`, and `SetSpecialistMusic` functions are project-specific helpers, not substitutes for the PC `Game`, `Isaac`, `EntityPlayer`, config, or `MusicManager` APIs. In Workshop mode the legacy Specialist script's `on_update` is skipped while any Workshop callback is registered, so the PC `main.lua` is not currently running alongside those helpers.

Do not invoke all callback IDs with fabricated parameters. That would only exercise our dispatcher and could call handlers in invalid game states. Add events one at a time, then count their natural trigger on-device and validate callback arguments against the PC API contract. Keep unsupported IDs unavailable until both trigger and argument behavior are known.

## Supported PC Lua subset in this build

- RegisterMod(name, 1)
- mod:AddCallback(ModCallbacks.MC_POST_UPDATE, fn)
- mod:AddCallback(ModCallbacks.MC_POST_NEW_ROOM, fn) (candidate room-transition signal; not yet confirmed on hardware)
- Basic Lua 5.1 functions and math/string/table/coroutine libraries.
- Startup-only local `require("folder.module")` resolving to `<mod folder>/folder/module.lua`. It is not Lua's `package` loader and does not provide built-in `json`.
- `Isaac.GetItemConfig():GetCollectible(id)` with exact `ID` and `Quality` loaded from `sd:/switch/TheModdingOfIsaac/items_metadata.xml`; absent/unknown entries return `nil`, and localized names are not fabricated.
- Existing IsaacBridge functions still exist, but they are NOT PC Isaac functions.

Unsupported: Game(), Isaac.GetRoomEntities, `Isaac.GetItemConfig().GetCollectible(id).Name`, MusicManager(), Isaac.GetCostumeIdByPath, MC_POST_NEW_LEVEL, MC_POST_PICKUP_INIT, MC_POST_PLAYER_UPDATE, MC_USE_ITEM, MC_USE_CARD, MC_POST_GAME_STARTED, MC_PRE_GAME_EXIT, `require("json")` unless the mod supplies a local compatible `json.lua`, dynamic/native module loading, file IO, save data, other callbacks, automatic resource merging and rendering/music imports.
The original PC Specialist main.lua therefore cannot run unmodified. This is deliberately a small loader proof of concept; no claims about native music or animation compatibility.

## Catalog calls from the original NRO

The Snapshot displays real-time counters for our own Lua callbacks. It cannot safely trace every native call of a live game.
Use the offline, read-only extractor for a broad static map of functions and calls from the exact Repentance.nro:

    python isaac-module-snapshot/tools/catalog_nro_calls.py path/to/Repentance.nro --output path/to/isaac-call-catalog

Input NRO is kept on your PC and SHA-256 checked. No game executable, assets or machine code bytes are uploaded to GitHub.
The call table now includes a symbol label for the caller and direct target when available, the resolution source, and the defined body offset when an imported PLT name matches a defined function in this exact NRO. A static room-transition path is visible from `RoomTransition::Update` through `RoomTransition::change_room`, `Game::ChangeRoom(int,int)`, and `Level::ChangeRoom(int,int)`. This is a candidate for targeted follow-up, not proof of runtime callback timing or a safe hook contract. `Entity_Pickup::Init` and `Entity_Pickup::Update` are named symbols too, while virtual calls remain unresolved by this catalog.\n\nThe output directory contains plt_imports.csv (validated imports), dyn_symbols.csv (named dynamic symbols), calls.csv (direct BL sites and indirect BLR sites), and summary.json.
BLR target addresses cannot be determined statically, and the list does not prove signatures, C++ object layouts, runtime execution or PC mod API semantics. Those must be verified individually before issuing native calls.

## Rollback

If Isaac freezes or crashes, restore the saved ELF and reboot/relaunch normally. Do not repeatedly run the same crashing build.

