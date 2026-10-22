# Native bridge contract: IsaacUpdate, 2026-10-22

Target NRO SHA256 CC363208C220F65547EDDA6D8268B3B6C0F7373B14C91AE6FFB378018DE4B66A.
Device NRO 0.1.0 observation confirmed both Build IDs and four entry windows.
Disassembly was read from the existing symbolized project with analysis disabled,
one CPU and Java heap capped at 512 MB. The process finished and closed.

Selected candidate: IsaacRepentance::IsaacUpdate, entry +0x3B37F0,
length 0x24. Its PLT is +0x67F970, JUMP_SLOT +0xAA5C20.
The enclosing game loop calls it at +0x4C0A0C; return site +0x4C0A10
overwrites d0 and does not consume an integer return value.

IsaacUpdate saves x29/x30 in a 16-byte aligned stack frame, supplies false
in w0 to KAGE::EngineUpdate, loads Manager* through +0xAAC648 into x0,
restores x29/x30 and SP, and tail-branches through Manager::Update PLT
+0x67A2B0 (GOT +0xAA30C0). It consumes no incoming argument registers.
Manager::Update takes its receiver in x0, preserves x19-x23 and x29/x30,
and returns through its restored LR after restoring its 0x50-byte frame.
Candidate calling shape: void IsaacUpdate(void), with a normal AAPCS64
compiled wrapper calling the unchanged original and returning normally.
No inline trampoline is needed if only the corresponding import slot is
replaced: no game instructions would be displaced. This is a proposed
contract, not an installed hook or proof of successful runtime callbacks.

Official SaltyNX tag 2.1.2, commit af8941410ff13b63586b615bf5a0eba340d782fd:
saltysd_core/source/saltysd_dynamic.c records replacement names in
ReplaceModuleImport and applies them after successful nn::ro::LoadModule.
That supports late NRO import interception in principle. Repeat registration
is not safe to assume idempotent; preserve earlier observations about addends.
Do not use the startup-only symbol registry to resolve the later NRO.
Core's file IPC is deinitialized after synchronous startup; the future
per-frame bridge must not use those file APIs after plugin return.

Remaining device evidence: NRO 0.2.0 checks 13 full function/PLT SHA256
digests, all six GOT bindings in the corrected frame route, and the loader's
run_repentance pointer against the independently verified NRO header.
The contract generator validates exact dynamic symbol extents and branches,
and rejects relocations inside hashed instructions. It publishes only
hashes and relative offsets, never game binary code.

Native-hook gate remains in force until this evidence is evaluated. A successful
reader does not itself implement Lua callbacks, player/pickup APIs or the mod.
