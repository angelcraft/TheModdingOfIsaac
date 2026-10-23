-- Safe Workshop compatibility smoke test: no native game mutation.
local mod = RegisterMod("Workshop Loader Demo", 1)
local calls = 0

local function post_update()
    calls = calls + 1
    if calls == 1 then
        log("Workshop demo: first MC_POST_UPDATE callback")
    elseif calls == 600 then
        log("Workshop demo: 600 callbacks executed")
    end
end

local function new_room()
    log("Workshop demo: MC_POST_NEW_ROOM callback")
end

mod:AddCallback(ModCallbacks.MC_POST_UPDATE, post_update)
mod:AddCallback(ModCallbacks.MC_POST_NEW_ROOM, new_room)
