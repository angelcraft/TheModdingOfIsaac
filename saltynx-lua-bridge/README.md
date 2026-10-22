# IsaacLuaBridge 0.5.0 — native room collectible reader

0.4.0 produced 1561 successful callbacks and zero Lua errors in the supplied device log.
It remains protected at `stable/isaacluabridge-0.4.0`; 0.3.8 is also preserved.

This candidate adds `IsaacBridge.GetRoomCollectibles()`, a bounded, read-only
native API returning `in_run`, `manager_state`, `entity_count`, `player_count`
and `items` (positive collectible subtype IDs). Invalid mappings/oversize lists
return `nil, reason`. No new gameplay native calls or game memory writes are used.
The original update hook, worker and continuous callback/error handling remain unchanged.

New field reads are derived from the exact-build assembly and guarded by the
existing build validation plus full-function drift fingerprints. See
[PEDESTAL_READ_CONTRACT.md](PEDESTAL_READ_CONTRACT.md). These fingerprints are not
cryptographic authentication. The read-only enum mapping and live field path still
require the first device check. Candidate IDs do not establish quality, curse
visibility or Specialist eligibility.

The bundled Lua polls every 15 callbacks. It records candidates present in the room,
including before collection. It does not yet change animation, costume or music.

## Install and one functional check

Close Isaac. Back up the working 0.4.0 ELF and bridge.lua outside SaltySD/plugins.
Install the matching 0.5.0 ELF/script and updated existing Snapshot from this commit.
Enter a treasure room with a pedestal. Stay near it for several seconds **before
picking it up**, then use the existing reader via the homebrew route that already works.
Expected: stage 8, zero failed callbacks, `bridge_pedestal_contract=1`,
`bridge_pedestal_candidate=OBSERVED`, nonzero player count and a nonzero last_seen_id.
The ID must agree with the actual pedestal. Preserve the log if it differs.
No additional tools, overlay or network setup is required.

Rollback: restore the saved 0.4.0 ELF and script and restart Isaac.
0.5.0 compilation success does not establish its device behavior.
The next change needs verified item quality/visibility and costume resource IDs.
