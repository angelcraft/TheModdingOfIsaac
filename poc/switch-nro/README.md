# Specialist mock checks on Switch — POC 0.2

This standalone homebrew NRO runs the same source-derived Specialist mock checks as the host Lua 5.1 test suite. It is separate from Isaac: it does not launch or patch the game, read game memory, or provide Isaac APIs.

## SD card layout

Extract the ZIP at the SD card root and keep this layout:

```text
sdmc:/switch/TheModdingOfIsaacLuaPOC.nro
sdmc:/switch/TheModdingOfIsaac/test.lua
sdmc:/switch/TheModdingOfIsaac/source/loader_core.lua
sdmc:/switch/TheModdingOfIsaac/mods/specialist_probe.lua
```

Launch `TheModdingOfIsaacLuaPOC.nro` from Homebrew Menu. A successful run prints:

```text
PASS: 9 Specialist mock scenarios plus callback dispatch
Lua runtime: Lua 5.1
LIMIT: simulation only; no Isaac API or game integration.
```

Press PLUS to close. The checks exercise modeled trigger rules using synthetic entities. A pass confirms this script ran on the Switch Lua runtime; it does not validate entity mapping or behavior inside Isaac.

## Build

CI builds Lua 5.1.5 from `carstene1ns/lua51-switch` at pinned commit `b1406bb55375787b3559bd1981020541c95a2bee`, then links the NRO using devkitPro/libnx.

## Safety boundary

- No Isaac process or game memory is accessed.
- No binary patch is produced.
- The Specialist module consumes mock entity records only; a verified native adapter is still missing.
- The Lua standard libraries are open for this POC. Do not treat it as a sandbox for arbitrary third-party mods.
- Never put game dumps, executable assets, or proprietary resources in this repository.
