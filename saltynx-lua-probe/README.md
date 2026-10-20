# SaltyNX Lua runtime probe

This is a non-hooking diagnostic plugin. It reads a Lua 5.1 script from the SD card and executes it inside the target process. It also asks SaltyNX's builtin-symbol lookup for two exported Repentance functions and logs whether each symbol was found. It does not call those addresses, patch code, or modify game behavior.

The GitHub Actions workflow builds the ELF using the published NX-FPS SaltyNX plugin scaffold and Lua 5.1 Switch sources, pinned to known commits. It produces an artifact containing `IsaacLuaBridge.elf` and `probe.lua`.

For a device check, the ELF and Lua file go in `SaltySD/plugins/<running-title-id>/`, beside each other. Use the title ID shown by SaltyNX for the running game. The plugin writes `probe.log` into that folder, including the lookup results for `Manager::Update` and `ModManager::RunModScript`. A positive lookup proves only that SaltyNX can find the exported NRO symbols; it does not prove a safe hook or provide Isaac callbacks. This probe is not a game mod loader.

The build uses the MIT-licensed [NX-FPS SaltyNX plugin scaffold](https://github.com/masagrator/NX-FPS) and [Lua 5.1 for Switch](https://github.com/carstene1ns/lua51-switch). Their upstream commits are pinned in the workflow.

