-- Specialist logic sketch using a MOCK host API, not the Isaac Lua API.
-- Host contract:
--   host:on(event, callback)
--   host:emit_action(name, payload)
--   ctx.entities = array of {type, variant, subtype, entity_id}
--   ctx.qualifying_items = set keyed by numeric collectible subtype
local M = {}

function M.install(host)
    local active_entity_id = nil
    local seen = {}

    local function stop_if_active(reason)
        if active_entity_id ~= nil then
            host:emit_action("specialist_stop", {
                entity_id = active_entity_id,
                reason = reason
            })
            active_entity_id = nil
        end
    end

    host:on("room_update", function(ctx)
        local candidate = nil
        for i = 1, #ctx.entities do
            local e = ctx.entities[i]
            if e.type == "pickup"
                and e.variant == "collectible"
                and e.subtype ~= 0
                and ctx.qualifying_items[e.subtype]
                and not seen[e.entity_id] then
                candidate = e
                break
            end
        end

        if active_entity_id ~= nil then
            local still_present = false
            for i = 1, #ctx.entities do
                if ctx.entities[i].entity_id == active_entity_id then
                    still_present = true
                    break
                end
            end
            if not still_present then stop_if_active("pedestal_removed_or_picked_up") end
        end

        if active_entity_id == nil and candidate ~= nil then
            active_entity_id = candidate.entity_id
            seen[candidate.entity_id] = true
            host:emit_action("specialist_start", {
                entity_id = candidate.entity_id,
                subtype = candidate.subtype,
                timing = "before_pickup"
            })
        end
    end)

    host:on("new_room", function(_ctx)
        stop_if_active("new_room")
        seen = {}
    end)
end

return M
