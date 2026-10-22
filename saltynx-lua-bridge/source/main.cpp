#include <switch_min.h>
#include <saltysd/saltysd_core.h>
#include <saltysd/saltysd_dynamic.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstdlib>
#include <cstdio>
#include <cstring>

extern "C" {
Result SaltySD_Init() __attribute__((weak));
Result SaltySD_Deinit() __attribute__((weak));
}

namespace {
constexpr u64 kExpectedTitleId = 0x010021C000B6A000ULL;
constexpr uintptr_t kCallerReturnOffset = 0x4C0A10;
constexpr uintptr_t kIsaacUpdateOffset = 0x3B37F0;
constexpr uintptr_t kMinimumTextSize = 0x68D000;
constexpr size_t kMaxScriptBytes = 256 * 1024;
constexpr unsigned kCallbackLimit = 3;

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
FILE* g_startup_log = nullptr;
unsigned g_frame = 0;
unsigned g_callback_count = 0;
bool g_inside_hook = false;
bool g_validation_failed = false;
bool g_runtime_result_written = false;

bool validate_image(uintptr_t base) {
    MemoryInfo info = {};
    u32 page_info = 0;
    if (R_FAILED(svcQueryMemory(&info, &page_info, base)) ||
        info.addr != base || !(info.perm & Perm_X) || info.size < kMinimumTextSize) return false;
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
    } else if (g_callback_count == 1) {
        trace("IsaacLuaBridge: first in-game Lua callback completed");
        if (!g_runtime_result_written && SaltySD_Init && SaltySD_Deinit &&
            R_SUCCEEDED(SaltySD_Init())) {
            FILE* result = SaltySDCore_fopen(
                "sdmc:/SaltySD/plugins/010021C000B6A000/bridge-runtime.log", "wb");
            if (result) {
                const char* message =
                    "contract=verified\noriginal_isaac_update=called\n"
                    "lua_on_update=success\ncallback_limit=3\n";
                SaltySDCore_fwrite(message, 1, std::strlen(message), result);
                SaltySDCore_fclose(result);
                g_runtime_result_written = true;
            }
            SaltySD_Deinit();
        }
    }
}

extern "C" void isaac_update_hook() {
    if (g_inside_hook || g_validation_failed) return;
    g_inside_hook = true;

    if (!g_original_update) {
        const uintptr_t caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
        if (!resolve_verified_original(caller)) {
            g_validation_failed = true;
            trace("IsaacLuaBridge: runtime contract mismatch; callback disabled");
            g_inside_hook = false;
            return;
        }
    }

    g_original_update();
    ++g_frame;
    dispatch_lua_update();
    g_inside_hook = false;
}

void bridge_worker(void*) {
    for (unsigned attempt = 0; attempt < 1200; ++attempt) {
        uintptr_t address = 0;
        while (true) {
            MemoryInfo info = {};
            u32 page_info = 0;
            if (R_FAILED(svcQueryMemory(&info, &page_info, address))) break;
            const uintptr_t next = info.addr + info.size;
            if ((info.perm & Perm_X) && info.size >= kMinimumTextSize &&
                validate_image(info.addr)) {
                const uintptr_t got = info.addr + 0xAA5C20;
                MemoryInfo got_info = {};
                if (R_SUCCEEDED(svcQueryMemory(&got_info, &page_info, got)) &&
                    (got_info.perm & Perm_W)) {
                    void** slot = reinterpret_cast<void**>(got);
                    void* expected = reinterpret_cast<void*>(info.addr + kIsaacUpdateOffset);
                    if (*slot == expected) {
                        g_original_update = reinterpret_cast<IsaacUpdate>(expected);
                        __atomic_store_n(slot, reinterpret_cast<void*>(isaac_update_hook), __ATOMIC_RELEASE);
                        trace("IsaacLuaBridge: verified IsaacUpdate GOT installed");
                    }
                }
                return;
            }
            if (next <= address) break;
            address = next;
        }
        svcSleepThread(25 * 1000 * 1000LL);
    }
    trace("IsaacLuaBridge: Repentance NRO wait timed out");
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
    trace("IsaacLuaBridge 0.3.0: preparing guarded frame callback");
    if (!prepare_lua()) {
        if (g_startup_log) SaltySDCore_fclose(g_startup_log);
        g_startup_log = nullptr;
        return 1;
    }

    Handle worker = INVALID_HANDLE;
    const Result worker_result = svcCreateThread(
        &worker, reinterpret_cast<void*>(bridge_worker), nullptr,
        g_worker_stack + sizeof(g_worker_stack), 0x2C, -2);
    if (R_FAILED(worker_result) || R_FAILED(svcStartThread(worker))) {
        trace("IsaacLuaBridge: could not start NRO wait thread");
        if (g_startup_log) SaltySDCore_fclose(g_startup_log);
        g_startup_log = nullptr;
        return 1;
    }
    trace("IsaacLuaBridge: verified NRO wait thread started");

    if (g_startup_log) SaltySDCore_fclose(g_startup_log);
    g_startup_log = nullptr;
    return 0;
}

