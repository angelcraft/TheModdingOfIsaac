-- Host entry point. The Switch NRO loads the same test.lua directly.
local source = debug.getinfo(1, "S").source
if source:sub(1, 1) == "@" then source = source:sub(2) end
source = source:gsub("\\", "/")
local test_path = source:gsub("tests/run%.lua$", "test.lua")
assert(test_path ~= source, "cannot resolve shared test.lua from tests/run.lua")
dofile(test_path)
