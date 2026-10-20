# Specialist Lua 5.1 source preparation

This preparation step uses the supplied Specialist for Good Items 1.5.2 source tree. The helper copies the tree to a new directory and changes only the copied `main.lua`; it preserves the original as `main.lua.original`.

## Guarded edits

The helper refuses any `main.lua` whose SHA-256 differs from the reviewed Workshop source. It requires one exact match for each edit:

- Translate the one floor-division expression and the one Curse of the Blind bit test into Lua 5.1 arithmetic.
- Correct the `SpecialistModAPI` costume-table lookup to test the requested table/key.
- Make `RemoveAnimation` return the documented success boolean.

It also refuses an existing output directory so it cannot overwrite an earlier candidate.

## Use

```sh
python poc/tools/prepare_specialist_lua51.py path/to/extracted-mod path/to/new-output
```

The generated folder includes the original mod files and `SWITCH_PORT_STATUS.md` describing its state.

## Boundary

This only prepares source syntax and two source-level API defects. It does not implement `RegisterMod`, Isaac callbacks/globals, entity/player mapping, costumes, music, JSON persistence, or resource registration. The existing Lua Switch NRO is a separate homebrew program and does not load this folder inside Isaac. The output is not an installable or playable Switch mod, and it does not authorize a game binary patch.
