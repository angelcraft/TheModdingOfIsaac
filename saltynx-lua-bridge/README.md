# IsaacLuaBridge 0.7.0 — Specialist candidate

Builds on the 0.6.0 device-confirmed dance. The Lua rule now selects room
pedestals only from the quality-4 IDs in the exact Switch `items_metadata.xml`.
It requests per-character Specialist animation assets for PlayerTypes 0–40 and
requests soundtrack ID 200 while a qualifying pedestal is present. The music
track is guarded by the exact-build Music API and restores room music through
the game's `Room::PlayMusic` routine.

The 0.7.0 source and locally prepared LayeredFS package are candidates until
tested on the Switch. The Actions artifact contains the ELF, Lua, and read-only
Snapshot NRO. Run `saltynx-lua-bridge/tools/build_specialist_070_assets.py`
from the project root to build the LayeredFS files from the user's existing
game dump and Workshop assets; those original assets are not stored in GitHub.

The package restores stock Gnawed Leaf artwork and adds the Specialist cache
actors/sheets plus one soundtrack. Validate first with normal Isaac, one player,
an uncollected quality-4 pedestal, and at least one non-Isaac character. Confirm
the dance starts on room entry, stops after leaving or pickup, music follows
and restores, and quality 0–3 items do not trigger it. The 0.6.0 build and
pre-change SD backup remain available for rollback.

See `DANCE_CONTRACT.md` for the exact-build guards, asset conversion, test
limits, and rollback details.
