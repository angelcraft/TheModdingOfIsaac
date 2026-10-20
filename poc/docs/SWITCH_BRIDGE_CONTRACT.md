# Switch bridge contract for The Specialist for Good Items

This is an implementation checklist for the prepared Workshop 2575911103 source. It does not claim that the Switch game exposes these APIs.

## Required Lua host

The host must create a Lua 5.1 state, load the mod's `main.lua`, provide `require("json")`, and implement `RegisterMod(name, apiVersion)`. The returned mod object needs `AddCallback`, `SaveData`, `LoadData`, and `HasData`.

The script registers:

- `MC_POST_UPDATE`: scan room pickups.
- `MC_POST_NEW_ROOM`: clear costume state.
- `MC_USE_ITEM` and `MC_USE_CARD`: rescan.
- `MC_POST_GAME_STARTED`: load settings.
- `MC_PRE_GAME_EXIT`: save settings.

The callback dispatcher must invoke these at verified points in the game's update lifecycle with compatible arguments.

## Required game API

- `Isaac.GetCostumeIdByPath`, `Isaac.GetMusicIdByName`,
  `Isaac.GetItemConfig():GetCollectible`, and `Isaac.FindInRadius`.
- `Game():GetNumPlayers`, `GetPlayer`, `GetLevel`, and `GetRoom`.
- Level: `GetCurses`, `GetStageType`; room: `GetType`, `PlayMusic`.
- Player: `GetPlayerType`, `GetCollectibleNum`, `AddNullCostume`,
  `TryRemoveNullCostume`.
- `MusicManager():GetCurrentMusicID`, `Play`, and `UpdateVolume`.
- Enums and values: `PlayerType`, `CollectibleType`, `PickupVariant`,
  `EntityPartition`, `RoomType`, `StageType`, `ModCallbacks`.
- Entity fields `Variant`, `SubType`, `Position.X/Y` and item fields
  `Name`, `Quality`.

## Required assets

The mod's `.anm2`, `.png`, and `.ogg` assets need registered resource paths compatible with the merged `costumes2.xml` and `music.xml`. The extracted update contains a `resources/` tree, but its LayeredFS behavior for added resources is not verified.

## Evidence and implementation boundary

The exact Switch NRO contains the `RunModScript` string but a literal scan found no `luaL_newstate`, `luaL_loadfile`, `lua_pcall`, `lua_call`, `lua_pushstring`, `luaopen_base`, `RegisterMod`, `MC_POST_UPDATE`, or `GetCostumeIdByPath` strings. This negative scan does not prove Lua is absent if fully stripped or linked without names.

Ghidra reports `RunModScripts` as an immediate return and `RunModScript` as returning zero; it found no incoming direct references. Returning true would not create a VM, load source, expose Isaac APIs, or dispatch callbacks.

The project README records earlier IPS/NRO hook experiments and a passthrough diagnostic crashing the game. Those patches were removed and the Switch installation was cleaned. Do not create or install a native patch until the exact target, live call flow, ABI, displaced instructions, and return path are independently verified.

The Lua 5.1 Switch NRO is an isolated homebrew test, not an Isaac loader.
