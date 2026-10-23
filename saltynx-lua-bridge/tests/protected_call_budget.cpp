#include <cstdio>
#include <cstring>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

constexpr int kLuaInstructionBudget = 1000;
#include "../source/lua_budget.h"

static bool run_case(lua_State* state, const char* name, const char* source,
                     bool should_exhaust) {
    lua_settop(state, 0);
    int status = luaL_loadstring(state, source);
    if (status != 0) {
        std::fprintf(stderr, "%s: compile error: %s\n", name, lua_tostring(state, -1));
        return false;
    }
    lua_begin_instruction_budget(state);
    status = lua_end_instruction_budget(state, lua_pcall(state, 0, 0, 0));
    const bool correct = should_exhaust
        ? status != 0 && g_lua_budget_exhausted &&
          lua_isstring(state, -1) &&
          std::strstr(lua_tostring(state, -1), "instruction budget exceeded")
        : status == 0 && !g_lua_budget_exhausted;
    if (!correct) {
        std::fprintf(stderr, "%s: status=%d exhausted=%d error=%s\n",
                     name, status, g_lua_budget_exhausted,
                     lua_tostring(state, -1) ? lua_tostring(state, -1) : "(none)");
        return false;
    }
    if (lua_gethook(state) != nullptr) {
        std::fprintf(stderr, "%s: outer hook was not cleared\n", name);
        return false;
    }
    lua_settop(state, 0);
    std::printf("Instruction budget: PASS (%s)\n", name);
    return true;
}

int main() {
    lua_State* state = luaL_newstate();
    if (!state) return 2;
    luaL_openlibs(state);
    lua_register(state, "pcall", lua_guarded_pcall);
    lua_register(state, "xpcall", lua_guarded_xpcall);
    struct Case { const char* name; const char* source; bool exhaust; };
    const Case cases[] = {
        {"pcall results and ordinary errors",
         "local ok,a,b,c=pcall(function(x) return x,nil,7 end,5); "
         "assert(ok and a==5 and b==nil and c==7); "
         "local ok,e=pcall(function() error({value=3}) end); "
         "assert(not ok and e.value==3)", false},
        {"xpcall results and ordinary handler",
         "local ok,a,b,c=xpcall(function() return 5,nil,7 end,function(e) return e end); "
         "assert(ok and a==5 and b==nil and c==7); "
         "local ok,e=xpcall(function() error({value=3}) end,function(e) return e.value+1 end); "
         "assert(not ok and e==4); "
         "local ok,e=xpcall(function() error('original') end,function() error('handler') end); "
         "assert(not ok and e=='error in error handling')", false},
        {"repeated protected timeout",
         "while true do pcall(function() while true do end end) end", true},
        {"nested protected timeout",
         "pcall(function() xpcall(function() while true do end end,function() return 'caught' end) end)", true},
        {"xpcall skips handler after timeout",
         "xpcall(function() while true do end end,function() while true do end end)", true},
        {"ordinary-error handler remains bounded",
         "xpcall(function() error('ordinary') end,function() while true do end end)", true},
        {"coroutine cannot hide exhaustion",
         "local co=coroutine.create(function() while true do end end); "
         "local ok=coroutine.resume(co); return ok", true},
        {"next dispatch recovers",
         "local ok,value=pcall(function() return 42 end); assert(ok and value==42)", false},
    };
    for (const auto& test : cases) {
        if (!run_case(state, test.name, test.source, test.exhaust)) {
            lua_close(state);
            return 1;
        }
    }
    lua_close(state);
    return 0;
}
