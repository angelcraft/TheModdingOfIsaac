# Repentance source review — The Specialist for Good Items

Updated: 2026-10-19

## Source identity and handling

- Workshop target: The Specialist for Good Items, ID 2575911103.
- Supplied archive metadata: name matches the Workshop target, directory specialistforgooditems, version 1.5.2.
- Archive SHA-256: FE4F89C4108E9977F98AB77A0C85C223CBD198B2F9F0CF87012DD7D39CE04CA7.
- REA performed a static ZIP inventory. The complete central-directory inventory contains 112 entries: 101 files and 11 directories, with no symlinks and no integrity contradictions.
- The archive includes main.lua (28,928 bytes; 807 lines), metadata.xml, costumes2.xml, music.xml, 42 ANM2 files, 43 PNG files and six OGG files.
- Only metadata and source behavior were reviewed. The ZIP, Lua code, or assets were not executed, and the third-party assets/source bundle were not uploaded to GitHub.

## Behavior described by main.lua

The mod scans nearby room entities during MC_POST_UPDATE and reacts to collectible pedestal pickups. It checks the pedestal variant, item configuration and quality, along with a configurable item allow/deny list. Curse of the Blind and Repentance alt-path treasure-room item visibility affect whether a pedestal can trigger the dance. It applies a character-specific null costume and optional item costumes, chooses music from the player's alias or default/tainted tracks, then restores room music and removes costumes when the trigger ends.

The source registers these callbacks: MC_PRE_GAME_EXIT, MC_POST_GAME_STARTED, MC_POST_UPDATE, MC_POST_NEW_ROOM, MC_USE_ITEM and MC_USE_CARD. It persists settings through SaveData/LoadData and the JSON module. Mod Config Menu is optional; if present, the source registers settings for quality, Blind-curse handling, item overrides, music and reduced flashing.

## Required Isaac-facing API surface

A compatible host must provide more than a Lua VM. The inspected source uses:

- Mod registration and save-data methods: RegisterMod, HasData, LoadData and SaveData.
- Callback constants and callback dispatch for game start/exit, per-frame update, room changes, item use and card use.
- Entity and item access: Isaac.FindInRadius, Isaac.GetItemConfig, GetCollectible, pickup variant/subtype, entity position and item quality/name.
- Player state and costume APIs: Game, player enumeration, GetPlayerType, GetCollectibleNum, AddNullCostume and TryRemoveNullCostume.
- Room/level and visibility APIs: room type, stage type, curses, room music restoration and the Vector/EntityPartition constants.
- Music registration/lookup/playback and volume update.
- Repentance enumerations for player types, collectible IDs, pickup variants, rooms, stages and callbacks.
- JSON persistence; optional Mod Config Menu support.

The source uses Lua 5.3 integer division (//) and bitwise AND (&). The current standalone Switch NRO is Lua 5.1, so it cannot parse this main.lua unchanged. A Lua 5.3-compatible runtime needs to be selected or the source deliberately translated with equivalent semantics.

## Porting notes

- Scan real room entities and track stable pedestal identity rather than relying only on a count. The source's update logic stores the number of collectible pedestals; a reroll that changes an item while preserving the count may not cause a fresh trigger.
- Keep item quality, player-type, pickup-variant and curse values tied to the exact Repentance build. Do not reuse Afterbirth+ IDs.
- Treat animation and audio as separate milestones. The archive provides the assets, but the Switch game still needs a verified resource-loading path and registration layer.
- In the public SpecialistModAPI helper, addAnimation's non-override branch reads danceCostumes[playerType] instead of checking the passed table/type. playerType is not declared in that helper's scope, so this appears to be a source defect; confirm intended registration semantics before reproducing it.
- The Workshop source does not solve native integration. The inspected Repentance mod-script entries at 0x421838 and 0x42183C remain immediate-return stubs in the supplied NRO analysis.

## Scope and next safe step

This is static source analysis only. It does not prove that a Switch loader exists, that a native hook is safe, or that the mod works on the console. The existing Lua OK result validates a standalone homebrew Lua smoke test only.

The next implementation step is to port the trigger predicate into the isolated host-side prototype, retaining the same quality/list/visibility rules while making pedestal identity explicit. Do not create an Isaac patch until the target NRO build identity, native call flow, ABI and a reversible read-only integration point are verified.
