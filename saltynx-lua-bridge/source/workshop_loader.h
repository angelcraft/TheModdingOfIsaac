#pragma once

// Optional first-step Workshop Lua loader. Runs SD file IPC ONLY during
// SaltyNX plugin startup. Lua callbacks run later, on IsaacUpdate's owner.
// No PC entity/music APIs are simulated; unsupported calls fail visibly.
// Do not run untrusted mods: Lua scripts are native-game-adjacent code.

constexpr size_t kWorkshopMaxMods = 4;
constexpr size_t kWorkshopMaxCallbacks = 32;
constexpr size_t kWorkshopMaxMainLua = 64 * 1024;
constexpr int kWorkshopPostUpdate = 1;

struct WorkshopCallback {
    int mod_ref;
    int function_ref;
    bool disabled;
};
WorkshopCallback g_workshop_callbacks[kWorkshopMaxCallbacks] = {};
size_t g_workshop_callback_count = 0;
u32 g_workshop_loaded = 0;
u32 g_workshop_errors = 0;
u32 g_workshop_callback_success = 0;
u32 g_workshop_callback_errors = 0;
bool g_workshop_loading = false;

void workshop_release_from(lua_State* state, size_t first) {
    while (g_workshop_callback_count > first) {
        WorkshopCallback& cb = g_workshop_callbacks[--g_workshop_callback_count];
        luaL_unref(state, LUA_REGISTRYINDEX, cb.mod_ref);
        luaL_unref(state, LUA_REGISTRYINDEX, cb.function_ref);
        cb = {};
    }
}

int lua_workshop_add_callback(lua_State* state) {
    if (!g_workshop_loading) return luaL_error(state, "AddCallback only supported while loading a mod");
    if (!lua_istable(state, 1) || !lua_isnumber(state, 2) ||
        !lua_isfunction(state, 3))
        return luaL_error(state, "AddCallback(mod, callback_id, function) required");
    if (lua_tointeger(state, 2) != kWorkshopPostUpdate)
        return luaL_error(state, "unsupported PC callback (only MC_POST_UPDATE available)");
    if (g_workshop_callback_count == kWorkshopMaxCallbacks)
        return luaL_error(state, "Workshop callback limit reached");
    WorkshopCallback& cb = g_workshop_callbacks[g_workshop_callback_count];
    lua_pushvalue(state, 1);
    cb.mod_ref = luaL_ref(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 3);
    cb.function_ref = luaL_ref(state, LUA_REGISTRYINDEX);
    cb.disabled = false;
    ++g_workshop_callback_count;
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
    lua_createtable(state, 0, 1);
    lua_pushinteger(state, kWorkshopPostUpdate);
    lua_setfield(state, -2, "MC_POST_UPDATE");
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

void workshop_load_mod(lua_State* state, const char* id) {
    char path[180] = {};
    const int n = std::snprintf(
        path, sizeof(path),
        "sdmc:/switch/TheModdingOfIsaac/mods/%s/main.lua", id);
    if (n <= 0 || n >= static_cast<int>(sizeof(path))) {
        ++g_workshop_errors;
        return;
    }
    FILE* file = SaltySDCore_fopen(path, "rb");
    if (!file) {
        trace("Workshop: main.lua missing (see mods.txt)");
        ++g_workshop_errors;
        return;
    }
    SaltySDCore_fseek(file, 0, SEEK_END);
    const int64_t length = SaltySDCore_ftell(file);
    SaltySDCore_fseek(file, 0, SEEK_SET);
    if (length <= 0 || length > static_cast<int64_t>(kWorkshopMaxMainLua)) {
        SaltySDCore_fclose(file);
        trace("Workshop: invalid main.lua size (max 64 KiB)");
        ++g_workshop_errors;
        return;
    }
    char* source = static_cast<char*>(std::malloc(static_cast<size_t>(length)));
    if (!source) {
        SaltySDCore_fclose(file);
        trace("Workshop: insufficient memory for Lua source");
        ++g_workshop_errors;
        return;
    }
    const size_t read = SaltySDCore_fread(source, 1, static_cast<size_t>(length), file);
    SaltySDCore_fclose(file);
    if (read != static_cast<size_t>(length)) {
        std::free(source);
        trace("Workshop: main.lua read was incomplete");
        ++g_workshop_errors;
        return;
    }

    const size_t callbacks_before = g_workshop_callback_count;
    int status = luaL_loadbuffer(state, source, read, path);
    std::free(source);
    if (status == 0) {
        // One global namespace per mod, with the restricted host globals as a
        // fallback. Avoid accidental overwrites across different mods.
        lua_newtable(state); // [chunk, env]
        lua_pushvalue(state, -1);
        lua_setfield(state, -2, "_G");
        lua_newtable(state); // [chunk, env, metatable]
        lua_pushvalue(state, LUA_GLOBALSINDEX);
        lua_setfield(state, -2, "__index");
        lua_setmetatable(state, -2);
        lua_setfenv(state, -2); // pops env
        g_workshop_loading = true;
        lua_sethook(state, lua_instruction_limit, LUA_MASKCOUNT, kLuaInstructionBudget);
        status = lua_pcall(state, 0, 0, 0);
        lua_sethook(state, nullptr, 0, 0);
        g_workshop_loading = false;
    }
    if (status != 0) {
        const char* error = lua_tostring(state, -1);
        trace(error ? error : "Workshop: Lua load error");
        lua_pop(state, 1);
        workshop_release_from(state, callbacks_before);
        ++g_workshop_errors;
        return;
    }
    ++g_workshop_loaded;
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
        ++g_workshop_errors;
        trace("Workshop: mods.txt too large");
        return;
    }
    char buffer[1026] = {};
    const size_t got = SaltySDCore_fread(buffer, 1, static_cast<size_t>(len), file);
    SaltySDCore_fclose(file);
    if (got != static_cast<size_t>(len)) {
        ++g_workshop_errors;
        trace("Workshop: failed to read mods.txt");
        return;
    }
    size_t count = 0;
    char seen[kWorkshopMaxMods][41] = {};
    char* cursor = buffer;
    for (;;) {
        char* end = cursor;
        while (*end && *end != '\n' && *end != '\r') ++end;
        const bool done = (*end == '\0');
        if (!done) { *end = '\0'; }
        const size_t n = std::strlen(cursor);
        if (n && cursor[0] != '#') {
            if (!workshop_valid_id(cursor, n)) {
                ++g_workshop_errors;
                trace("Workshop: rejected invalid mod id in mods.txt");
            } else if (count >= kWorkshopMaxMods) {
                ++g_workshop_errors;
                trace("Workshop: max 4 enabled mods");
            } else {
                bool duplicate = false;
                for (size_t i = 0; i < count; ++i)
                    if (std::strcmp(seen[i], cursor) == 0) duplicate = true;
                if (duplicate) {
                    ++g_workshop_errors;
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
        // Treat CRLF as one separator. Standalone CR also supported.
        if (cursor[0] == '\n' && end[0] == '\0' && cursor > buffer &&
            cursor[-1] == '\0') ++cursor;
        if (!*cursor) break;
    }
    char info[96] = {};
    std::snprintf(info, sizeof(info), "Workshop: %u mods, %u callbacks, %u errors",
                  static_cast<unsigned>(g_workshop_loaded),
                  static_cast<unsigned>(g_workshop_callback_count),
                  static_cast<unsigned>(g_workshop_errors));
    trace(info);
}

void workshop_dispatch_post_update(lua_State* state) {
    for (size_t i = 0; i < g_workshop_callback_count; ++i) {
        WorkshopCallback& cb = g_workshop_callbacks[i];
        if (cb.disabled) continue;
        lua_rawgeti(state, LUA_REGISTRYINDEX, cb.function_ref);
        lua_rawgeti(state, LUA_REGISTRYINDEX, cb.mod_ref);
        lua_sethook(state, lua_instruction_limit, LUA_MASKCOUNT, kLuaInstructionBudget);
        const int status = lua_pcall(state, 1, 0, 0);
        lua_sethook(state, nullptr, 0, 0);
        if (status != 0) {
            const char* error = lua_tostring(state, -1);
            trace(error ? error : "Workshop: callback error");
            lua_pop(state, 1);
            cb.disabled = true;
            ++g_workshop_callback_errors;
        } else {
            ++g_workshop_callback_success;
        }
    }
}
