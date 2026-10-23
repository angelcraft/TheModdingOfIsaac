-- Use the same verified ItemConfig metadata exposed to Workshop mods.
-- Missing or unknown IDs fail closed and never trigger the dance.
local item_config = Isaac.GetItemConfig()
local function has_quality_four(id)
    local item = item_config:GetCollectible(id)
    return item ~= nil and item.Quality == 4
end
log('Specialist filter: collectible quality 4')
local previous, last_error, active_item = nil, nil, nil
local music_error = nil
function on_update(frame)
    if frame % 3 ~= 0 then return end
    local room, reason = IsaacBridge.GetRoomCollectibles()
    local selected = nil
    if room and room.in_run then
        for _, id in ipairs(room.items) do
            if id > 0 and has_quality_four(id) then
                selected = id; break
            end
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
        log(should_dance and ('Specialist ON: pedestal ' .. tostring(selected)) or 'Specialist OFF: no qualifying pedestal')
        previous = should_dance
    end
    active_item = selected
end

