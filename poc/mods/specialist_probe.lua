-- Source-derived Specialist trigger model using a MOCK host API.
-- This stays Lua 5.1-compatible and does not call Isaac or read game memory.
--
-- Host contract:
--   host:on(event, callback)
--   host:emit_action(name, payload)
--   ctx.entities = array of normalized entity records:
--     {entity_id, type, variant, subtype, item_name, quality, position_x, position_y}
--   ctx.settings = {quality_threshold, ignore_quality, blind_bypass, custom_pog_list}
--   ctx.curse_of_blind = boolean
--   ctx.alt_path_treasure_room = boolean
--   ctx.alt_path_collectible_count = optional number
--
-- Entity values, item quality/name, position, and room flags must be supplied by
-- a separately verified adapter. This module does not implement that adapter.
local M = {}

local DEFAULT_POG_LIST = {
    ["Cricket's Head"] = 1,
    ["Magic Mushroom"] = 1,
    ["Sacred Heart"] = 1,
    ["Brimstone"] = 1
}

local function is_collectible_pedestal(entity)
    return entity ~= nil
        and entity.type == "pickup"
        and entity.variant == "collectible"
        and type(entity.subtype) == "number"
        and entity.subtype > 0
end

local function count_collectible_pedestals(entities)
    local count = 0
    for i = 1, #entities do
        if is_collectible_pedestal(entities[i]) then
            count = count + 1
        end
    end
    return count
end

local function alt_path_item_is_visible(entity, item_count)
    local x = entity.position_x
    local y = entity.position_y
    if type(x) ~= "number" or type(y) ~= "number" then
        return false
    end

    if item_count == 2 then
        return x <= 320 and y <= 280
    elseif item_count > 2 then
        return x <= 320 and not (y == 280 and x == 320)
    end

    return false
end

local function custom_item_key(entity)
    if entity.subtype == 120 then
        return "Odd Mushroom (Thin)"
    elseif entity.subtype == 121 then
        return "Odd Mushroom (Large)"
    end
    return entity.custom_key or entity.item_name
end

local function is_visible(entity, context, settings, item_count)
    if settings.blind_bypass == true then
        return true
    end
    if context.curse_of_blind == true then
        return false
    end
    if context.alt_path_treasure_room == true and item_count > 0 then
        return alt_path_item_is_visible(entity, item_count)
    end
    return true
end

local function is_qualifying(entity, context, item_count)
    if not is_collectible_pedestal(entity) or entity.entity_id == nil then
        return false
    end

    -- The original script skips an item-config entry whose quality is nil.
    if type(entity.quality) ~= "number" then
        return false
    end

    local settings = context.settings or {}
    if not is_visible(entity, context, settings, item_count) then
        return false
    end

    local key = custom_item_key(entity)
    local configured = settings.custom_pog_list or {}
    local override = nil
    if key ~= nil then
        override = configured[key]
        if override == nil then
            override = DEFAULT_POG_LIST[key]
        end
    end

    -- Source behavior: explicit "boogie" wins; explicit "not boogie" vetoes
    -- quality; otherwise the threshold path only runs when IgnoreQuality=false.
    if override == 1 then
        return true
    elseif override == 2 then
        return false
    end

    local threshold = settings.quality_threshold
    if threshold == nil then threshold = 4 end
    return settings.ignore_quality ~= true and entity.quality >= threshold
end

function M.install(host)
    local active = nil
    local seen = {}

    local function stop_if_active(reason)
        if active ~= nil then
            host:emit_action("specialist_stop", {
                entity_id = active.entity_id,
                subtype = active.subtype,
                reason = reason
            })
            active = nil
        end
    end

    host:on("room_update", function(ctx)
        ctx = ctx or {}
        local entities = ctx.entities or {}
        local item_count = ctx.alt_path_collectible_count
        if item_count == nil then
            item_count = count_collectible_pedestals(entities)
        end
        local settings = ctx.settings or {}
        ctx.settings = settings

        local candidate = nil
        for i = 1, #entities do
            local entity = entities[i]
            local by_subtype = entity and seen[entity.entity_id]
            local already_seen = by_subtype and by_subtype[entity.subtype]
            if is_qualifying(entity, ctx, item_count) and not already_seen then
                candidate = entity
                break
            end
        end

        if active ~= nil then
            local current = nil
            for i = 1, #entities do
                if entities[i].entity_id == active.entity_id then
                    current = entities[i]
                    break
                end
            end

            if current == nil then
                stop_if_active("pedestal_removed_or_picked_up")
            elseif current.subtype ~= active.subtype then
                stop_if_active("pedestal_changed")
            elseif not is_qualifying(current, ctx, item_count) then
                stop_if_active("pedestal_no_longer_qualifies")
            end
        end

        if active == nil and candidate ~= nil then
            local by_subtype = seen[candidate.entity_id]
            if not by_subtype then
                by_subtype = {}
                seen[candidate.entity_id] = by_subtype
            end
            by_subtype[candidate.subtype] = true
            active = {
                entity_id = candidate.entity_id,
                subtype = candidate.subtype
            }
            host:emit_action("specialist_start", {
                entity_id = candidate.entity_id,
                subtype = candidate.subtype,
                item_name = candidate.item_name,
                quality = candidate.quality,
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
