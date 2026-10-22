# IsaacLuaBridge 0.3.8 — bounded eager IsaacUpdate GOT hook

**Experimental and version-specific. Not Specialist yet.** The purpose of this
single test is to make Isaac's own update loop call `on_update(frame)` in the
persistent Lua 5.1 state. Gameplay APIs for items/costumes/music are not exposed.

## Change from 0.3.7

The old worker treated the first unexpected GOT value as a permanent mismatch.
This could happen even when Repentance was already loaded because some dynamic
imports still pointed to the common lazy-resolution stub.

The 0.3.8 worker now rechecks the GOT every 25 ms for up to 20 seconds, and
performs one atomic compare-and-swap **only when the target is one of these**:

- The exact previously verified original `IsaacUpdate` implementation, or
- The shared lazy-resolution entry observed in device logs, at NRO base
  `+0x66FAA0`, and **only if that address is executable**.

Any unknown destination remains untouched. A concurrent loader relocation
that wins the race makes the compare-and-swap fail; the worker retries with
the new value. The actual NRO build ID, `IsaacUpdate` bytes, call boundary
and GOT page permissions must match before installation.

There is no byte patch or inline trampoline. Network telemetry has been
**disabled** in this build, including the extra networking thread, so
**Windows will not receive packets**. Keep the Windows LiveBridge window closed.

## One Switch test

Use the `IsaacLuaBridge.elf` built from this branch; do not reuse a prior
artifact. Backup the old ELF, then place the new one at:

`sd:/SaltySD/plugins/010021C000B6A000/IsaacLuaBridge.elf`

Leave the existing `bridge.lua` in the same directory.

1. Fully close Isaac. Relaunch it with Repentance enabled and enter a room.
2. Leave it running for several seconds.
3. Run **IsaacModuleSnapshot 0.3.1** through the same homebrew path that
   previously worked for you, without closing the Isaac process.
4. Send only `sd:/switch/TheModdingOfIsaac/module-snapshot.log` if necessary.

Success: `bridge_stage=8`, `bridge_got_hook=yes`, and
`bridge_callback=SUCCESS`. A stage of 4 with no 8 means the import hook
was installed but the Lua callback was not observed.

Stage 6 means no accepted binding emerged during the bounded wait.
Stage 5 means the GOT was not writable. Stage 7 means the target NRO was
not found. Neither is permission to overwrite random addresses.

If the game crashes/freezes, remove this experimental ELF, restore the
previous backup, restart the console/game normally and keep its crash
report. Do not rerun a crashing ELF unchanged.

The exact target NRO Build ID is
`91C73FDD575061318D68886316AFEAC72388B2AB`.
A successful GitHub Actions build is not evidence of device stability.
