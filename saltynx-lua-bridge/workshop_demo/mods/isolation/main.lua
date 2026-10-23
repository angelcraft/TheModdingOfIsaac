-- Second half of the two-mod isolation smoke test. A clean result means
-- mod globals and mutable API/library tables were copied for this environment.
local mod = RegisterMod("Workshop Isolation Check", 1)
local failures = {}
local function expect(name, ok)
    if not ok then failures[#failures + 1] = name end
end

expect("global namespace", workshop_global_probe == nil)
expect("math table", math.__tmi_isolation_probe == nil)
expect("string table", string.__tmi_isolation_probe == nil)
expect("table library", table.__tmi_isolation_probe == nil)
expect("coroutine library", coroutine.__tmi_isolation_probe == nil)
expect("ModCallbacks table", ModCallbacks.MC_POST_UPDATE == 1)
expect("IsaacBridge table", IsaacBridge.__tmi_isolation_probe == nil)
expect("Isaac API table", Isaac.__tmi_isolation_probe == nil)
local item_four = Isaac.GetItemConfig():GetCollectible(4)
expect("ItemConfig API wrapper isolation", item_four and item_four.Quality == 4)

if #failures == 0 then
    log("Workshop isolation: PASS (globals, libraries, API tables)")
else
    log("Workshop isolation: FAIL (" .. table.concat(failures, ", ") .. ")")
end

if #failures == 0 then
    local calls = 0
    local function post_update(self)
        calls = calls + 1
        if calls == 1 and self == mod then
            log("Workshop isolation: callback argument OK")
        end
    end
    local function new_room(self)
        if self == mod then
            log("Workshop isolation: MC_POST_NEW_ROOM callback")
        end
    end
    mod:AddCallback(ModCallbacks.MC_POST_UPDATE, post_update)
    mod:AddCallback(ModCallbacks.MC_POST_NEW_ROOM, new_room)
end
