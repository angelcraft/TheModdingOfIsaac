-- Tiny script for the future standalone Switch Lua NRO milestone.
-- It deliberately uses no Isaac globals or game APIs.
local message = "Lua OK"
assert(type(message) == "string")
assert(string.upper(message) == "LUA OK")
print(message)
