#pragma once
// Exact-build drift guards; FNV is not cryptographic authentication.
static const struct { uintptr_t offset; size_t size; u64 fingerprint; } kPedestalGuards[] = {
    {0x3F8DB8,0x368,0xBC2DBACC43329F71ULL},
    {0x46D788,0x3384,0x398FBDA9CB04D06AULL},
    {0x7A00C,0x148,0x8B754C2BA5FFB966ULL},
    {0x8BF814,0x5,0xDC95AD3608D85F82ULL},
};
