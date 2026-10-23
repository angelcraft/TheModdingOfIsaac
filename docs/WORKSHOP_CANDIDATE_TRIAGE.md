# Workshop candidate triage

The experimental bridge currently supports startup loading of a single `main.lua`
per mod and only `MC_POST_UPDATE`, plus a candidate `MC_POST_NEW_ROOM` signal.
It does not yet resolve `require`, import resource folders, or expose general
PC `Game`, entity, player, and music APIs. Before copying another Workshop mod
to the SD, inspect it with:

```text
python tools/import_workshop_candidate.py "C:\path\to\workshop\mod"
```

This tool reads only local files. It emits a conservative blocker list and
SHA-256 of `main.lua`; it never uploads or modifies the Workshop source. A
`candidate: true` result means only that the static scan did not find a known
blocker. Lua can call APIs dynamically and the scan can flag words in comments
or strings, so actual compatibility still requires a game test.

To stage a candidate into a **new** SD package directory, use `--stage` and
`--mod-id`. The original `main.lua` is copied byte-for-byte. The tool refuses
to overwrite an existing output directory. The package contains `mods.txt`,
the mod script, and a report; it does not include or alter the bridge ELF.
Keep a backup of your existing SD files before installing such a package.

The six mods currently downloaded in the user's Steam Workshop directory were
checked locally on 2026-10-23. None is immediately stageable with the present
bridge: GoodTrip needs `require` and 16 extra files; Specialist needs four more
callbacks, several PC game APIs, and 97 extra files; the remaining four require
additional callbacks and/or game APIs. This is a capability inventory, not a
claim that those mods can never be ported. The next hardware test should be the
existing two-mod Lua smoke test while the bridge's `ItemConfig` API is checked.

No Build ID, hook, native call, binary patch, or game resource is changed by
this staging tool.

