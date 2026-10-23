#pragma once
// Exact-build drift guards; no original executable bytes.
static const struct { uintptr_t offset; size_t size; u64 fingerprint; } kDanceGuards[] = {
    {0x291BBC,0x44,0xB956850D96D9AAFFULL},
    {0x2F2170,0x40,0xE6DBD362CA2616AFULL},
    {0x3C0CC4,0x28,0x417BC6ADA73E62FEULL},
    {0x290C38,0x8A8,0xD857E309CA3BB113ULL},
    {0x2914E0,0x16C,0x700974D17633C631ULL},
    {0x277B74,0xB0,0x82B2805E5E36A2F2ULL},
    {0x6730A0,0x10,0xDB6B53F01B263885ULL},
    {0x6716F0,0x10,0xED5AFCF6825FABB6ULL},
    {0x678230,0x10,0x0A31A6EE58AEEB8AULL},
    {0xC970,0x1FC,0xCF2D49D95481937BULL},
    {0xCB6C,0x8,0xD3B86355E5FEB39BULL},
    {0xA1F4,0xF4,0x0FC4A98DB0E415E8ULL}, // ANM2::Play(const char*,bool)
    {0xA454,0x60,0x1323FD2292CF041FULL}, // ANM2::IsPlaying(const char*)
    {0x2AACCC,0x1410,0x1B502654BCEB5506ULL},
};
static const struct { uintptr_t got, target; } kDanceBindings[] = {
    {0xA9F7B8,0x3C0CC4},
    {0xA9EAE0,0x290C38},
    {0xAA2080,0x2914E0},
};
