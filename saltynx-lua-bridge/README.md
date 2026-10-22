# IsaacLuaBridge 0.4.0 — continuous callbacks

The 0.3.8 in-game Lua milestone is preserved at
`stable/isaacluabridge-0.3.8`, commit
`103e5e4563f475bad318241b3bc5fb5c1544f758`.
The user reported stage 8, EXPECTED_HOOK and Lua callback SUCCESS on Switch.
Version 0.4.0 is the next device candidate; its device stability is not yet established.

## What changes

- Removes the three-callback limit. Calls `on_update(frame)` after every
  intercepted original IsaacUpdate, including menu updates.
- Keeps the existing hook, offsets, build checks, worker and original function call unchanged.
- Stops further Lua callbacks after an error; stage 9 can now supersede stage 8.
  Isaac's original update continues.
- Uses a 100,000 Lua-instruction count hook per script invocation. This catches
  ordinary Lua infinite loops. It is not a sandbox or a wall-clock timeout:
  native C functions and scripts that change debug hooks are not bounded.
- Publishes completed/attempted/failed counters through the existing startup marker.
  No SD file IPC or network calls are added to the game update callback.

## Minimal Lua API

`IsaacBridge.GetFrameCount()` returns our intercepted update count.
It is not Isaac's gameplay frame counter.

`IsaacBridge.GetRuntimeState()` returns a new table with `version`,
`target_build_id`, `frame`, `completed_callbacks`, `failed_callbacks`,
`target_verified` and `callback_limit=false`. The completed count excludes
the currently executing callback. These are bridge runtime values, not gameplay
state. This build does not implement `Game()`, `RegisterMod`, pedestal/player
enumeration, item quality, costumes or music. Do not use manager existence or
callback counts to infer that a run is active.

The bundled bridge.lua exercises continuous callbacks and checks this API.
It does not pretend to run Specialist or simulate game entities.

## Install and one device check

Close Isaac first. Back up the working 0.3.8 ELF and bridge.lua outside the
SaltySD plugins directory, then copy the matching 0.4.0 ELF and bridge.lua from
the artifact to `sd:/SaltySD/plugins/010021C000B6A000/`.
Update the existing Snapshot NRO with the artifact from the same commit.

Launch Isaac, enter a run, move between rooms and leave it running about a minute.
Use only the homebrew access path that already worked with Isaac remaining alive.
Run Snapshot; it should report `bridge_got_hook=yes`, `bridge_stage=8`,
`bridge_continuous=OBSERVED`, a succeeded count greater than three, and zero failed callbacks.
These observations confirm counted callbacks, not timing performance or Specialist.
No new diagnostic app, overlay or Windows networking tool is required.

If stage 9 appears, restore the working script before another run.
To roll back the whole candidate, restore the backed-up 0.3.8 ELF and script,
then restart Isaac. The stable 0.3.8 artifact is also available from
[its successful build](https://github.com/angelcraft/TheModdingOfIsaac/actions/runs/39864839133).

## Specialist next

The existing Workshop source triggers on valid collectible pedestals before pickup.
Next native API work must verify the active run, player/entity list,
collectible subtype/quality and costume calls against the exact NRO.
No guessed structure layouts or new native offsets are introduced by 0.4.0.
Target NRO Build ID: `91C73FDD575061318D68886316AFEAC72388B2AB`.
