# Specialist API contract

**Status:** The source-derived trigger predicate is implemented in the isolated mock probe. This is not an Isaac API implementation or a Switch game mod.

## Scope and evidence

The target is [The Specialist for Good Items, Workshop ID 2575911103](https://steamcommunity.com/sharedfiles/filedetails/?id=2575911103), version 1.5.2. The user supplied ZIP SHA-256 is FE4F89C4108E9977F98AB77A0C85C223CBD198B2F9F0CF87012DD7D39CE04CA7. The archive and source inventory are documented in [SPECIALIST_REPENTANCE_SOURCE_REVIEW.md](SPECIALIST_REPENTANCE_SOURCE_REVIEW.md).

Workshop ID 3231114249 and the public [SpecialistABP adaptation](https://github.com/D00NIK/Specialistabp) are a separate Afterbirth+ target. They are not used as evidence for the Repentance source rules described here.

## Repentance source-facing API surface

| Concern | Calls used by the supplied main.lua | Status for this project |
|---|---|---|
| Mod registration and lifecycle | RegisterMod, AddCallback, MC_PRE_GAME_EXIT, MC_POST_GAME_STARTED, MC_POST_UPDATE, MC_POST_NEW_ROOM, MC_USE_ITEM, MC_USE_CARD | Static source evidence only; no Switch dispatcher exists. |
| Pickup and item data | Isaac.FindInRadius, Isaac.GetItemConfig, GetCollectible, pickup variant/subtype, item name and quality | The mock accepts normalized values; Switch values and entity layout are unverified. |
| Character and costumes | PlayerType mapping, Isaac.GetCostumeIdByPath, AddNullCostume, TryRemoveNullCostume, GetCollectibleNum | Not implemented in the mock or verified on Switch. |
| Blind and stage rules | GetCurses, GetStageType, room type and entity positions | Mock inputs are source-derived; game constants and values are not mapped. |
| Music and persistence | Isaac.GetMusicIdByName, MusicManager, room music restore, SaveData/LoadData, JSON | Not implemented in the mock. |
| Optional configuration | Mod Config Menu settings and per-item customization | Mock consumes normalized settings; the MCM itself is not implemented. |

The POC's room_update, new_room, host actions, normalized entity strings and IDs remain internal test vocabulary. They do not claim to be Isaac callback names or native values.

## Modeled source behavior

The mock probe now models the source's trigger rules:

1. Accept only collectible pickup entities with a positive subtype, stable entity identity, and a numeric item quality.
2. Block Curse of the Blind unless BlindBypass is enabled.
3. In an alt-path treasure room with collectible pedestals, apply the source position rule: for two items, x must be at most 320 and y at most 280; for more than two, x must be at most 320 and the exact point (320, 280) is excluded.
4. The source default quality threshold is four. An explicit custom item value of 1 triggers regardless of quality; value 2 vetoes; otherwise the quality threshold path runs only when IgnoreQuality is false.
5. The default custom list contains Cricket's Head, Magic Mushroom, Sacred Heart and Brimstone.
6. Track entity identity and subtype, stop when the pedestal disappears or changes, and allow a changed subtype to be considered again. Clear per-room history on new_room.

The source's IgnoreQuality label can be misread: in main.lua the quality comparison is gated by IgnoreQuality == false, so enabling that setting disables ordinary quality-based triggers while explicit value-1 entries still work. The mock preserves the code's observed behavior.

## Implemented versus unresolved

Implemented only in the deterministic host mock:

- Source-derived quality threshold and custom-list decision.
- Blind-curse and alt-path visibility conditions.
- Stable identity/subtype tracking, removal cleanup and reroll handling.
- Lua 5.1-compatible translation of this predicate.

Still unresolved:

- The Switch build's exact runtime identity and actual game loading path.
- A verified native call flow, ABI, callback lifecycle and room/entity snapshot.
- Mapping player, item, room, stage, pickup, curse and callback values for the exact build.
- Lua 5.3 compatibility for the full original source, which uses // and & operators.
- Costumes, animation resources, music registration/playback and persistence.
- MCM support, character edge cases, and end-to-end behavior in Isaac.

The existing game NRO analysis found the inspected script-runner entries at 0x421838 and 0x42183C are immediate-return stubs. The standalone Lua OK NRO is a separate homebrew program and does not load this predicate inside Isaac.

## Safety and acceptance

The mock code does not read or write game memory and is not packaged as an installable mod. A real bridge must remain disabled when the exact target build does not match. Do not create or install a native patch until the target build identity and native call flow/ABI are verified.

After those gates pass, validate read-only entity observation and callback timing first; only then add one reversible costume operation, resource loading, music and cleanup. Source parsing or host mock checks alone must never be described as Switch compatibility.
