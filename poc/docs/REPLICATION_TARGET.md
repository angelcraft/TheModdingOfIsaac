# Repentance-first compatibility target

User confirmed on 2026-10-19 that Repentance DLC is installed and enabled. Therefore, **Repentance is the primary research target**, while Afterbirth+ remains a separately versioned fallback/reference target.

## Candidate executable from the previously inspected update archive

- Relative path: `Program #0/1/.nro/Repentance.nro`
- Archive update Title ID in filename: `010021C000B6A800`
- Package version in filename: `v524288`
- Reported size: `11,210,752` bytes
- Reported SHA-256: `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`
- Repentance resource layer: `Program #0/1/rp_patch/resources/`

These values come from a prior read-only report for the supplied archive. They are not yet re-hashed from a file in this work session and do not prove the console currently loads that exact binary. Before any integration work, re-identify the launched Title ID/update/DLC state and obtain the SHA-256 + NRO ModuleId from the exact target file.

## Version boundary

Never apply the Afterbirth+ NRO symbol values, resource IDs, animation paths, or offsets to Repentance. Keep target metadata and all future reports explicitly labelled `repentance` or `afterbirthplus`. The presence of Repentance-only items such as Almond Milk must not be inferred from the update archive alone; actual DLC activation is supplied by the user but the on-device executable identity still needs confirming.
