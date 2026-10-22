# Specialist 0.6.0 device candidate

Stable 0.5.0 remains at `stable/isaacluabridge-0.5.0` / e9b20cc807047c4307d2bcf4cdcbb59310516508.
Its device reader observed item 502, 7320 successful Lua callbacks, zero callback
errors, 488 scans and zero read errors. This is not evidence of animation success.

## Lua rule

Every third update, read the current room. Apply while an uncollected type-5,
variant-100 pedestal with subtype 502 is present. Remove when absent, including
pickup and leaving its room. No range or input check. This candidate does not
implement the Workshop mod's quality, visibility, music, other characters or API.
Multiple adjacent qualifying rooms keep the dance active. Use one normal Isaac.

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

## Local assets

This temporary visual test reuses null costume 4 (Gnawed Leaf statue), replacing
its ANM2, RGBA PCX, cache actor and XML priority/flying metadata via LayeredFS.
Consequently Gnawed Leaf's statue visual is also changed during this test.
No new unknown costume ID or animation method is invoked. Assets remain local;
the repository and Actions artifacts contain no game asset cache.

`build_specialist_assets.py` serializes the complete original ANM2 into a
byte-exact 4816-byte cache actor at offset5092462 before accepting replacement.
It preserves the original cache key and every byte outside that actor. The
replacement comes from the provided Specialist Isaac animation, using the
existing registered statue-head texture path. The generated 4-plane PCX is
independently decoded and compared against all source RGBA pixels.

Assets are mounted under `romfs/rp_patch/resources/`. Cache-format agreement is
verified offline; the mount, native calls and actual visible dance still require
the device. Snapshot 0.6.0 reports contract, requests, additions, removals and
failure codes, and deliberately never claims visual success from those counts.

Rollback: restore the preserved 0.5.0 ELF/Lua and remove only the four files
listed in the local installer manifest, restoring prior files from its backup.
