-- First half of the two-mod isolation smoke test. Changes here must not leak
-- into the next mod's globals, standard-library tables or API enum tables.
local mod = RegisterMod("Workshop Loader Demo", 1)
local item_config = Isaac.GetItemConfig()
local item_four = item_config:GetCollectible(4)
local item_502 = item_config:GetCollectible(502)
if item_four and item_four.ID == 4 and item_four.Quality == 4 and
   item_502 and item_502.ID == 502 and item_502.Quality == 1 then
    log("Workshop ItemConfig: PASS (id 4 quality 4; id 502 quality 1)")
else
    log("Workshop ItemConfig: FAIL (metadata missing or values differ)")
end
local function has_quality_four(id)
    local item = Isaac.GetItemConfig():GetCollectible(id)
    return item ~= nil and item.Quality == 4
end
if has_quality_four(4) and not has_quality_four(502) and not has_quality_four(999) then
    log("Workshop ItemConfig quality filter: PASS (4 accepted; 502 and unknown rejected)")
else
    log("Workshop ItemConfig quality filter: FAIL")
end
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
