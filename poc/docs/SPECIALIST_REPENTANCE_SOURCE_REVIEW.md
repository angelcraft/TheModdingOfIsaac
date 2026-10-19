# Repentance source review — The Specialist for Good Items

Updated: 2026-10-19

## Source identity and handling

- Workshop target: The Specialist for Good Items, ID 2575911103.
- Supplied archive metadata: name matches the Workshop target, directory specialistforgooditems, version 1.5.2.
- Archive SHA-256: FE4F89C4108E9977F98AB77A0C85C223CBD198B2F9F0CF87012DD7D39CE04CA7.
- REA performed a static ZIP inventory. The complete central-directory inventory contains 112 entries: 101 files and 11 directories, with no symlinks and no integrity contradictions.
- The archive includes main.lua (28,928 bytes; 806 numbered source lines in the extracted copy), metadata.xml, costumes2.xml, music.xml, 42 ANM2 files, 43 PNG files and six OGG files.
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

The source uses Lua 5.3 integer division (//) and bitwise AND (&). The current standalone Switch NRO is Lua 5.1, so it cannot parse this main.lua unchanged. A Lua 5.3-compatible runtime needs to be selected or the source deliberately translated with equivalent semantics. The source's IgnoreQuality setting gates the quality comparison with IgnoreQuality=false; enabling it therefore disables ordinary quality-based triggers while explicit value-1 item entries still trigger.

## Porting notes

- Scan real room entities and track stable pedestal identity rather than relying only on a count. The source's update logic stores the number of collectible pedestals; a reroll that changes an item while preserving the count may not cause a fresh trigger.
- Keep item quality, player-type, pickup-variant and curse values tied to the exact Repentance build. Do not reuse Afterbirth+ IDs.
- Treat animation and audio as separate milestones. The archive provides the assets, but the Switch game still needs a verified resource-loading path and registration layer.
- In the public SpecialistModAPI helper, addAnimation's non-override branch reads danceCostumes[playerType] instead of checking the passed table/type. playerType is not declared in that helper's scope, so this appears to be a source defect; confirm intended registration semantics before reproducing it.
- The Workshop source does not solve native integration. The inspected Repentance mod-script entries at 0x421838 and 0x42183C remain immediate-return stubs in the supplied NRO analysis.

## Additional exact-source findings from the supplied ZIP

- `SpecialistModAPI:RemoveDanceCostume()` and `RemoveItemCostume()` delegate to `removeAnimation()`, which deletes the table entry but returns no value. The nearby API comments promise a boolean result. The Switch-facing compatibility contract should choose and test an explicit return value rather than silently preserving nil.
- `DoCostume(false)` removes an item-specific costume only while the player still owns that collectible. If an item is lost while its costume is active, the current source path skips that removal; verify this cleanup behavior before porting the costume lifecycle.
- `DoCostume()` loops from player index 1 through `GetNumPlayers()`. Repentance's PC API documentation describes player IDs as zero-based and says an invalid ID falls back to player zero. The source therefore relies on that fallback to include player zero; a port should enumerate 0 through count minus one explicitly and avoid depending on fallback behavior. [Game:GetPlayer documentation](https://wofsauge.github.io/IsaacDocs/rep/Game.html) and [Isaac.GetPlayer documentation](https://wofsauge.github.io/IsaacDocs/rep/Isaac.html).
- The source's `IgnoreQuality` option is explicitly described as ignoring quality, but the ordinary quality trigger requires `IgnoreQuality == false` at `main.lua:702`. Setting it to true disables that trigger; only explicit allow-list entries still trigger. This is a confirmed source-level logic mismatch, not a Switch compatibility finding.

These are static source observations from the supplied 1.5.2 ZIP. They do not establish how the original mod behaves at runtime or which fixes are safe without an in-game test.
 
## Scope and next safe step

This is static source analysis only. It does not prove that a Switch loader exists, that a native hook is safe, or that the mod works on the console. The existing Lua OK result validates a standalone homebrew Lua smoke test only.

The source-derived trigger predicate is now modeled in the isolated host-side probe at poc/mods/specialist_probe.lua. It remains a mock with no Isaac access. The next step is mapping its normalized inputs to an exact, verified read-only game observation path. Do not create an Isaac patch until the target NRO build identity, native call flow, ABI and a reversible read-only integration point are verified.
