-- Native read candidate: identify collectible pedestals before collection.
local previous = ""
function on_update(frame)
    if frame % 15 ~= 0 then return end
    local room, reason = IsaacBridge.GetRoomCollectibles()
    if not room then
        if previous ~= reason then log("room read: " .. tostring(reason)); previous = reason end
        return
    end
    local signature = tostring(room.manager_state) .. ":" .. table.concat(room.items, ",")
    if signature ~= previous then
        log("room collectibles: state=" .. room.manager_state .. " players=" ..
            room.player_count .. " ids=" .. table.concat(room.items, ","))
        previous = signature
    end
end
