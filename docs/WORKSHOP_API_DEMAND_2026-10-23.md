# Workshop API demand from the local corpus (2026-10-23)

This is a prioritization snapshot from the six Workshop mods installed locally under Steam App ID 250900. It records API names and event identifiers only; no Workshop Lua source or assets are copied here.

## Method and limits

- Start from each mod's `main.lua` and follow literal local `require("a.b")` references to matching `a/b.lua` files.
- Strip Lua comments and string literals before counting API references.
- Count a feature once per mod, not once per call. For `require`, inspect both literal branches without evaluating conditions; this can over-count code that the target will not select.
- This is a six-mod convenience sample, not a representative count of every Steam mod. Dynamic requires, `include`, generated Lua, and APIs constructed indirectly may be missed.
- The scan is read-only; no game binary, Workshop source, or assets are uploaded.

## Callback demand

| PC callback | Mods using it | Current bridge |
| --- | ---: | --- |
| `MC_POST_GAME_STARTED` | 5/6 | Unsupported |
| `MC_POST_NEW_ROOM` | 5/6 | Implemented as room-token change; device dispatch observed, exact event timing still needs validation |
| `MC_POST_NEW_LEVEL` | 4/6 | Unsupported |
| `MC_POST_RENDER` | 4/6 | Unsupported |
| `MC_POST_UPDATE` | 3/6 | Implemented; device witness recorded 3,446 update callback successes in the two-mod smoke test (derived from 3,448 total successes minus 2 room callbacks) |
| `MC_PRE_GAME_EXIT` | 3/6 | Unsupported |
| `MC_USE_ITEM` | 3/6 | Unsupported |
| `MC_USE_CARD` | 3/6 | Unsupported |
| `MC_POST_PLAYER_INIT` | 2/6 | Unsupported |
| `MC_POST_PICKUP_INIT`, `MC_POST_PICKUP_UPDATE`, `MC_PRE_PICKUP_COLLISION` | 1/6 each | Unsupported |

The 2026-10-23 device logs show 3,448 Workshop callback successes, zero callback errors, and two room callback successes. This confirms the current smoke-test dispatcher runs continuously on the Switch. It does not prove every PC callback's timing or argument semantics.

## Most reused game APIs

| PC API or constructor | Mods using it | Current bridge |
| --- | ---: | --- |
| `Game()` | 5/6 | Unsupported |
| `Isaac.GetPlayer` | 4/6 | Unsupported |
| `Isaac.LoadModData` | 4/6 | Unsupported |
| `Vector()` | 4/6 | Unsupported |
| `Isaac.GetRoomEntities` | 2/6 | Unsupported |
| `Isaac.FindInRadius` | 2/6 | Unsupported |
| `Isaac.GetItemConfig` | 2/6 | Partial: user-supplied metadata exposes collectible `ID` and `Quality` only |
| `Isaac.SaveModData` | 2/6 | Unsupported |
| `Isaac.WorldToScreen`, `Isaac.WorldToRenderPosition` | 2/6 each | Unsupported |
| `MusicManager()` | 1/6 | Unsupported |
| `Isaac.GetCostumeIdByPath`, `Isaac.GetMusicIdByName` | 1/6 each; Specialist | Unsupported |

Three mods require a local or built-in `json` module; three refer to `scripts.modconfig`, a dependency not found as a local module in those packages. The loader's local dotted `require` does not provide built-in modules automatically. The target environment now exposes `REPENTANCE=true`, and module path validation accepts safe `+` filename characters used by conditional AB+ / Repentance packages.

## PC API semantics to preserve during porting

The Isaac Lua reference documents `Isaac.GetPlayer(playerID)` with player IDs starting at 0 and shows iteration from 0 through `GetNumPlayers() - 1`. Its `Game:GetPlayer(index)` entry also warns that unused indices may return the last player. See the [Repentance Lua API reference](https://wofsauge.github.io/IsaacDocs/rep/Isaac.html#getplayer) and [Game reference](https://wofsauge.github.io/IsaacDocs/rep/Game.html#get-player).

The locally supplied Specialist source instead loops `playerNum = 1, game:GetNumPlayers()` before calling `game:GetPlayer(playerNum)`. That can skip player ID 0 and revisit the last player in co-op. The mod source remains local; this report records only the compatibility finding. The bridge has no `Game` or player wrapper yet. Preserve PC's zero-based IDs in any future implementation, then decide whether to add a narrow, documented source fix for Specialist.

These are PC API semantics from community-maintained documentation, not proof that our Switch wrapper has been implemented or verified. Validate the exact target behavior before exposing the wrapper.

## What this changes about prioritization

The next reusable API foundation should be real `Game`, `Room`, `Level`, and `Player` wrappers plus lifecycle callbacks, with verified state and arguments. Those features cover the most repeated needs in this corpus and the original Specialist mod. Pickup enumeration, costumes, music, save data, and JSON follow as separate verified capabilities.

Static NRO symbol names are leads only. A native symbol is not a callable Lua binding until the object acquisition path, lifetime, ABI, arguments, and behavior are established for the exact Repentance build ID. In particular, the catalog's `ItemConfig::GetCollectible(int)` symbol is not called: its owning instance and safe injected-call contract are not verified.
