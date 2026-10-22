#include <switch_min.h>
#include <saltysd/saltysd_core.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstdlib>
#include <cstdio>
#include <cstring>


namespace {
constexpr u64 kExpectedTitleId = 0x010021C000B6A000ULL;
constexpr uintptr_t kCallerReturnOffset = 0x4C0A10;
constexpr uintptr_t kIsaacUpdateOffset = 0x3B37F0;
constexpr uintptr_t kMinimumTextSize = kCallerReturnOffset + 8;
constexpr size_t kMaxScriptBytes = 256 * 1024;
constexpr unsigned kCallbackLimit = 3;

// Query the actual mapping of each read; text and GOT live in different mappings.
bool readable_range(uintptr_t address, size_t length) {
    MemoryInfo info = {};
    u32 page_info = 0;
    if (!length || R_FAILED(svcQueryMemory(&info, &page_info, address)) ||
        !(info.perm & Perm_R) || address < info.addr) return false;
    const uintptr_t offset = address - info.addr;
    return offset < info.size && length <= info.size - offset;
}

const u8 kExpectedBuildId[0x20] = {
    0x91, 0xC7, 0x3F, 0xDD, 0x57, 0x50, 0x61, 0x31,
    0x8D, 0x68, 0x88, 0x63, 0x16, 0xAF, 0xEA, 0xC7,
    0x23, 0x88, 0xB2, 0xAB, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const u8 kExpectedIsaacUpdate[0x24] = {
    0xFD, 0x7B, 0xBF, 0xA9, 0xFD, 0x03, 0x00, 0x91,
    0xE0, 0x03, 0x1F, 0x2A, 0xD9, 0x1F, 0x0B, 0x94,
    0xC8, 0x37, 0x00, 0xB0, 0x08, 0x25, 0x43, 0xF9,
    0x00, 0x01, 0x40, 0xF9, 0xFD, 0x7B, 0xC1, 0xA8,
    0xA8, 0x1A, 0x0B, 0x14,
};

// BL IsaacUpdate PLT followed by the verified instruction at the return site.
const u8 kExpectedCallBoundary[8] = {
    0xD9, 0xFB, 0x06, 0x94, 0x40, 0x39, 0x69, 0x1E,
};

using IsaacUpdate = void (*)();

alignas(16) char g_heap[0x40000];
alignas(0x1000) char g_worker_stack[0x4000];
void* g_original_lr = nullptr;
lua_State* g_lua = nullptr;
IsaacUpdate g_original_update = nullptr;
uintptr_t g_main_base = 0;
FILE* g_startup_log = nullptr;
unsigned g_frame = 0;
unsigned g_callback_count = 0;
alignas(4) u32 g_hook_busy = 0;
bool g_validation_failed = false;
// Readable from a memory snapshot; no IPC is performed by worker diagnostics.
// 1=waiting, 2=NRO verified, 3=GOT original, 4=installed,
// 5=read-only GOT, 6=unexpected GOT, 7=timeout, 8=Lua success, 9=Lua error.
alignas(8) u32 g_bridge_stage = 0;
void set_stage(u32 value) {
    u32 old = __atomic_load_n(&g_bridge_stage, __ATOMIC_ACQUIRE);
    do {
        // A completed callback must not be overwritten by the installing worker.
        if (old == 8 || old == 9) return;
    } while (!__atomic_compare_exchange_n(&g_bridge_stage, &old, value, false,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
}

bool validate_image(uintptr_t base) {
    MemoryInfo info = {};
    u32 page_info = 0;
    if (R_FAILED(svcQueryMemory(&info, &page_info, base)) ||
        info.addr != base || !(info.perm & Perm_X) || info.size < kMinimumTextSize ||
        base > UINTPTR_MAX - 0xAA5C20 - sizeof(void*) ||
        !readable_range(base, 0x60) ||
        !readable_range(base + kIsaacUpdateOffset, sizeof(kExpectedIsaacUpdate)) ||
        !readable_range(base + kCallerReturnOffset - 4, sizeof(kExpectedCallBoundary))) return false;
    const u8* image = reinterpret_cast<const u8*>(base);
    return std::memcmp(image + 0x10, "NRO0", 4) == 0 &&
        std::memcmp(image + 0x40, kExpectedBuildId, sizeof(kExpectedBuildId)) == 0 &&
        std::memcmp(image + kIsaacUpdateOffset, kExpectedIsaacUpdate,
                    sizeof(kExpectedIsaacUpdate)) == 0 &&
        std::memcmp(image + kCallerReturnOffset - 4, kExpectedCallBoundary,
                    sizeof(kExpectedCallBoundary)) == 0;
}

void trace(const char* message) {
    if (!message) return;
    svcOutputDebugString(message, std::strlen(message));
    if (g_startup_log) {
        SaltySDCore_fwrite(message, 1, std::strlen(message), g_startup_log);
        SaltySDCore_fwrite("\n", 1, 1, g_startup_log);
    }
}


int lua_log(lua_State* state) {
    const char* message = lua_tostring(state, 1);
    trace(message ? message : "(Lua non-string)");
    return 0;
}

bool read_script(char** output, size_t* output_size) {
    FILE* file = SaltySDCore_fopen(
        "sdmc:/SaltySD/plugins/010021C000B6A000/bridge.lua", "rb");
    if (!file) {
        trace("IsaacLuaBridge: bridge.lua not found");
        return false;
    }

    SaltySDCore_fseek(file, 0, SEEK_END);
    const int64_t length = SaltySDCore_ftell(file);
    SaltySDCore_fseek(file, 0, SEEK_SET);
    if (length <= 0 || static_cast<uint64_t>(length) > kMaxScriptBytes) {
        SaltySDCore_fclose(file);
        trace("IsaacLuaBridge: invalid bridge.lua size");
        return false;
    }

    char* source = static_cast<char*>(std::malloc(static_cast<size_t>(length)));
    if (!source) {
        SaltySDCore_fclose(file);
        trace("IsaacLuaBridge: script allocation failed");
        return false;
    }
    const size_t read = SaltySDCore_fread(source, 1, static_cast<size_t>(length), file);
    SaltySDCore_fclose(file);
    if (read != static_cast<size_t>(length)) {
        std::free(source);
        trace("IsaacLuaBridge: incomplete bridge.lua read");
        return false;
    }
    *output = source;
    *output_size = read;
    return true;
}

bool prepare_lua() {
    char* source = nullptr;
    size_t source_size = 0;
    if (!read_script(&source, &source_size)) return false;

    g_lua = luaL_newstate();
    if (!g_lua) {
        std::free(source);
        trace("IsaacLuaBridge: luaL_newstate failed");
        return false;
    }
    luaL_openlibs(g_lua);
    lua_register(g_lua, "log", lua_log);

    int status = luaL_loadbuffer(g_lua, source, source_size, "@bridge.lua");
    std::free(source);
    if (status == 0) status = lua_pcall(g_lua, 0, 0, 0);
    if (status != 0) {
        trace(lua_tostring(g_lua, -1));
        lua_close(g_lua);
        g_lua = nullptr;
        return false;
    }

    lua_getglobal(g_lua, "on_update");
    const bool valid = lua_isfunction(g_lua, -1);
    lua_pop(g_lua, 1);
    if (!valid) {
        trace("IsaacLuaBridge: bridge.lua must define on_update(frame)");
        lua_close(g_lua);
        g_lua = nullptr;
        return false;
    }
    trace("IsaacLuaBridge: persistent Lua state ready");
    return true;
}

bool resolve_verified_original(uintptr_t return_address) {
    if (return_address < kCallerReturnOffset) return false;
    const uintptr_t base = return_address - kCallerReturnOffset;

    if (!validate_image(base)) return false;

    g_original_update = reinterpret_cast<IsaacUpdate>(base + kIsaacUpdateOffset);
    trace("IsaacLuaBridge: live Repentance contract verified");
    return true;
}

void dispatch_lua_update() {
    if (!g_lua || g_callback_count >= kCallbackLimit) return;
    lua_getglobal(g_lua, "on_update");
    lua_pushinteger(g_lua, static_cast<lua_Integer>(g_frame));
    const int status = lua_pcall(g_lua, 1, 0, 0);
    ++g_callback_count;
    if (status != 0) {
        trace(lua_tostring(g_lua, -1));
        lua_pop(g_lua, 1);
        g_callback_count = kCallbackLimit;
        set_stage(9);
    } else if (g_callback_count == 1) {
        trace("IsaacLuaBridge: first in-game Lua callback completed");
        set_stage(8);
    }
}

extern "C" void isaac_update_hook() {
    // Isaac's update thread must be the only Lua state owner.
    // If this hook is entered recursively or concurrently, call the original only.
    if (g_validation_failed ||
        __atomic_exchange_n(&g_hook_busy, 1u, __ATOMIC_ACQUIRE) != 0) {
        if (g_original_update) g_original_update();
        return;
    }

    if (!g_original_update) {
        const uintptr_t caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
        if (!resolve_verified_original(caller)) {
            g_validation_failed = true;
            trace("IsaacLuaBridge: runtime contract mismatch; callback disabled");
            __atomic_store_n(&g_hook_busy, 0u, __ATOMIC_RELEASE);
            return;
        }
    }

    g_original_update();
    ++g_frame;
    dispatch_lua_update();
    __atomic_store_n(&g_hook_busy, 0u, __ATOMIC_RELEASE);
}

[[noreturn]] void worker_exit() {
    svcExitThread();
    for (;;) {}
}

void bridge_worker(void*) {
    set_stage(1);
    // The verified main loader slot supplies the late NRO base directly.
    // SaltySDCore_getCodeStart() points at rtld; main follows its 0x4000 mapping.
    if (!g_main_base || g_main_base > UINTPTR_MAX - 0x10CC78)
        worker_exit();
    const uintptr_t loader_slot = g_main_base + 0x10CC78;
    for (unsigned attempt = 0; attempt < 4800; ++attempt) {
        if (readable_range(loader_slot, sizeof(uintptr_t))) {
            const uintptr_t entry = __atomic_load_n(
                reinterpret_cast<uintptr_t*>(loader_slot), __ATOMIC_ACQUIRE);
            if (entry >= 0x4C0840) {
                const uintptr_t base = entry - 0x4C0840;
                if (validate_image(base)) {
                    set_stage(2);
                    const uintptr_t got = base + 0xAA5C20;
                    MemoryInfo got_info = {};
                    u32 page_info = 0;
                    if (R_SUCCEEDED(svcQueryMemory(&got_info, &page_info, got)) &&
                        readable_range(got, sizeof(void*))) {
                        void** slot = reinterpret_cast<void**>(got);
                        void* expected = reinterpret_cast<void*>(base + kIsaacUpdateOffset);
                        void* actual = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
                        if (actual == expected) {
                            set_stage(3);
                            g_original_update = reinterpret_cast<IsaacUpdate>(expected);
                            void* replacement = reinterpret_cast<void*>(isaac_update_hook);
                            if (!(got_info.perm & Perm_W)) {
                                set_stage(5);
                                worker_exit();
                            }
                            if (__atomic_compare_exchange_n(slot, &expected, replacement,
                                                            false, __ATOMIC_ACQ_REL,
                                                            __ATOMIC_ACQUIRE)) {
                                set_stage(4);
                                trace("IsaacLuaBridge: verified IsaacUpdate GOT installed");
                            } else {
                                set_stage(6);
                            }
                        } else {
                            set_stage(6);
                        }
                    } else {
                        set_stage(6);
                    }
                    worker_exit();
                }
            }
        }
        svcSleepThread(25 * 1000 * 1000LL);
    }
    set_stage(7);
    trace("IsaacLuaBridge: Repentance loader wait timed out");
    worker_exit();
}
} // namespace

extern "C" {
extern char* fake_heap_start;
extern char* fake_heap_end;
void __libc_init_array(void);
void __libc_fini_array(void);
void __nx_exit(int, void*);
void virtmemSetup(void);

void __libnx_init(void*, Handle, void* saved_lr) {
    fake_heap_start = &g_heap[0];
    fake_heap_end = &g_heap[sizeof(g_heap)];
    g_original_lr = saved_lr;
    __libc_init_array();
    virtmemSetup();
}

void __attribute__((weak, noreturn)) __libnx_exit(int result) {
    __libc_fini_array();
    __nx_exit(result, g_original_lr);
    for (;;) {}
}
}

int main(int, char**) {
    u64 title_id = 0;
    if (R_FAILED(svcGetInfo(&title_id, 18, CUR_PROCESS_HANDLE, 0)) ||
        title_id != kExpectedTitleId) {
        return 1;
    }

    g_startup_log = SaltySDCore_fopen(
        "sdmc:/SaltySD/plugins/010021C000B6A000/bridge-startup.log", "wb");
    trace("IsaacLuaBridge 0.3.5: guarded frame callback");
    if (!prepare_lua()) {
        if (g_startup_log) SaltySDCore_fclose(g_startup_log);
        g_startup_log = nullptr;
        return 1;
    }

    // Source of this main offset: the independently verified loader binding.
    const uintptr_t code_start = SaltySDCore_getCodeStart();
    if (!code_start || code_start > UINTPTR_MAX - 0x4000 - 0x10CC78) {
        trace("IsaacLuaBridge: main loader base unavailable");
        if (g_startup_log) SaltySDCore_fclose(g_startup_log);
        g_startup_log = nullptr;
        return 1;
    }
    g_main_base = code_start + 0x4000;
    MemoryInfo main_info = {};
    u32 page_info = 0;
    if (R_FAILED(svcQueryMemory(&main_info, &page_info, g_main_base)) ||
        main_info.addr != g_main_base || !(main_info.perm & Perm_X)) {
        trace("IsaacLuaBridge: main executable mapping mismatch");
        if (g_startup_log) SaltySDCore_fclose(g_startup_log);
        g_startup_log = nullptr;
        return 1;
    }
    trace("IsaacLuaBridge: main loader mapping verified");
    // These addresses allow the existing read-only Snapshot NRO to confirm
    // the callback without relying on IPC from the late worker/game thread.
    char marker[112] = {};
    std::snprintf(marker, sizeof(marker), "bridge_stage_address=0x%016llX",
                  static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(&g_bridge_stage)));
    trace(marker);
    std::snprintf(marker, sizeof(marker), "bridge_hook_address=0x%016llX",
                  static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(isaac_update_hook)));
    trace(marker);
    Handle worker = INVALID_HANDLE;
    const Result worker_result = svcCreateThread(
        &worker, reinterpret_cast<void*>(bridge_worker), nullptr,
        g_worker_stack + sizeof(g_worker_stack), 0x2C, -2);
    if (R_FAILED(worker_result)) {
        char line[96] = {};
        std::snprintf(line, sizeof(line),
                      "IsaacLuaBridge: worker create failed rc=0x%08X",
                      static_cast<unsigned>(worker_result));
        trace(line);
        if (g_startup_log) SaltySDCore_fclose(g_startup_log);
        g_startup_log = nullptr;
        return 1;
    }
    // The worker must never touch the SaltySD startup file IPC after
    // the plugin's main() returns. Close it *before* starting that thread.
    trace("IsaacLuaBridge: NRO worker created; starting");
    if (g_startup_log) SaltySDCore_fclose(g_startup_log);
    g_startup_log = nullptr;
    const Result start_result = svcStartThread(worker);
    if (R_FAILED(start_result)) {
        char line[96] = {};
        std::snprintf(line, sizeof(line),
                      "IsaacLuaBridge: worker start failed rc=0x%08X",
                      static_cast<unsigned>(start_result));
        trace(line);
        svcCloseHandle(worker);
        if (g_startup_log) SaltySDCore_fclose(g_startup_log);
        g_startup_log = nullptr;
        return 1;
    }
    svcCloseHandle(worker);
    trace("IsaacLuaBridge: NRO wait thread started");
    if (g_startup_log) SaltySDCore_fclose(g_startup_log);
    g_startup_log = nullptr;
    return 0;
}

