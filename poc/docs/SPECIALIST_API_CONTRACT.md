# Specialist API contract

**Status:** source-derived design contract for the isolated Lua POC. This document does not implement Isaac APIs or establish compatibility with a game build.

## Scope and evidence

The selected target is [The Specialist for Good Items, Steam Workshop ID 2575911103](https://steamcommunity.com/sharedfiles/filedetails/?id=2575911103), whose official listing identifies Repentance as its required DLC. The listing/changelog describe tainted-character support and configurable dance/music behavior, but the original target's Lua source and resource bundle have not yet been acquired or inspected.

The available [SpecialistABP source](https://github.com/D00NIK/Specialistabp/blob/main/main.lua) is a separate Afterbirth+ adaptation (Workshop ID 3231114249); its author warns against using it with Repentance. The source-facing API inventory and source-review findings below apply only to that AB+ adaptation. They are not verified implementation details of Workshop ID 2575911103. The POC’s [mock-host probe](../mods/specialist_probe.lua) is also separate: it is a deterministic logic simulation, not the Isaac Lua API or an in-game port.

## AB+ source-facing surface

| Concern | Calls and behavior used by the reference mod | Evidence status |
|---|---|---|
| Mod registration and lifecycle | `RegisterMod`, `mod:AddCallback`, `MC_POST_UPDATE`, `MC_POST_NEW_ROOM` | Present in source; target callback semantics still need validation. |
| Game and room access | `Game()`, current room, room entity enumeration | Present in source; Switch object layout and entity values are unverified. |
| Pickup detection | Pickup entity type and collectible variant, nonzero `SubType`, and an item allowlist | The source predicate is visible; exact target constants and timing are unknown. |
| Character costume | Character/player type mapping, `Isaac.GetCostumeIdByPath`, `Player:AddNullCostume`, `Player:TryRemoveNullCostume` | Calls are visible; availability, arguments, return behavior, and resource resolution must be checked on the selected build. |
| Music | `Isaac.GetMusicIdByName`, `MusicManager()`, and room music restoration | Calls are visible; ordering, ownership, and restoration behavior are unverified. |
| Content files | Costume/animation and music resources referenced by the mod | Requires a separate resource-path and packaging check. |

This table is a research checklist, not a promise that these globals, methods, constants, or callbacks exist in Repentance on Switch.

## AB+ source review findings

The public source is explicitly Afterbirth+ only; its README says not to use it with Repentance. These are source-derived findings, not Switch runtime observations:

- `ClearItemList` fills numeric slots, but the pickup path calls `table.insert(mod.items, v.SubType, true)`. That inserts and shifts later entries; the Repentance adaptation should use a keyed set such as `seen[subtype] = true`.
- The active-dance scan matches pickup type and subtype but omits pickup variant and instance identity. The adapter should retain a stable identity for the exact pedestal that triggered the animation.
- Both room scans stop at `#itemsNearby - 1` based on an upstream crash comment. Confirm the target container's indexing and bounds before choosing a loop limit.
- The allowlist comes from the AB+ source and must be rebuilt or verified against the selected Repentance target.

## Behavioral contract for the POC

The isolated probe uses only the mock host API documented in its source. Its current contract is:

1. On a room-update event, inspect the provided entity snapshot.
2. Select at most one previously unseen entity whose mock type is `pickup`, mock variant is `collectible`, subtype is nonzero, and subtype is in the supplied qualifying-item set.
3. Emit `specialist_start` for that entity with timing marked `before_pickup`.
4. If the active entity disappears from a later snapshot, emit `specialist_stop` with reason `pedestal_removed_or_picked_up`.
5. On a new-room event, stop any active entity and clear the per-room seen set.

The probe’s `room_update`, `new_room`, `entity_id`, `host:on`, and `host:emit_action` names are POC abstractions. An adapter must map them to verified game callbacks and stable entity identity; no mapping to Isaac has been implemented or proven.

## Intended adapter boundary

Keep the eventual integration split into three layers:

- **Lua runtime:** loads a controlled mod folder and dispatches callbacks.
- **Game adapter:** exposes only the verified room/entity snapshot and narrowly scoped actions needed by a compatibility target.
- **Mod compatibility layer:** maps source-facing mod APIs onto that adapter and reports unavailable APIs instead of silently fabricating behavior.

For the first Specialist slice, expose room and qualifying-pickup observations only. Add one costume action only after observation and callback timing are validated. Add resource loading, music changes, and cleanup last. The POC’s action names are internal test vocabulary and are not the names of an Isaac API.

## Unresolved before claiming compatibility

- Original target bundle for Workshop ID 2575911103, including its exact Lua source, metadata and resources.
- Exact game title, NRO build ID, and file identity for the build being tested.
- Lua language/runtime baseline and actual loader entry point for that build.
- Callback registration, argument shape, ordering, and room-transition behavior.
- Entity type/variant/subtype values and a stable way to track pickup instances.
- Costume API and resource-path behavior, including player/character edge cases.
- Music lookup, playback, interruption, and restoration behavior.
- Mod metadata and content packaging rules on the selected game.
- Repeat-run, continue, room-transition, and failure-cleanup behavior.

The source contains an entity-loop boundary quirk that skips the final element in one scan. Treat that as an upstream implementation detail to investigate; do not encode it as intended compatibility behavior without runtime evidence.

## Acceptance sequence

1. Record and verify the exact target build identity before any build-specific work.
2. Validate a read-only room/entity observation path and callback timing.
3. Confirm the qualifying-pickup predicate against observed values.
4. Verify the lifecycle and deduplication behavior with deterministic host-side cases.
5. Validate one reversible costume action and cleanup.
6. Validate resource loading and music only after the earlier steps pass.
7. Report unsupported API calls and dependencies explicitly; do not mark the whole mod compatible from script parsing alone.

No binary patch, NRO, or installation package is produced by this contract. Do not create or install a native patch until both target build identity and native call flow/ABI have been verified, as required by the project’s safety gate.
