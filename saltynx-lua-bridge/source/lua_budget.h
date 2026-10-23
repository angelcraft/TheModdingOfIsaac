#pragma once

// One Lua state owner: reset this flag only at an outer mod load/update.
// Nested protected calls must never turn a watchdog failure into success.
bool g_lua_budget_exhausted = false;

void lua_instruction_limit(lua_State* state, lua_Debug*) {
    g_lua_budget_exhausted = true;
    luaL_error(state, "IsaacLuaBridge: Lua instruction budget exceeded");
}

void lua_begin_instruction_budget(lua_State* state) {
    g_lua_budget_exhausted = false;
    lua_sethook(state, lua_instruction_limit, LUA_MASKCOUNT, kLuaInstructionBudget);
}

int lua_end_instruction_budget(lua_State* state, int status) {
    lua_sethook(state, nullptr, 0, 0);
    // coroutine.resume can catch a hook error and let the outer chunk return.
    // Its caller still exceeded this dispatch's budget.
    if (g_lua_budget_exhausted && status == 0) {
        lua_pushliteral(state, "IsaacLuaBridge: Lua instruction budget exceeded");
        return LUA_ERRRUN;
    }
    return status;
}

int lua_rethrow_instruction_budget(lua_State* state) {
    return luaL_error(state, "IsaacLuaBridge: Lua instruction budget exceeded");
}

int lua_guarded_pcall(lua_State* state) {
    if (g_lua_budget_exhausted) return lua_rethrow_instruction_budget(state);
    luaL_checkany(state, 1);
    const int status = lua_pcall(state, lua_gettop(state) - 1, LUA_MULTRET, 0);
    if (g_lua_budget_exhausted) return lua_rethrow_instruction_budget(state);
    lua_pushboolean(state, status == 0);
    lua_insert(state, 1);
    return lua_gettop(state);
}

int lua_guarded_xpcall(lua_State* state) {
    if (g_lua_budget_exhausted) return lua_rethrow_instruction_budget(state);
    luaL_checkany(state, 2);
    lua_settop(state, 2); // Lua 5.1 xpcall accepts no arguments for the function.
    lua_pushvalue(state, 1);
    // Never register a user error handler with lua_pcall's errfunc argument:
    // Lua 5.1 invokes it before unwinding a failing hook (allowhook is false).
    // Invoke an ordinary-error handler separately, with hooks enabled.
    int status = lua_pcall(state, 0, LUA_MULTRET, 0);
    lua_remove(state, 1); // [handler, results...] or [handler, error]
    if (g_lua_budget_exhausted) return lua_rethrow_instruction_budget(state);
    if (status != 0) {
        lua_pushvalue(state, 1);
        lua_insert(state, 2); // [handler, handler, error]
        status = lua_pcall(state, 1, 1, 0);
        if (g_lua_budget_exhausted) return lua_rethrow_instruction_budget(state);
        if (status != 0) {
            lua_pop(state, 1);
            lua_pushliteral(state, "error in error handling");
        }
        lua_pushboolean(state, 0);
        lua_replace(state, 1);
        return 2;
    }
    lua_pushboolean(state, 1);
    lua_replace(state, 1);
    return lua_gettop(state);
}
