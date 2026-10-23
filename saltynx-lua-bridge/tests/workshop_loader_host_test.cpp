#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

using u32 = std::uint32_t;
using u64 = std::uint64_t;
constexpr int kLuaInstructionBudget = 100000;

static std::string g_test_root;
static std::vector<std::string> g_trace;
static uintptr_t g_room_token = 0x1234;

static void trace(const char* message) {
    const std::string text = message ? message : "(null)";
    g_trace.push_back(text);
    std::cout << text << '\n';
}

static int host_log(lua_State* state) {
    trace(luaL_checkstring(state, 1));
    return 0;
}

static void lua_instruction_limit(lua_State* state, lua_Debug*) {
    luaL_error(state, "host test instruction budget exceeded");
}

static bool read_current_room_token(uintptr_t& token) {
    token = g_room_token;
    return true;
}

static std::string map_sd_path(const char* path) {
    static const char prefix[] = "sdmc:/switch/TheModdingOfIsaac/";
    const std::string input(path ? path : "");
    if (input.compare(0, sizeof(prefix) - 1, prefix) != 0) return {};
    return g_test_root + "/" + input.substr(sizeof(prefix) - 1);
}

static FILE* SaltySDCore_fopen(const char* path, const char* mode) {
    const std::string mapped = map_sd_path(path);
    return mapped.empty() ? nullptr : std::fopen(mapped.c_str(), mode);
}
static int SaltySDCore_fseek(FILE* file, std::int64_t offset, int origin) {
    return fseeko(file, static_cast<off_t>(offset), origin);
}
static std::int64_t SaltySDCore_ftell(FILE* file) {
    return static_cast<std::int64_t>(ftello(file));
}
static std::size_t SaltySDCore_fread(void* buffer, std::size_t size,
                                     std::size_t count, FILE* file) {
    return std::fread(buffer, size, count, file);
}
static int SaltySDCore_fclose(FILE* file) {
    return std::fclose(file);
}

#include "../source/workshop_loader.h"
#include "../source/item_config.h"

static bool has_trace(const char* expected) {
    for (const auto& line : g_trace)
        if (line == expected) return true;
    return false;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: workshop_loader_host_test <workshop_demo_root>\n";
        return 2;
    }
    g_test_root = argv[1];

    lua_State* state = luaL_newstate();
    if (!state) {
        std::cerr << "could not create Lua state\n";
        return 2;
    }
    luaL_openlibs(state);
    lua_register(state, "log", host_log);
    lua_newtable(state);
    lua_setglobal(state, "IsaacBridge");
    workshop_register_compat(state);
    register_item_config_api(state);
    if (luaL_dostring(state,
        "local c = Isaac.GetItemConfig(); "
        "local hi = c:GetCollectible(4); "
        "assert(hi and hi.ID == 4 and hi.Quality == 4); "
        "local lo = Isaac.GetItemConfig().GetCollectible(502); "
        "assert(lo and lo.Quality == 1); "
        "assert(c:GetCollectible(999) == nil)") != 0) {
        std::cerr << "ItemConfig metadata/API mismatch: " << lua_tostring(state, -1) << '\n';
        lua_close(state);
        return 1;
    }

    workshop_load_manifest(state);
    if (g_workshop.loaded != 2 || g_workshop.errors != 0 ||
        g_workshop.registered_post_update != 2 ||
        g_workshop.registered_post_new_room != 2) {
        std::cerr << "manifest/registration mismatch: loaded=" << g_workshop.loaded
                  << " errors=" << g_workshop.errors
                  << " post_update=" << g_workshop.registered_post_update
                  << " new_room=" << g_workshop.registered_post_new_room << '\n';
        lua_close(state);
        return 1;
    }
    if (!has_trace("Workshop isolation: PASS (globals, libraries, API tables)")) {
        std::cerr << "second mod did not see isolated environment\n";
        lua_close(state);
        return 1;
    }

    workshop_dispatch_post_update(state);
    if (g_workshop.callback_errors != 0 || g_workshop.callback_success != 4 ||
        g_workshop.room_callback_success != 2 ||
        !has_trace("Workshop demo: callback mod argument OK") ||
        !has_trace("Workshop isolation: callback argument OK") ||
        !has_trace("Workshop demo: MC_POST_NEW_ROOM callback with mod argument") ||
        !has_trace("Workshop isolation: MC_POST_NEW_ROOM callback")) {
        std::cerr << "callback dispatch/isolation mismatch: success="
                  << g_workshop.callback_success << " errors="
                  << g_workshop.callback_errors << " room_success="
                  << g_workshop.room_callback_success << '\n';
        lua_close(state);
        return 1;
    }

    lua_close(state);
    std::cout << "Workshop loader host integration: PASS (2 mods, isolated state, "
                 "4 callbacks, mod self argument)\n";
    return 0;
}
