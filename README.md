# The Modding of Isaac

Experimental research project to investigate a Lua mod loader and modding compatibility layer for the Nintendo Switch version of The Binding of Isaac.

## Status

**Research only — no working Lua loader exists yet.** Do not install experimental IPS/NRO patches from earlier prototypes; they caused crashes and were removed. Current Switch installation has been cleaned and the game launches normally again.

## Goals

1. Establish a safe, isolated Lua runtime proof of concept on Switch.
2. Identify a supported integration point with the exact Isaac executable build.
3. Design a minimal host bridge for read-only room/pedestal detection.
4. Expand carefully toward costume/animation/music APIs and a Specialist compatibility layer.
5. Keep all tests reversible, version-specific, and validated before SD installation.

## Known environment

- Console: Nintendo Switch OLED
- CFW: Atmosphère + Hekate, using EmuMMC
- Game/dump provided in prior conversation: The Binding of Isaac: Afterbirth+ update 1.7.9b
- Title IDs previously noted:
  - Base: `010021C000B6A000`
  - Update: `010021C000B6A800`
  - Repentance DLC title: `010021C000B6B001`
- Relevant user goal: load Lua scripts and eventually support the Specialist dance mod when a qualifying item is present on a pedestal, before pickup.

**Important version warning:** Almond Milk belongs to Repentance, not base Afterbirth+. Do not mix resource paths or executable offsets between AB+ and Repentance. Confirm the actual launched executable/title/update and DLC state before researching offsets.

## Prior findings (unverified until independently rechecked)

- The PC mod `D00NIK/Specialistabp` uses Isaac's Lua callbacks and APIs. The relevant source is `main.lua`; it scans room entities in `MC_POST_UPDATE`, detects collectible pickups by subtype, applies a null costume and plays music.
- Switch's vanilla port does not expose the PC modding Lua API as-is. A standalone Lua interpreter alone would not provide Isaac's object/event APIs.
- The update dump contained `Program #0/1/.nro/AfterbirthPlus.nro`, `Program #0/1/.nro/Repentance.nro`, `resources/animations.b`, and `rp_patch/resources/animations.b`.
- Prior experiments attempted loose ANM2/resources and native IPS/NRO hooks. They did not make the character dance; an experimental native hook and a passthrough diagnostic both crashed the game. All test patches/resources were removed and Isaac launches normally.
- Previous discussion suggested candidate native symbols such as pickup update, player costume, and music routines. Treat all offsets, file offset assumptions, function signatures, and hook designs as **untrusted leads** until checked against the exact NRO, build ID, architecture/disassembly, and loader implementation.
- Atmosphère supports an NRO IPS patch mechanism at `atmosphere/nro_patches`; investigate its actual conventions from Atmosphère source and the exact version before generating any patch.
- Public reference repositories mentioned:
  - https://github.com/D00NIK/Specialistabp
  - https://github.com/wofsauge/External-Item-Descriptions
  - https://github.com/carstene1ns/lua51-switch
  - https://github.com/ItsDeidara/The-Binding-of-Isaac-Mod-Conversion-Kit

## Safety rules for the project

- Never call a package "working", "validated on Switch", or "install-ready" without evidence.
- Do not write/recommend a native patch until instruction addresses, hook ABI, displaced instructions, return path, executable build ID and patcher offsets have all been independently verified.
- Start with host-side tools/tests and a non-invasive loader POC; no patch on the SD for the first steps.
- Preserve backups. Do not overwrite user mods or original game resources.
- Keep game dumps, private keys, proprietary assets and other copyrighted game files out of this public repository. This repository is private for now.
- Report unknowns plainly. First milestone is to run `test.lua` in an isolated runtime, not to claim a full Isaac Lua mod loader.
