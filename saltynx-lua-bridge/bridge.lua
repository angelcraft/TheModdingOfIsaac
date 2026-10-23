-- Quality 4 IDs generated from the exact Switch ItemConfig metadata.
local QUALITY_4 = {
    [4]=true,[12]=true,[52]=true,[105]=true,[108]=true,[114]=true,[118]=true,
    [149]=true,[168]=true,[169]=true,[182]=true,[223]=true,[232]=true,
    [245]=true,[292]=true,[313]=true,[331]=true,[360]=true,[395]=true,
    [415]=true,[441]=true,[477]=true,[489]=true,[531]=true,[550]=true,
    [551]=true,[552]=true,[581]=true,[625]=true,[628]=true,[636]=true,
    [643]=true,[664]=true,[678]=true,[689]=true,[691]=true,[698]=true,
    [706]=true,[711]=true,[723]=true
}
local previous, last_error, active_item = nil, nil, nil
local music_error = nil
function on_update(frame)
    if frame % 3 ~= 0 then return end
    local room, reason = IsaacBridge.GetRoomCollectibles()
    local selected = nil
    if room and room.in_run then
        for _, id in ipairs(room.items) do
            if QUALITY_4[id] then selected = id; break end
        end
    end
    local should_dance = selected ~= nil
    local ok, native_reason = IsaacBridge.SetSpecialistDance(should_dance, selected or 0)
    if not ok then
        local message = native_reason or reason or 'unknown'
        if last_error ~= message then log('Specialist blocked: ' .. message) end
        last_error = message
        return
    end
    last_error = nil
    local music_ok, music_reason = IsaacBridge.SetSpecialistMusic(should_dance)
    if not music_ok then
        local message = music_reason or 'music unavailable'
        if music_error ~= message then log('Specialist music: ' .. message) end
        music_error = message
    else
        music_error = nil
    end
    if previous ~= should_dance then
        log(should_dance and ('Specialist ON: quality 4 pedestal ' .. tostring(selected)) or 'Specialist OFF: no quality 4 pedestal')
        previous = should_dance
    end
    active_item = selected
end

