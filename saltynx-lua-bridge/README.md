# Isaac Lua frame bridge 0.3.0

This is the first guarded in-game callback experiment for the exact verified
Nintendo Switch Repentance build. It registers one SaltyNX import replacement
for `IsaacUpdate`. On first invocation it derives the ASLR base from the verified
caller, checks the executable mapping, NRO Build ID, complete 36-byte original
function and call boundary, then calls the untouched original function directly.

After the original returns, it invokes `on_update(frame)` in a persistent Lua
5.1 state. This build limits Lua dispatch to three calls. It exposes no Isaac
gameplay API and is not yet the playable Specialist mod.

Install both `IsaacLuaBridge.elf` and `bridge.lua` in:

`SaltySD/plugins/010021C000B6A000/`

Remove or rename the previous `IsaacLuaBridge.elf` first. Keep the previous ELF
as a backup outside that folder. On startup, `bridge-startup.log` should say the
Lua state is ready and the late import replacement was registered. Runtime
success is also emitted through `svcOutputDebugString` as
`first in-game Lua callback completed`.

This build is specific to Repentance NRO Build ID
`91C73FDD575061318D68886316AFEAC72388B2AB`. It contains no game binary and no
inline patch or displaced instruction trampoline.

