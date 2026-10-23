# IsaacLuaBridge 0.8.0 — experimental Workshop Lua loader

This isolated feature branch adds optional SD-based PC-style Lua mod loading:
`RegisterMod` + `MC_POST_UPDATE` + `MC_POST_NEW_ROOM` only. Room transitions are detected from the already verified current-room pointer; no new native call or memory write is used. The current, previously verified
specialized Specialist bridge.lua is preserved but its per-frame callback is paused
when at least one Workshop callback is enabled, preventing two dance/music
implementations from running together.

When `sd:/switch/TheModdingOfIsaac/mods.txt` is missing, **no Workshop scripts
are loaded**. For an opt-in smoke test install the bundled demo from
`examples/switch/TheModdingOfIsaac/`.

The read-only Snapshot can report loaded mod count, registered callbacks,
callback successes and errors. A separate offline catalog enumerates PLT
imports, dynamic symbols, BL and BLR instruction sites from the verified
Repentance.nro for further API reverse-engineering; it does **not** capture
every live/native function call.

**Not yet tested on hardware; original Specialist Workshop Lua does not work
unmodified.** See `docs/WORKSHOP_COMPAT_V1.md` for full details.

---

# IsaacLuaBridge 0.7.1 — Specialist correction candidate

Resource correction after the second failed Switch test: cache actor base paths
must end in `/.`, matching the original cache. GetAbsolutePathFromBasePath splits
the base with getline and removes its final component. A trailing slash has no
final empty token, so the old base lost specialist_switch and looked for sheets
in the parent directory. The resource builder now preserves the folder with the
dot sentinel. This changes only animations.b; native ELF and PCX sheets are
unchanged. The separate resource-fix package still needs Switch confirmation.

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
