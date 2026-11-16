#pragma once

// Tiny host-only declarations to syntax-check main.cpp without pretending to
// emulate libnx or replace an actual devkitA64 compilation.
#include <cstddef>
#include <cstdint>

using Result = std::uint32_t;
using u64 = std::uint64_t;

inline bool R_FAILED(Result result) { return result != 0; }
inline bool R_SUCCEEDED(Result result) { return result == 0; }

struct PadState {};
struct SwkbdConfig {};

constexpr unsigned HidNpadStyleSet_NpadStandard = 1;
constexpr u64 HidNpadButton_A = 1 << 0;
constexpr u64 HidNpadButton_B = 1 << 1;
constexpr u64 HidNpadButton_X = 1 << 2;
constexpr u64 HidNpadButton_Y = 1 << 3;
constexpr u64 HidNpadButton_R = 1 << 4;
constexpr u64 HidNpadButton_L = 1 << 8;
constexpr u64 HidNpadButton_Plus = 1 << 5;
constexpr u64 HidNpadButton_Up = 1 << 6;
constexpr u64 HidNpadButton_Down = 1 << 7;

void consoleInit(void*);
void consoleExit(void*);
void consoleClear();
void consoleUpdate(void*);
void padConfigureInput(int, unsigned);
void padInitializeDefault(PadState*);
void padUpdate(PadState*);
u64 padGetButtonsDown(PadState*);
bool appletMainLoop();
void svcSleepThread(std::int64_t);
Result socketInitializeDefault();
void socketExit();
Result swkbdCreate(SwkbdConfig*, int);
void swkbdConfigMakePresetDefault(SwkbdConfig*);
void swkbdConfigSetHeaderText(SwkbdConfig*, const char*);
Result swkbdShow(SwkbdConfig*, char*, std::size_t);
void swkbdClose(SwkbdConfig*);
