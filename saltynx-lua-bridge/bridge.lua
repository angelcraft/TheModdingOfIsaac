-- The Specialist decision lives in Lua. Item 502 was observed on this console.
local TARGET_ITEM = 502
local previous, last_error = nil, nil
function on_update(frame)
    if frame % 3 ~= 0 then return end
    local room, reason = IsaacBridge.GetRoomCollectibles()
    local should_dance = false
    if room and room.in_run then
        for _, id in ipairs(room.items) do
            if id == TARGET_ITEM then should_dance = true; break end
        end
    end
    local ok, native_reason = IsaacBridge.SetSpecialistDance(should_dance)
    if not ok then
        local message = native_reason or reason or 'unknown'
        if last_error ~= message then log('Specialist blocked: ' .. message) end
        last_error = message
        return
    end
    last_error = nil
    if previous ~= should_dance then
        log(should_dance and 'Specialist ON: pedestal 502' or 'Specialist OFF: pedestal absent')
        previous = should_dance
    end
end

