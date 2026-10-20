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

namespace {
constexpr size_t kMaxScriptBytes = 256 * 1024;
char g_heap[0x10000];
void* g_original_lr = nullptr;
char g_log_path[128] = {};
FILE* g_log_file = nullptr;

void trace(const char* message) {
    if (g_log_file && message) {
        SaltySDCore_fwrite(message, 1, std::strlen(message), g_log_file);
        SaltySDCore_fwrite("\n", 1, 1, g_log_file);
    }
    if (message) svcOutputDebugString(message, std::strlen(message));
}

void probe_builtin_symbol(const char* label, const char* mangled_name) {
    const uintptr_t address = SaltySDCore_FindSymbolBuiltin(mangled_name);
    char line[192] = {};
    std::snprintf(
        line,
        sizeof(line),
        "builtin symbol %s: %s (0x%llX)",
        label,
        address ? "FOUND" : "not found",
        static_cast<unsigned long long>(address));
    trace(line);
}

void probe_repentance_symbols() {
    // Read-only lookup: this does not call or patch the returned game addresses.
    probe_builtin_symbol(
        "Manager::Update",
        "_ZN15IsaacRepentance7Manager6UpdateEv");
    probe_builtin_symbol(
        "ModManager::RunModScript",
        "_ZN15IsaacRepentance10ModManager12RunModScriptERNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE");
}

int lua_log(lua_State* state) {
    const char* message = lua_tostring(state, 1);
    trace(message ? message : "(non-string)");
    return 0;
}

bool build_script_path(char* path, size_t path_size) {
    u64 title_id = 0;
    const Result result = svcGetInfo(&title_id, 18, CUR_PROCESS_HANDLE, 0);
    if (R_FAILED(result)) {
        trace("svcGetInfo failed while reading the running title id");
        return false;
    }

    const int written = std::snprintf(
        path,
        path_size,
        "sdmc:/SaltySD/plugins/%016llX/probe.lua",
        static_cast<unsigned long long>(title_id));
    if (written < 0 || static_cast<size_t>(written) >= path_size) {
        trace("script path is too long");
        return false;
    }

    const int log_written = std::snprintf(
        g_log_path,
        sizeof(g_log_path),
        "sdmc:/SaltySD/plugins/%016llX/probe.log",
        static_cast<unsigned long long>(title_id));
    if (log_written < 0 || static_cast<size_t>(log_written) >= sizeof(g_log_path)) return false;
    return true;
}

bool execute_probe() {
    char path[128] = {};
    if (!build_script_path(path, sizeof(path))) return false;

    g_log_file = SaltySDCore_fopen(g_log_path, "wb");
    if (!g_log_file) {
        const char* message = "IsaacLuaBridge: could not create probe.log";
        svcOutputDebugString(message, std::strlen(message));
        return false;
    }
    trace("SaltyNX plugin entered; preparing Lua 5.1");
    probe_repentance_symbols();

    FILE* file = SaltySDCore_fopen(path, "rb");
    if (!file) {
        trace("probe.lua was not found beside the plugin");
        return false;
    }

    SaltySDCore_fseek(file, 0, SEEK_END);
    const int64_t file_size = SaltySDCore_ftell(file);
    SaltySDCore_fseek(file, 0, SEEK_SET);
    if (file_size < 0 || static_cast<uint64_t>(file_size) > kMaxScriptBytes) {
        trace("probe.lua has an invalid size (limit is 256 KiB)");
        SaltySDCore_fclose(file);
        return false;
    }

    const size_t size = static_cast<size_t>(file_size);
    char* source = static_cast<char*>(std::malloc(size ? size : 1));
    if (!source) {
        trace("not enough memory to read probe.lua");
        SaltySDCore_fclose(file);
        return false;
    }

    const size_t bytes_read = size ? SaltySDCore_fread(source, 1, size, file) : 0;
    SaltySDCore_fclose(file);
    if (bytes_read != size) {
        trace("could not read all of probe.lua");
        std::free(source);
        return false;
    }

    lua_State* state = luaL_newstate();
    if (!state) {
        trace("luaL_newstate failed");
        std::free(source);
        return false;
    }

    luaL_openlibs(state);
    lua_register(state, "log", lua_log);
    trace("executing probe.lua");

    int status = luaL_loadbuffer(state, source, size, "@probe.lua");
    std::free(source);
    if (status == 0) status = lua_pcall(state, 0, 0, 0);
    if (status != 0) {
        const char* error = lua_tostring(state, -1);
        trace(error ? error : "Lua failed with an unknown error");
    } else {
        trace("Lua completed successfully");
    }

    lua_close(state);
    return status == 0;
}
} // namespace

extern "C" {
extern u32 __start__;
extern char* fake_heap_start;
extern char* fake_heap_end;
void __libc_init_array(void);
void __libc_fini_array(void);
void __nx_exit(int, void*);
void virtmemSetup(void);

void __libnx_init(void* context, Handle main_thread, void* saved_lr) {
    (void)context;
    (void)main_thread;
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
    const bool ok = execute_probe();
    trace(ok ? "plugin finished successfully" : "plugin finished with an error");
    if (g_log_file) {
        SaltySDCore_fclose(g_log_file);
        g_log_file = nullptr;
    }
    return ok ? 0 : 1;
}

