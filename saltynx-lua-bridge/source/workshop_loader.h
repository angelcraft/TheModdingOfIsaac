#pragma once

// Optional first-step Workshop Lua loader. Runs SD file IPC ONLY during
// SaltyNX plugin startup. Lua callbacks run later, on IsaacUpdate's owner.
// No PC entity/music APIs are simulated; unsupported calls fail visibly.
// Do not run untrusted mods: Lua scripts are native-game-adjacent code.
// This whitelisted namespace is not a security boundary or a true VM sandbox.

constexpr size_t kWorkshopMaxMods = 4;
constexpr size_t kWorkshopMaxCallbacks = 32;
constexpr size_t kWorkshopMaxMainLua = 64 * 1024;
constexpr size_t kWorkshopMaxModuleFiles = 16;
constexpr size_t kWorkshopMaxModuleLua = 64 * 1024;
constexpr size_t kWorkshopMaxTotalLua = 256 * 1024;
constexpr int kWorkshopPostUpdate = 1;
constexpr int kWorkshopPostNewRoom = 2;

struct WorkshopCallback {
    int callback_id;
    int mod_ref;
    int function_ref;
    bool disabled;
};
WorkshopCallback g_workshop_callbacks[kWorkshopMaxCallbacks] = {};
size_t g_workshop_callback_count = 0;
struct WorkshopWitness {
    u32 magic, version, loaded, errors, callback_success, callback_errors,
        registered_post_update, registered_post_new_room, room_callback_success;
};
static_assert(sizeof(WorkshopWitness) == 36, "Snapshot Workshop witness layout");
alignas(8) WorkshopWitness g_workshop = {0x574D4431u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
bool g_workshop_loading = false;
char g_workshop_current_mod_id[41] = {};
size_t g_workshop_module_count = 0;
size_t g_workshop_total_lua_bytes = 0;
uintptr_t g_workshop_room_token = 0;
bool g_workshop_room_seen = false;

void workshop_release_from(lua_State* state, size_t first) {
    while (g_workshop_callback_count > first) {
        WorkshopCallback& cb = g_workshop_callbacks[--g_workshop_callback_count];
        luaL_unref(state, LUA_REGISTRYINDEX, cb.mod_ref);
        luaL_unref(state, LUA_REGISTRYINDEX, cb.function_ref);
        cb = {};
    }
    g_workshop.registered_post_update = 0;
    g_workshop.registered_post_new_room = 0;
    for (size_t i = 0; i < g_workshop_callback_count; ++i) {
        if (g_workshop_callbacks[i].callback_id == kWorkshopPostUpdate)
            ++g_workshop.registered_post_update;
        else if (g_workshop_callbacks[i].callback_id == kWorkshopPostNewRoom)
            ++g_workshop.registered_post_new_room;
    }
}

int lua_workshop_add_callback(lua_State* state) {
    if (!g_workshop_loading) return luaL_error(state, "AddCallback only supported while loading a mod");
    if (!lua_istable(state, 1) || !lua_isnumber(state, 2) ||
        !lua_isfunction(state, 3))
        return luaL_error(state, "AddCallback(mod, callback_id, function) required");
    const int callback_id = static_cast<int>(lua_tointeger(state, 2));
    if (callback_id != kWorkshopPostUpdate && callback_id != kWorkshopPostNewRoom)
        return luaL_error(state, "unsupported PC callback (MC_POST_UPDATE or MC_POST_NEW_ROOM available)");
    if (g_workshop_callback_count == kWorkshopMaxCallbacks)
        return luaL_error(state, "Workshop callback limit reached");
    WorkshopCallback& cb = g_workshop_callbacks[g_workshop_callback_count];
    cb.callback_id = callback_id;
    lua_pushvalue(state, 1);
    cb.mod_ref = luaL_ref(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 3);
    cb.function_ref = luaL_ref(state, LUA_REGISTRYINDEX);
    cb.disabled = false;
    ++g_workshop_callback_count;
    if (callback_id == kWorkshopPostUpdate) ++g_workshop.registered_post_update;
    else if (callback_id == kWorkshopPostNewRoom) ++g_workshop.registered_post_new_room;
    return 0;
}

int lua_workshop_register_mod(lua_State* state) {
    if (!g_workshop_loading)
        return luaL_error(state, "RegisterMod is available only during mod startup");
    const char* name = luaL_checkstring(state, 1);
    const int api_version = static_cast<int>(luaL_checkinteger(state, 2));
    if (!name || !*name || std::strlen(name) > 96 || api_version != 1)
        return luaL_error(state, "invalid mod name or API version");
    lua_createtable(state, 0, 2);
    lua_pushstring(state, name);
    lua_setfield(state, -2, "Name");
    lua_pushcfunction(state, lua_workshop_add_callback);
    lua_setfield(state, -2, "AddCallback");
    return 1;
}

void workshop_register_compat(lua_State* state) {
    lua_register(state, "RegisterMod", lua_workshop_register_mod);
    lua_createtable(state, 0, 2);
    lua_pushinteger(state, kWorkshopPostUpdate);
    lua_setfield(state, -2, "MC_POST_UPDATE");
    lua_pushinteger(state, kWorkshopPostNewRoom);
    lua_setfield(state, -2, "MC_POST_NEW_ROOM");
    lua_setglobal(state, "ModCallbacks");
}

bool workshop_valid_id(const char* id, size_t len) {
    if (!id || !len || len > 40) return false;
    for (size_t i = 0; i < len; ++i) {
        const char c = id[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

// Give each mod private copies of mutable global tables. The values are
// still the same Lua functions, but table mutations in one mod do not leak to
// later mods through shared standard-library/API tables.
void workshop_copy_env_value(lua_State* state, int environment_index, const char* name) {
    lua_getglobal(state, name);
    if (!lua_istable(state, -1)) {
        lua_setfield(state, environment_index, name);
        return;
    }
    lua_newtable(state);
    const int source_index = lua_gettop(state) - 1;
    const int copy_index = lua_gettop(state);
    lua_pushnil(state);
    while (lua_next(state, source_index)) {
        lua_pushvalue(state, -2);
        lua_pushvalue(state, -2);
        lua_settable(state, copy_index);
        lua_pop(state, 1);
    }
    lua_setfield(state, environment_index, name);
    lua_pop(state, 1);
}

int lua_workshop_require(lua_State* state) {
    if (!g_workshop_loading)
        return luaL_error(state, "require is supported only while loading a mod");
    size_t module_len = 0;
    const char* module = luaL_checklstring(state, 1, &module_len);
    if (!module_len || module_len > 128)
        return luaL_error(state, "require module name must contain 1..128 bytes");

    char relative[160] = {};
    size_t out = 0;
    bool segment_has_char = false;
    for (size_t i = 0; i < module_len; ++i) {
        const char c = module[i];
        if (c == '.') {
            if (!segment_has_char || out + 1 >= sizeof(relative))
                return luaL_error(state, "invalid require module path");
            relative[out++] = '/';
            segment_has_char = false;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-') {
            if (out + 1 >= sizeof(relative))
                return luaL_error(state, "invalid require module path");
            relative[out++] = c;
            segment_has_char = true;
        } else {
            return luaL_error(state, "require accepts dotted module names only");
        }
    }
    if (!segment_has_char)
        return luaL_error(state, "invalid require module path");
    relative[out] = 0;

    // The per-mod cache lives in that mod's private Lua environment.
    lua_pushvalue(state, lua_upvalueindex(1));
    const int environment_index = lua_gettop(state);
    lua_getfield(state, environment_index, "__workshop_loaded_modules");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushvalue(state, -1);
        lua_setfield(state, environment_index, "__workshop_loaded_modules");
    }
    const int cache_index = lua_gettop(state);
    lua_pushvalue(state, 1);
    lua_gettable(state, cache_index);
    if (!lua_isnil(state, -1))
        return 1;
    lua_pop(state, 1);

    if (g_workshop_module_count >= kWorkshopMaxModuleFiles)
        return luaL_error(state, "Workshop mod exceeded 16 Lua modules");
    char path[256] = {};
    const int path_len = std::snprintf(path, sizeof(path),
        "sdmc:/switch/TheModdingOfIsaac/mods/%s/%s.lua",
        g_workshop_current_mod_id, relative);
    if (path_len <= 0 || path_len >= static_cast<int>(sizeof(path)))
        return luaL_error(state, "require module path is too long");
    FILE* file = SaltySDCore_fopen(path, "rb");
    if (!file)
        return luaL_error(state, "required Lua module was not found in this mod");
    SaltySDCore_fseek(file, 0, SEEK_END);
    const int64_t length = SaltySDCore_ftell(file);
    SaltySDCore_fseek(file, 0, SEEK_SET);
    if (length <= 0 || length > static_cast<int64_t>(kWorkshopMaxModuleLua) ||
        static_cast<size_t>(length) > kWorkshopMaxTotalLua - g_workshop_total_lua_bytes) {
        SaltySDCore_fclose(file);
        return luaL_error(state, "required Lua module exceeds size limits");
    }
    char* source = static_cast<char*>(std::malloc(static_cast<size_t>(length)));
    if (!source) {
        SaltySDCore_fclose(file);
        return luaL_error(state, "insufficient memory for required Lua module");
    }
    const size_t read = SaltySDCore_fread(source, 1, static_cast<size_t>(length), file);
    SaltySDCore_fclose(file);
    if (read != static_cast<size_t>(length)) {
        std::free(source);
        return luaL_error(state, "required Lua module read was incomplete");
    }
    ++g_workshop_module_count;
    g_workshop_total_lua_bytes += read;

    // Mark before execution so recursive requires terminate; clear it on error.
    lua_pushvalue(state, 1);
    lua_pushboolean(state, 1);
    lua_settable(state, cache_index);
    int status = luaL_loadbuffer(state, source, read, path);
    std::free(source);
    if (status != 0) {
        lua_pushvalue(state, 1);
        lua_pushnil(state);
        lua_settable(state, cache_index);
        return lua_error(state);
    }
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_setfenv(state, -2);
    status = lua_pcall(state, 0, 1, 0);
    if (status != 0) {
        lua_pushvalue(state, 1);
        lua_pushnil(state);
        lua_settable(state, cache_index);
        return lua_error(state);
    }
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        lua_pushboolean(state, 1);
    }
    lua_pushvalue(state, 1);
    lua_pushvalue(state, -2);
    lua_settable(state, cache_index);
    return 1;
}

void workshop_load_mod(lua_State* state, const char* id) {
    char path[180] = {};
    const int n = std::snprintf(
        path, sizeof(path),
        "sdmc:/switch/TheModdingOfIsaac/mods/%s/main.lua", id);
    if (n <= 0 || n >= static_cast<int>(sizeof(path))) {
        ++g_workshop.errors;
        return;
    }
    FILE* file = SaltySDCore_fopen(path, "rb");
    if (!file) {
        trace("Workshop: main.lua missing (see mods.txt)");
        ++g_workshop.errors;
        return;
    }
    SaltySDCore_fseek(file, 0, SEEK_END);
    const int64_t length = SaltySDCore_ftell(file);
    SaltySDCore_fseek(file, 0, SEEK_SET);
    if (length <= 0 || length > static_cast<int64_t>(kWorkshopMaxMainLua)) {
        SaltySDCore_fclose(file);
        trace("Workshop: invalid main.lua size (max 64 KiB)");
        ++g_workshop.errors;
        return;
    }
    char* source = static_cast<char*>(std::malloc(static_cast<size_t>(length)));
    if (!source) {
        SaltySDCore_fclose(file);
        trace("Workshop: insufficient memory for Lua source");
        ++g_workshop.errors;
        return;
    }
    const size_t read = SaltySDCore_fread(source, 1, static_cast<size_t>(length), file);
    SaltySDCore_fclose(file);
    if (read != static_cast<size_t>(length)) {
        std::free(source);
        trace("Workshop: main.lua read was incomplete");
        ++g_workshop.errors;
        return;
    }

    const size_t callbacks_before = g_workshop_callback_count;
    std::memcpy(g_workshop_current_mod_id, id, std::strlen(id) + 1);
    g_workshop_module_count = 0;
    g_workshop_total_lua_bytes = read;
    int status = luaL_loadbuffer(state, source, read, path);
    std::free(source);
    if (status == 0) {
        // Each mod has its own deliberately small global namespace:
        // no io, os, debug, package, require, loadfile or dofile.
        lua_newtable(state); // [chunk, env]
        lua_pushvalue(state, -1);
        lua_setfield(state, -2, "_G");
        const int environment_index = lua_gettop(state);
        const char* allowed[] = {
            "_VERSION", "assert", "error", "ipairs", "pairs", "next", "type",
            "tonumber", "tostring", "select", "unpack", "pcall", "xpcall",
            "math", "string", "table", "coroutine",
            "log", "RegisterMod", "ModCallbacks", "IsaacBridge", "Isaac"
        };
        for (size_t i = 0; i < sizeof(allowed)/sizeof(allowed[0]); ++i)
            workshop_copy_env_value(state, environment_index, allowed[i]);
        // Mod-local require reads dotted module names only during startup,
        // when SaltySD file IPC is available.
        lua_newtable(state);
        lua_setfield(state, environment_index, "__workshop_loaded_modules");
        lua_pushvalue(state, environment_index);
        lua_pushcclosure(state, lua_workshop_require, 1);
        lua_setfield(state, environment_index, "require");
        lua_setfenv(state, -2); // pops env
        g_workshop_loading = true;
        lua_sethook(state, lua_instruction_limit, LUA_MASKCOUNT, kLuaInstructionBudget);
        status = lua_pcall(state, 0, 0, 0);
        lua_sethook(state, nullptr, 0, 0);
        g_workshop_loading = false;
    }
    g_workshop_current_mod_id[0] = 0;
    g_workshop_module_count = 0;
    g_workshop_total_lua_bytes = 0;
    if (status != 0) {
        const char* error = lua_tostring(state, -1);
        trace(error ? error : "Workshop: Lua load error");
        lua_pop(state, 1);
        workshop_release_from(state, callbacks_before);
        ++g_workshop.errors;
        return;
    }
    ++g_workshop.loaded;
    char notice[96] = {};
    std::snprintf(notice, sizeof(notice), "Workshop: loaded mod %s", id);
    trace(notice);
}

void workshop_load_manifest(lua_State* state) {
    static const char* manifest =
        "sdmc:/switch/TheModdingOfIsaac/mods.txt";
    FILE* file = SaltySDCore_fopen(manifest, "rb");
    if (!file) {
        trace("Workshop: no mods.txt (legacy bridge unchanged)");
        return;
    }
    SaltySDCore_fseek(file, 0, SEEK_END);
    const int64_t len = SaltySDCore_ftell(file);
    SaltySDCore_fseek(file, 0, SEEK_SET);
    if (len < 0 || len > 1024) {
        SaltySDCore_fclose(file);
        ++g_workshop.errors;
        trace("Workshop: mods.txt too large");
        return;
    }
    char buffer[1026] = {};
    const size_t got = SaltySDCore_fread(buffer, 1, static_cast<size_t>(len), file);
    SaltySDCore_fclose(file);
    if (got != static_cast<size_t>(len)) {
        ++g_workshop.errors;
        trace("Workshop: failed to read mods.txt");
        return;
    }
    size_t count = 0;
    char seen[kWorkshopMaxMods][41] = {};
    char* cursor = buffer;
    for (;;) {
        char* end = cursor;
        while (*end && *end != '\n' && *end != '\r') ++end;
        const char separator = *end;
        const bool done = (separator == '\0');
        if (!done) *end = '\0';
        const size_t n = std::strlen(cursor);
        if (n && cursor[0] != '#') {
            if (!workshop_valid_id(cursor, n)) {
                ++g_workshop.errors;
                trace("Workshop: rejected invalid mod id in mods.txt");
            } else if (count >= kWorkshopMaxMods) {
                ++g_workshop.errors;
                trace("Workshop: max 4 enabled mods");
            } else {
                bool duplicate = false;
                for (size_t i = 0; i < count; ++i)
                    if (std::strcmp(seen[i], cursor) == 0) duplicate = true;
                if (duplicate) {
                    ++g_workshop.errors;
                    trace("Workshop: duplicate mod id");
                } else {
                    std::memcpy(seen[count], cursor, n + 1);
                    ++count;
                    workshop_load_mod(state, cursor);
                }
            }
        }
        if (done) break;
        cursor = end + 1;
        // Ignore the second byte of a Windows CRLF separator.
        if (separator == '\r' && cursor[0] == '\n')
            ++cursor;
        if (!*cursor) break;
    }
    char info[96] = {};
    std::snprintf(info, sizeof(info), "Workshop: %u mods, %u callbacks, %u errors",
                  static_cast<unsigned>(g_workshop.loaded),
                  static_cast<unsigned>(g_workshop_callback_count),
                  static_cast<unsigned>(g_workshop.errors));
    trace(info);
}

bool workshop_room_changed() {
    uintptr_t token = 0;
    if (!read_current_room_token(token)) {
        g_workshop_room_seen = false;
        return false;
    }
    const bool changed = !g_workshop_room_seen || g_workshop_room_token != token;
    g_workshop_room_token = token;
    g_workshop_room_seen = true;
    return changed;
}

void workshop_dispatch_post_update(lua_State* state) {
    const bool room_changed = workshop_room_changed();
    for (size_t i = 0; i < g_workshop_callback_count; ++i) {
        WorkshopCallback& cb = g_workshop_callbacks[i];
        if (cb.disabled || (cb.callback_id == kWorkshopPostNewRoom && !room_changed)) continue;
        lua_rawgeti(state, LUA_REGISTRYINDEX, cb.function_ref);
        // Workshop mod callbacks are methods on their RegisterMod table.
        // Specialist's source registers Epic:OnGameUpdate / Epic:OnNewRoom,
        // whose first parameter is the mod table ("self").
        lua_rawgeti(state, LUA_REGISTRYINDEX, cb.mod_ref);
        lua_sethook(state, lua_instruction_limit, LUA_MASKCOUNT, kLuaInstructionBudget);
        const int status = lua_pcall(state, 1, 0, 0);
        lua_sethook(state, nullptr, 0, 0);
        if (status != 0) {
            const char* error = lua_tostring(state, -1);
            trace(error ? error : "Workshop: callback error");
            lua_pop(state, 1);
            cb.disabled = true;
            ++g_workshop.callback_errors;
        } else {
            ++g_workshop.callback_success;
            if (cb.callback_id == kWorkshopPostNewRoom)
                ++g_workshop.room_callback_success;
        }
    }
}
