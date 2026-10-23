# IsaacLuaBridge 0.7.1 — Specialist correction candidate

The user confirmed the 0.6.0 dance, then reported that 0.7.0 displayed a small
headless body and played no Specialist music. This candidate corrects two
specific faults found in the exact-build code:

- ANM2::Load resets the animation states. After loading the actor, explicitly
  Play("HeadDown", true) and verify IsPlaying("HeadDown"). On failure, remove the
  bridge-owned costume so its hidden-head effect is not left on the player.
  Both native calls have full-function drift guards.
- Music::LoadConfig accepts explicit IDs 1–118, rejecting the old ID 200.
  Register ID 19 only if its live name, intro, main path and layers are empty.
  If occupied, preserve it and report an error. LoadConfig(preserve=true)
  preserves the remaining game music entries.

The Lua rule selects quality-4 uncollected room pedestals using this build's
40 metadata IDs, plus the intentional item 502 test exception. There are 41
character asset mappings, with a one-player-entity limit; paired characters
and co-op remain unfinished.

The 0.7.1 code and assets still need Switch confirmation. Actions builds the
ELF and Lua. Keep the read-only Snapshot 0.7.0; witness layouts are unchanged.
Run `saltynx-lua-bridge/tools/build_specialist_070_assets.py`
from the project root to build the LayeredFS files from the user's existing
game dump and Workshop assets; those original assets are not stored in GitHub.

The package restores stock Gnawed Leaf artwork and adds the Specialist cache
actors/sheets plus one soundtrack. Validate first with normal Isaac, one player,
the known 502 pedestal, a quality-4 pedestal, and a non-Isaac character. Confirm
the dance starts on room entry, stops after leaving or pickup, music follows
and restores, and quality 0–3 items other than 502 do not trigger it. The 0.6.0 build and
pre-change SD backup remain available for rollback.

See `DANCE_CONTRACT.md` for the exact-build guards, asset conversion, test
limits, and rollback details.
