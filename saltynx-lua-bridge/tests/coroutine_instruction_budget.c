#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static void instruction_budget(lua_State *state, lua_Debug *debug) {
    (void)debug;
    luaL_error(state, "IsaacLuaBridge: Lua instruction budget exceeded");
}

static int run(lua_State *state, const char *source, const char *label) {
    const int status = luaL_dostring(state, source);
    if (status == 0) return 0;
    fprintf(stderr, "%s failed: %s\n", label,
            lua_tostring(state, -1) ? lua_tostring(state, -1) : "(non-string error)");
    lua_pop(state, 1);
    return 1;
}

int main(void) {
    lua_State *state = luaL_newstate();
    if (!state) {
        fputs("luaL_newstate failed\n", stderr);
        return 1;
    }
    luaL_openlibs(state);

    // Mod chunks run with this count hook. Lua 5.1 must copy it to threads
    // created while the chunk is running, and clearing the main-state hook
    // afterward must not disable the coroutine's own copy.
    lua_sethook(state, instruction_budget, LUA_MASKCOUNT, 64);
    int failed = run(state,
        "created = coroutine.create(function() while true do end end)",
        "coroutine.create setup");
    lua_sethook(state, NULL, 0, 0);
    if (!failed) {
        failed = run(state,
            "local ok, err = coroutine.resume(created); "
            "assert(not ok, 'resume unexpectedly completed'); "
            "assert(type(err) == 'string' and "
            "string.find(err, 'instruction budget exceeded', 1, true), "
            "'coroutine.create thread did not retain instruction hook')",
            "coroutine.create budget");
    }

    lua_sethook(state, instruction_budget, LUA_MASKCOUNT, 64);
    if (!failed) {
        failed = run(state,
            "wrapped = coroutine.wrap(function() while true do end end)",
            "coroutine.wrap setup");
    }
    lua_sethook(state, NULL, 0, 0);
    if (!failed) {
        failed = run(state,
            "local ok, err = pcall(wrapped); "
            "assert(not ok, 'wrapped coroutine unexpectedly completed'); "
            "assert(type(err) == 'string' and "
            "string.find(err, 'instruction budget exceeded', 1, true), "
            "'coroutine.wrap thread did not retain instruction hook')",
            "coroutine.wrap budget");
    }

    lua_close(state);
    if (failed) return 1;
    puts("Lua 5.1 coroutine instruction budget: PASS (create, wrap)");
    return 0;
}
