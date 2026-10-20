# Project status and handoff

Last updated: 2026-10-20

## Current state
- Repository created by the user: https://github.com/angelcraft/TheModdingOfIsaac
- README project baseline added by assistant.
- Nintendo Switch OLED running Atmosphère/Hekate with EmuMMC.
- User says game launches normally after removing all experimental patch/resource files.
- Do not reuse or install previous native POC/diagnostic ZIPs. Both crashed on the user's console.
- No working mod loader and no verified native hook exists yet.
- The user ran the standalone Lua 5.1 Switch mock on-device; the photo showed 9 specialist-mock checks passing and explicitly said this was not an Isaac mod. This proves standalone Lua execution only.

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

The user’s DBI screenshot shows v1.7.9b and update BID `B6E5BDB9DC12E1D1`. Current local dump contains both NRO modules, with exact hashes and internal Build IDs recorded in [SWITCH_RUNTIME_FINDINGS.md](SWITCH_RUNTIME_FINDINGS.md). The relationship between the screenshot’s BID and the internal module IDs has not been established.

Important: Almond Milk is a Repentance item, not base AB+ content. Confirm exactly which executable/game/DLC the user launches before making item-ID or offset assumptions. Don't mix AB+ and Repentance resource trees or NRO offsets.

## Source mod references
- Specialist AB+: https://github.com/D00NIK/Specialistabp
- External Item Descriptions: https://github.com/wofsauge/External-Item-Descriptions
- Switch Lua 5.1 port reference: https://github.com/carstene1ns/lua51-switch
- Conversion kit reference: https://github.com/ItsDeidara/The-Binding-of-Isaac-Mod-Conversion-Kit

The Specialist Lua source uses Isaac callbacks, scans room entities in `MC_POST_UPDATE`, detects collectible pickups/pedestals by subtype, applies a null costume, and plays/restores music. It needs the game's Lua API, not just a generic Lua VM.

## Switch runtime findings (2026-10-20)
- The two exact update NROs have been hashed and their internal Build IDs recorded; see [SWITCH_RUNTIME_FINDINGS.md](SWITCH_RUNTIME_FINDINGS.md).
- In the Repentance module, `RunModScripts` returns immediately and `RunModScript` returns false. The base module's `RunModScript` also returns false. `UpdateWorkshopMods` returns immediately.
- The native pickup update dispatch is confirmed, but a complete per-frame path from `Game::Update` to `Room::Update` and a safe hook/API contract are not.
- Neither NRO contains the checked Lua or Isaac callback strings; this is limited negative string evidence, not proof of absence of stripped code.
- No installable playable package was produced. Do not flip the script result to true and call that a Lua bridge.

## Prior failures to learn from
- Resource-only ZIPs that associated Specialist's animation with Crown of Light/Almond Milk did not cause the dance. They did not implement pedestal detection or original Lua callbacks.
- One experimental native hook and one attempted passthrough diagnostic both crashed the user's game. User then removed all test files and confirmed Isaac launches normally.
- Do not reuse previous IPS bytes, offsets, trampolines or claimed symbol addresses without re-disassembling the exact target NRO and independently validating file offsets vs mapped offsets, IPS patch offsets, hook ABI, displaced instructions, branch ranges, register/stack state, and return path. Previous claims about offsets were unreliable.
- Don't tell the user a patch has been "validated" from checking only ZIP structure/XML when it hasn't been executed on device.

## Recommended roadmap
1. Establish project build/test scaffolding on PC and define exact target NRO version/build ID.
2. Obtain/inspect Lua 5.1 Switch port and identify build requirements, without hooking Isaac yet.
3. Keep the standalone Lua test as a separate milestone; it already passed on the user's Switch and does not provide Isaac APIs.
4. Separately research supported homebrew/overlay/sysmodule approaches and determine whether they can access this game process. The native script-loading stubs and unresolved resource redirection are recorded in the findings.
5. Find a minimal non-invasive game-host probe; don't mutate game state initially.
6. Only after validated host integration, define a tiny API: room entities, pickup variant/subtype, callbacks, read-only diagnostics.
7. Add tested animation/costume/music operations and then Specialist compatibility.

## Rules
- Keep game dumps, NROs, keys, and proprietary game assets out of GitHub; keep the repo private while scope is unsettled.
- All experiments must be reversible; no installable patch until host-side tests and byte-level verification succeed.
- Never conflate "Lua interprets script", "script is injected into Isaac", and "the Isaac API exists".
