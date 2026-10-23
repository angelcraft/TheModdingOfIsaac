## 0.7.1 correction after the failed 0.7.0 device test

The user observed a headless body and no music. Exact-build assembly shows
Load resets both animation states. The new reload sequence explicitly calls
ANM2::Play(const char*,bool) at 0xA1F4 and verifies IsPlaying at 0xA454. These
functions have full-symbol-size FNV guards. If playback is not established,
the bridge removes its costume and reports error 15 instead of leaving a
hidden head. This is not yet device-confirmed.

Music::LoadConfig's explicit-ID branch at 0x4260D4..0x4260E4 accepts IDs 1..118;
ID 200 was discarded. Use slot 19 only if its live name, intro, main path and
layer vector are empty. Otherwise report error 6 without overriding any track.
The static XML omits 19, but only the live occupancy check permits registration.
The existing music table is retained. Playback/restoration still need testing.

# Specialist 0.7.0 candidate and 0.6.0 device baseline

Stable 0.5.0 remains at `stable/isaacluabridge-0.5.0` / e9b20cc807047c4307d2bcf4cdcbb59310516508.
Its device reader observed item 502, 7320 successful Lua callbacks, zero callback
errors, 488 scans and zero read errors. The user then confirmed visible
Specialist dancing with 0.6.0 on device. That confirmation is the stable
starting point for this candidate.

## Lua rule

Every third update, read the current room. 0.7.0 applies only while at least
one uncollected type-5, variant-100 pedestal whose ID is quality 4 in the
extracted Switch metadata is present. The embedded ID list was derived from
`items_metadata.xml` SHA256
`42b3eb47a2f39d7120708ad32a790fc0c1129dd845f3cbdc969d702a5f17152e`.
Unknown IDs fail closed. Use one normal Isaac; simultaneous/co-op player
costumes are still rejected.

## Verified native call flow

Input NRO SHA256 CC363208C220F65547EDDA6D8268B3B6C0F7373B14C91AE6FFB378018DE4B66A.
Build ID 91C73FDD575061318D68886316AFEAC72388B2AB.

* AddNullCostume @291BBC, size44 hex: `void(Entity_Player*, int)`;
  manager GOT AAC648, manager+36538 ItemConfig; GetNullItem @3C0CC4 through
  PLT6730A0; returned Item* in x1, original player x0, boolean w2=0; tail calls
  AddCostume @290C38 through PLT6716F0.
* TryRemoveCostume(null enum) @2F2170, size40 hex: same GetNullItem route,
  returned Item* in x1 and player x0, tail calls RemoveCostume @2914E0 through
  PLT678230. It does not take the collectible overload's third argument.
* Constructor @277B74 installs *(base+AACF38)+10 as vptr. The corresponding
  dynamic relocation names Entity_Player's vtable at A37100. Runtime checks
  both the relocated GOT and each current player's vptr A37110 and type/variant.
* RemoveCostume enumerates player+1588..1590 in 178-byte entries, comparing
  Item* at +158. The same bounded read confirms additions/removals.

All numbers above are hex. Full Add/Remove implementations, enum wrappers,
GetNullItem, constructor prefix and the three PLT stubs have exact-build drift
guards. The generator rejects relocations inside code guards and verifies the
PLT symbol implementations and vtable symbol. Runtime also rejects conflicting
GOT call-throughs, allowing only the intended implementation or known lazy
resolver. This adds no executable patch and does not change the working hook.

Only freshly enumerated current-run player pointers are used, with bounds on
every vector/range. A pre-existing null costume 4 is never claimed or removed.
Native calls execute after original IsaacUpdate, on its serialized owner thread.
For PlayerType 0–40, 0.7.0 reads the verified PlayerType field and asks the game
to load the matching Specialist ANM2 into the null-4 costume entry. The ANM2
Load and LoadGraphics functions and ChangePlayerType implementation are
fingerprinted for the target NRO. A player-type change removes and recreates
only the costume owned by this bridge.

## Local assets

The 0.6.0 test reused null costume 4 (Gnawed Leaf statue), replacing its art.
The 0.7.0 package restores the original Gnawed Leaf ANM2, texture and cache
actor. It appends 41 Specialist cache actors, includes each referenced sheet,
and retains the original costume-4 priority/flying settings needed for the
animation to render above character costumes. Assets remain local; the
repository and Actions artifacts contain no game asset cache.

`build_specialist_070_assets.py` appends to the original Switch animation cache
without replacing existing actors. It serializes the provided character ANM2s
and converts their source PNGs to four-plane PCX, independently decoding each
conversion and checking every RGBA pixel. It also prepares an isolated Ogg
Vorbis Specialist track as music ID 19; the native code verifies the loaded
music-table path before calling `Music::Play`, and calls `Room::PlayMusic` when
the condition ends. Track 19 is registered from a separate
`specialist_music.xml` via the verified `Music::LoadConfig(path,true)` path;
the true flag preserves existing base and DLC music entries.

Assets are mounted under `romfs/rp_patch/resources/`. Cache format and audio
metadata are verified offline. New actor mounting, all character drawings,
quality selection, music playback/restoration, and no regressions to original
tracks still require device confirmation. Snapshot 0.7.0 reports contract,
requests, additions, removals and music starts/restores/failures; those counters
do not prove audible or visual success.

Rollback: restore the seven runtime files from the pre-0.7.0 SD backup after
checking BEFORE.json hashes. Leave current diagnostic logs intact. New isolated
Specialist assets may remain unreferenced by the restored 0.6.0 runtime; no
recursive deletion is needed.
