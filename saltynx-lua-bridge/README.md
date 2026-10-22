# IsaacLuaBridge 0.6.0 — Specialist candidate

Preserves the confirmed 0.5.0 baseline. This build adds guarded native costume
calls and a Lua rule for pedestal item 502. It is not yet validated on device.

The Actions artifact contains the ELF and Lua only. Use the complete locally
prepared SD package, including its LayeredFS animation assets. Test as normal
Isaac in single player: enter the room containing item 502, leave, re-enter,
then pick it up. Dance should start with the pedestal and stop without it.

This temporary test replaces Gnawed Leaf's statue costume with Specialist.
Music, quality rules and other characters are not implemented.

See DANCE_CONTRACT.md for exact-build guards, asset conversion and rollback.