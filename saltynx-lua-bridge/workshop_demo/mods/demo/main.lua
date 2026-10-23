-- Safe Workshop compatibility smoke test: no native game mutation.
local mod = RegisterMod("Workshop Loader Demo", 1)
local calls = 0

function mod:PostUpdate()
    calls = calls + 1
    if calls == 1 then
        log("Workshop demo: first MC_POST_UPDATE callback")
    elseif calls == 600 then
        log("Workshop demo: 600 callbacks executed")
    end
end

mod:AddCallback(ModCallbacks.MC_POST_UPDATE, mod.PostUpdate)
