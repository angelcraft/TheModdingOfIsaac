# Workshop compatibility prototype 0.8.0 (opt-in)

**Keep the verified 0.6.0/0.7.1 SD backups.** This is a separate experimental branch. A successful CI build is not proof of correct Switch execution.

## Design

- SaltyNX/SaltySD injects IsaacLuaBridge ELF and hosts Lua 5.1 in the exact verified Switch Repentance NRO.
- The plugin reads mods.txt plus up to four main.lua files during plugin startup, while SaltySD file IPC is available; no file IPC is used in the game's update callback.
- The working IsaacUpdate hook invokes registered Workshop MC_POST_UPDATE handlers on the game update thread. If at least one Workshop callback is registered, it temporarily skips the old Specialist on_update logic to keep the smoke test isolated. Without mod callbacks, legacy behavior is unchanged.
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

## Supported PC Lua subset in this build

- RegisterMod(name, 1)
- mod:AddCallback(ModCallbacks.MC_POST_UPDATE, fn)
- Basic Lua 5.1 functions and math/string/table/coroutine libraries.
- Existing IsaacBridge functions still exist, but they are NOT PC Isaac functions.

Unsupported: Game(), Isaac.GetRoomEntities, MusicManager(), Isaac.GetCostumeIdByPath, MC_POST_NEW_ROOM, require, file IO, save data, other callbacks, automatic resource merging and rendering/music imports.
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

