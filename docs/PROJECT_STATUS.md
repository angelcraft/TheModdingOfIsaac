# Project status and handoff

Last updated: 2026-10-19

## Current state
- Repository created by the user: https://github.com/angelcraft/TheModdingOfIsaac
- README project baseline added by assistant.
- Nintendo Switch OLED running Atmosphère/Hekate with EmuMMC.
- User says game launches normally after removing all experimental patch/resource files.
- Do not reuse or install previous native POC/diagnostic ZIPs. Both crashed on the user's console.
- No working mod loader and no verified native hook exists yet.

## The actual goal
Build a reusable Lua mod loader/compatibility layer for The Binding of Isaac on Nintendo Switch, ideally eventually run PC Lua mods such as Specialist. Focus first on a standalone Lua 5.1 runtime test, then study how to safely integrate with Isaac. Lua execution and Isaac's modding API are separate problems; don't imply the standalone runtime provides the API.

## Known supplied material
The user uploaded an update dump archive in the previous conversation:
`The Binding of Isaac_ Afterbirth+ 1.7.9b [010021C000B6A800][v524288][UPD].zip` (~511 MB compressed).
Earlier inspection reported:
- `Program #0/1/.nro/AfterbirthPlus.nro`
- `Program #0/1/.nro/Repentance.nro`
- `Program #0/1/resources/animations.b`
- `Program #0/1/rp_patch/resources/animations.b`
- `resources/costumes2.xml`, `resources/music.xml`
- `animations.b` ~7,066,833 bytes; matching SHA was reported for root and rp_patch copies.

These are notes from prior work, not independently revalidated in this repository. In a new chat, don't assume the 511 MB dump is mounted: ask the user to attach it again only if byte-level work requires it. Prefer working from small outputs (NRO hashes/build IDs, section map, disassembly slices, symbols, extracted XML/animation samples) to avoid repeatedly sending the whole dump.

## Title IDs noted earlier
- Base: `010021C000B6A000`
- Update: `010021C000B6A800`
- Repentance DLC title: `010021C000B6B001`

Important: Almond Milk is a Repentance item, not base AB+ content. Confirm exactly which executable/game/DLC the user launches before making item-ID or offset assumptions. Don't mix AB+ and Repentance resource trees or NRO offsets.

## Source mod references
- Specialist AB+: https://github.com/D00NIK/Specialistabp
- External Item Descriptions: https://github.com/wofsauge/External-Item-Descriptions
- Switch Lua 5.1 port reference: https://github.com/carstene1ns/lua51-switch
- Conversion kit reference: https://github.com/ItsDeidara/The-Binding-of-Isaac-Mod-Conversion-Kit

The Specialist Lua source uses Isaac callbacks, scans room entities in `MC_POST_UPDATE`, detects collectible pickups/pedestals by subtype, applies a null costume, and plays/restores music. It needs the game's Lua API, not just a generic Lua VM.

## Prior failures to learn from
- Resource-only ZIPs that associated Specialist's animation with Crown of Light/Almond Milk did not cause the dance. They did not implement pedestal detection or original Lua callbacks.
- One experimental native hook and one attempted passthrough diagnostic both crashed the user's game. User then removed all test files and confirmed Isaac launches normally.
- Do not reuse previous IPS bytes, offsets, trampolines or claimed symbol addresses without re-disassembling the exact target NRO and independently validating file offsets vs mapped offsets, IPS patch offsets, hook ABI, displaced instructions, branch ranges, register/stack state, and return path. Previous claims about offsets were unreliable.
- Don't tell the user a patch has been "validated" from checking only ZIP structure/XML when it hasn't been executed on device.

## Recommended roadmap
1. Establish project build/test scaffolding on PC and define exact target NRO version/build ID.
2. Obtain/inspect Lua 5.1 Switch port and identify build requirements, without hooking Isaac yet.
3. Build a standalone NRO/homebrew proof of concept that reads `test.lua` from SD and logs output to a file/screen. This proves Lua VM works in the Switch environment only.
4. Separately research supported techniques to run code inside the game process. Assess system-module/overlay/accessibility/API limitations before writing patches.
5. Find a minimal non-invasive game-host probe; don't mutate game state initially.
6. Only after validated host integration, define a tiny API: room entities, pickup variant/subtype, callbacks, read-only diagnostics.
7. Add tested animation/costume/music operations and then Specialist compatibility.

## Rules
- Keep game dumps, NROs, keys, and proprietary game assets out of GitHub; keep the repo private while scope is unsettled.
- All experiments must be reversible; no installable patch until host-side tests and byte-level verification succeed.
- Never conflate "Lua interprets script", "script is injected into Isaac", and "the Isaac API exists".
