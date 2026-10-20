# SaltyNX Lua runtime probe

This is a non-hooking diagnostic plugin. It is intended to prove that a SaltyNX ELF can read a Lua 5.1 script from the SD card and execute it inside the target process. The sample script only writes a marker into `probe.log`. It does not call Isaac APIs, inspect game memory, or modify game behavior.

The GitHub Actions workflow builds the ELF using the published NX-FPS SaltyNX plugin scaffold and Lua 5.1 Switch sources, pinned to known commits. It produces an artifact containing `IsaacLuaBridge.elf` and `probe.lua`.

For a device check, the ELF and Lua file go in `SaltySD/plugins/<running-title-id>/`, beside each other. Use the title ID shown by SaltyNX for the running game. The plugin writes `probe.log` into that folder. This probe is not a game mod loader and does not make Isaac's Lua callbacks available.

The build uses the MIT-licensed [NX-FPS SaltyNX plugin scaffold](https://github.com/masagrator/NX-FPS) and [Lua 5.1 for Switch](https://github.com/carstene1ns/lua51-switch). Their upstream commits are pinned in the workflow.
