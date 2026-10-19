# Standalone Switch Lua NRO — Milestone 1B

This app is completely separate from Isaac. It starts Lua 5.1, reads one Lua file from the SD card, executes it, reports success/errors on the homebrew console and waits for PLUS to exit.

## SD layout

Copy the built NRO and the script so the SD card contains:

```text
sdmc:/switch/TheModdingOfIsaacLuaPOC.nro
sdmc:/switch/TheModdingOfIsaac/test.lua
```

The supplied test script prints `Lua OK`. It is only a smoke-test script and exposes no Isaac API.

## Build

The workflow builds Lua 5.1.5 from the `carstene1ns/lua51-switch` repository at pinned commit `b1406bb55375787b3559bd1981020541c95a2bee`, then links this NRO against `liblua.a` using devkitPro/libnx.

Local build prerequisites: devkitPro with devkitA64 and libnx, make, git, and a C toolchain. From the repo root:

```sh
git clone https://github.com/carstene1ns/lua51-switch.git poc/switch-nro/third_party/lua51-switch
git -C poc/switch-nro/third_party/lua51-switch checkout b1406bb55375787b3559bd1981020541c95a2bee
make -C poc/switch-nro/third_party/lua51-switch
make -C poc/switch-nro
```

## Limitations and security

- This proves only standalone Lua execution on Switch when built and run successfully.
- It does not inject into Isaac, inspect game memory, or implement Isaac callbacks.
- This POC opens the standard Lua libraries for smoke testing. Do not treat it as a secure sandbox for arbitrary third-party mods.
- Do not put game dumps, executable assets, or proprietary resources in this directory.
