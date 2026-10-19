-- Shared deterministic checks for the source-derived Specialist mock.
-- This exact file runs on host Lua 5.1 and in the standalone Switch NRO.
-- It does not call Isaac APIs or inspect game memory.
local source = debug.getinfo(1, "S").source
if source:sub(1, 1) == "@" then source = source:sub(2) end
source = source:gsub("\\", "/")
local script_dir = source:match("^(.*[/])") or "./"
package.path = script_dir .. "source/?.lua;" .. script_dir .. "mods/?.lua;" .. package.path

local Core = require("loader_core")
local specialist = require("specialist_probe")
local actions, logs = {}, {}
local host = Core.new(function(line) logs[#logs + 1] = line end)
function host:emit_action(name, payload)
    actions[#actions + 1] = { name = name, payload = payload }
end
specialist.install(host)

local function check(condition, message)
    if not condition then error("FAIL: " .. message, 2) end
end

local function item(id, subtype, quality, name, extra)
    local e = {
        entity_id = id,
        type = "pickup",
        variant = "collectible",
        subtype = subtype,
        quality = quality,
        item_name = name
    }
    for key, value in pairs(extra or {}) do e[key] = value end
    return e
end

local function frame(entities, options)
    local ctx = {entities = entities, settings = {}}
    for key, value in pairs(options or {}) do ctx[key] = value end
    host:emit("room_update", ctx)
end

local function next_room()
    host:emit("new_room", {})
end

local function clear_actions()
    actions = {}
end

-- 1. Non-collectibles, subtype zero, missing quality, and below-threshold
-- items with no explicit override do not trigger.
frame({
    {entity_id = 1, type = "pickup", variant = "heart", subtype = 42, quality = 4},
    item(2, 0, 4, "Zero Subtype"),
    item(3, 42, nil, "Missing Quality"),
    item(4, 42, 3, "Quality Three")
})
check(#actions == 0, "irrelevant or non-qualifying pickups must not trigger")

-- 2. Default quality threshold, identity deduplication, cleanup and new rooms.
frame({item(10, 42, 4, "Quality Four")})
check(#actions == 1 and actions[1].name == "specialist_start", "quality-four pedestal should start")
check(actions[1].payload.entity_id == 10 and actions[1].payload.timing == "before_pickup", "start identifies the pedestal and timing")
frame({item(10, 42, 4, "Quality Four")})
check(#actions == 1, "the same pedestal must not retrigger")
frame({})
check(#actions == 2 and actions[2].name == "specialist_stop", "removed pedestal should stop")
frame({item(20, 99, 4, "Another Quality Four")})
check(#actions == 3 and actions[3].name == "specialist_start", "a different pedestal should trigger")
next_room()
check(#actions == 4 and actions[4].name == "specialist_stop", "room change should stop the dance")

-- 3. Source defaults can trigger below quality four.
clear_actions()
frame({item(30, 42, 0, "Cricket's Head")})
check(#actions == 1 and actions[1].name == "specialist_start", "default custom item should override the quality threshold")
next_room()

-- 4. Explicit allowlist entry wins; explicit deny vetoes quality.
clear_actions()
frame({item(40, 43, 0, "Custom Good")}, {
    settings = {custom_pog_list = {["Custom Good"] = 1}}
})
check(#actions == 1 and actions[1].name == "specialist_start", "custom value 1 should force a dance")
next_room()
clear_actions()
frame({item(41, 44, 4, "Custom Bad")}, {
    settings = {custom_pog_list = {["Custom Bad"] = 2}}
})
check(#actions == 0, "custom value 2 should block a quality-qualified item")

-- 5. IgnoreQuality disables the quality-based path; explicit entries still work.
frame({item(50, 45, 4, "Quality Disabled")}, {
    settings = {ignore_quality = true}
})
check(#actions == 0, "IgnoreQuality should skip the quality-based trigger")
frame({item(51, 46, 0, "Forced Despite Ignore")}, {
    settings = {
        ignore_quality = true,
        custom_pog_list = {["Forced Despite Ignore"] = 1}
    }
})
check(#actions == 1 and actions[1].payload.entity_id == 51, "explicit allowlist should still work")

-- 6. Curse of the Blind blocks unless bypass is enabled.
next_room()
clear_actions()
local blind_item = item(60, 47, 4, "Blind Room Item")
frame({blind_item}, {curse_of_blind = true})
check(#actions == 0, "Blind curse should block by default")
frame({blind_item}, {
    curse_of_blind = true,
    settings = {blind_bypass = true}
})
check(#actions == 1 and actions[1].name == "specialist_start", "Blind bypass should allow the trigger")

-- 7. Alt-path visibility uses the source's position rules.
next_room()
clear_actions()
frame({
    item(70, 48, 4, "Right Item", {position_x = 400, position_y = 200}),
    item(71, 49, 2, "Low Quality Left", {position_x = 200, position_y = 200})
}, {
    alt_path_treasure_room = true,
    alt_path_collectible_count = 2
})
check(#actions == 0, "the invisible right-side item must not trigger")
frame({
    item(70, 48, 4, "Right Item", {position_x = 400, position_y = 200}),
    item(72, 49, 4, "Visible Left", {position_x = 200, position_y = 200})
}, {
    alt_path_treasure_room = true,
    alt_path_collectible_count = 2
})
check(#actions == 1 and actions[1].payload.entity_id == 72, "visible left-side item should trigger")

-- 8. A rerolled pedestal stops the old action before starting the new item.
next_room()
clear_actions()
frame({item(80, 42, 4, "Before Reroll")})
frame({item(80, 99, 4, "After Reroll")})
check(#actions == 3, "reroll should emit stop then start")
check(actions[1].name == "specialist_start" and actions[2].name == "specialist_stop"
    and actions[3].name == "specialist_start", "reroll action order should be start-stop-start")
check(actions[2].payload.reason == "pedestal_changed", "reroll stop should identify a pedestal change")

-- 9. Stable identity is required for deduplication and safe cleanup.
next_room()
clear_actions()
frame({item(nil, 42, 4, "No Stable Identity")})
check(#actions == 0, "entity without stable identity must fail closed")

-- Callback failures remain contained by the core dispatcher.
local ran_after_error = false
host:on("error_test", function() error("expected-test-error") end)
host:on("error_test", function() ran_after_error = true end)
host:emit("error_test", {})
check(ran_after_error, "later callback should run after a callback error")
check(#logs == 1 and string.find(logs[1], "expected%-test%-error") ~= nil, "callback error should be logged")

print("PASS: 9 Specialist mock scenarios plus callback dispatch")
print("Lua runtime: " .. _VERSION)
print("LIMIT: simulation only; no Isaac API or game integration.")
