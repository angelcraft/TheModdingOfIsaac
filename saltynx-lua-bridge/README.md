# IsaacLuaBridge 0.3.6 — guarded first frame callback

Experimental first in-game Lua callback for the exact verified Nintendo Switch Repentance NRO.
**Not a playable Specialist mod.** No player/entity/music API is exposed.

## What it actually does

1. At plugin startup, verifies the Isaac title, prepares persistent Lua 5.1, checks
   the main module mapping, and starts a late-NRO worker.
2. The worker waits for Repentance, validates the NRO Build ID, the complete
   `IsaacUpdate` 36-byte function and its call boundary, and checks the original
   GOT target and writable mapping.
3. If all checks pass, it atomically redirects **only** the `IsaacUpdate` GOT slot.
   There is no inline patch or trampoline.
4. The wrapper calls the untouched original function before dispatching
   `on_update(frame)`, at most three times. The Lua state is guarded against
   simultaneous callback dispatch.
5. The first successful Lua callback stores stage 8 in plugin memory. Stage 9
   records a Lua callback error. No file IPC is used inside the worker or hook.

The existing `IsaacModuleSnapshot.nro` (v0.3.0 from **this same branch**) can
read that stage and the actual GOT binding without modifying game memory.

## Optional LiveBridge local streaming (experimental)

This build also attempts to send small **UDP** status events on the local Wi-Fi
to **Windows 10.10.0.165:47821** from the bridge's background worker; the Switch
is expected to have IP **10.10.0.162**. It never sends from the frame callback.
Socket initialization can fail in this injected environment; that must not
prevent gameplay, and a successful build alone does not prove network delivery.

The same GitHub Actions artifact contains `windows/IsaacLiveBridge.ps1`.
Unzip it into a folder and run from that folder in **Windows PowerShell**:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\IsaacLiveBridge.ps1
```

If the Windows firewall blocks LAN UDP, open an administrator PowerShell and
allow the **Switch only**, on the **Private** profile:

```powershell
New-NetFirewallRule -DisplayName "Isaac LiveBridge" -Direction Inbound -Protocol UDP -Action Allow -LocalAddress 10.10.0.165 -LocalPort 47821 -RemoteAddress 10.10.0.162 -Profile Private
```

Remove the rule when finished with `Remove-NetFirewallRule -DisplayName
"Isaac LiveBridge"`. This receiver only reads local UDP messages and writes
`livebridge-events.log`; it does **not** grant ChatGPT access to the PC and
does **not** upload logs to GitHub.

## Single device test (make a backup first)

Use **both** newly built artifacts from this branch. Do not mix the new
`IsaacLuaBridge.elf` with the older `IsaacModuleSnapshot.nro`.

- Copy `IsaacLuaBridge.elf` and `bridge.lua` to
  `sd:/SaltySD/plugins/010021C000B6A000/` after backing up or renaming the old ELF.
- Copy `IsaacModuleSnapshot.nro` to
  `sd:/switch/TheModdingOfIsaac/`, backing up the old NRO.
- Fully close and relaunch Isaac, wait until you can move in a room, and let
  several frames run.
- Use the **previously working homebrew access method** while Isaac remains
  running; launch `IsaacModuleSnapshot.nro` once. Do not use a Homebrew Menu
  route that previously froze the console.
- Read the result on screen and, if needed, send the one
  `sd:/switch/TheModdingOfIsaac/module-snapshot.log` file.

Look for `bridge_callback=SUCCESS` together with
`bridge_got_hook=yes` and `bridge_stage=8`. This shows `on_update(frame)`
actually completed, **not** that Specialist already works.

A normal startup log only proves Lua was initialized and the worker launched;
it does **not** prove the worker installed the GOT hook. There is deliberately
no `bridge-runtime.log`: the late worker cannot safely use the startup-only
SaltySD file interface.

## Stop/rollback

If Isaac freezes/crashes, remove the new `IsaacLuaBridge.elf` from the plugin
folder, restore the backup, and restart normally. Keep crash logs; do not
repeat the same failing test without examining them first.

This test is restricted to Repentance NRO build
`91C73FDD575061318D68886316AFEAC72388B2AB`. Compilation in GitHub
Actions is **not** proof of safe device execution.
