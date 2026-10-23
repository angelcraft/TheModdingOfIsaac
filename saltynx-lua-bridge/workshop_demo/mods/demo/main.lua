-- First half of the two-mod isolation smoke test. Changes here must not leak
-- into the next mod's globals, standard-library tables or API enum tables.
local mod = RegisterMod("Workshop Loader Demo", 1)
local post_update_id = ModCallbacks.MC_POST_UPDATE
local post_new_room_id = ModCallbacks.MC_POST_NEW_ROOM
local calls = 0

math.__tmi_isolation_probe = "demo"
string.__tmi_isolation_probe = "demo"
table.__tmi_isolation_probe = "demo"
coroutine.__tmi_isolation_probe = "demo"
ModCallbacks.MC_POST_UPDATE = 999
IsaacBridge.__tmi_isolation_probe = "demo"
Isaac.__tmi_isolation_probe = "demo"
local config_probe = Isaac.GetItemConfig()
config_probe.GetCollectible = function() return nil end
workshop_global_probe = "demo"

local function post_update(self)
    calls = calls + 1
    if calls == 1 then
        if self == mod then
            log("Workshop demo: callback mod argument OK")
        else
            log("Workshop demo: callback mod argument mismatch")
        end
    elseif calls == 600 then
        log("Workshop demo: 600 callbacks executed")
    end
end

local function new_room(self)
    if self == mod then
        log("Workshop demo: MC_POST_NEW_ROOM callback with mod argument")
    else
        log("Workshop demo: room callback mod argument mismatch")
    end
end

mod:AddCallback(post_update_id, post_update)
mod:AddCallback(post_new_room_id, new_room)
