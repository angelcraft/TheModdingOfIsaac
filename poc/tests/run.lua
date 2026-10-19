-- Deterministic unit tests. Run from project root with: lua tests/run.lua
package.path = "./source/?.lua;./mods/?.lua;" .. package.path

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
local qualifying = {[42] = true, [99] = true}
local function frame(entities)
    host:emit("room_update", {entities = entities, qualifying_items = qualifying})
end
local function entity(id, kind, variant, subtype)
    return {entity_id = id, type = kind, variant = variant, subtype = subtype}
end

-- 1. Ignores non-collectibles and zero-subtype pickups.
frame({entity(1, "pickup", "heart", 42), entity(2, "pickup", "collectible", 0)})
check(#actions == 0, "irrelevant pickups must not trigger Specialist")

-- 2. Starts exactly once when a qualifying pedestal appears.
frame({entity(10, "pickup", "collectible", 42)})
check(#actions == 1 and actions[1].name == "specialist_start", "qualifying pedestal should start dance")
check(actions[1].payload.entity_id == 10 and actions[1].payload.timing == "before_pickup", "start payload should identify pedestal and timing")
frame({entity(10, "pickup", "collectible", 42)})
check(#actions == 1, "same pedestal must not retrigger")

-- 3. Stops when pedestal is gone, even if another room tick follows.
frame({})
check(#actions == 2 and actions[2].name == "specialist_stop", "disappearing pedestal should stop dance")
frame({})
check(#actions == 2, "stop should only fire once")

-- 4. A different pedestal can trigger; room change cleans state.
frame({entity(20, "pickup", "collectible", 99)})
check(#actions == 3 and actions[3].name == "specialist_start", "new qualifying pedestal should trigger")
host:emit("new_room", {})
check(#actions == 4 and actions[4].name == "specialist_stop", "room change should stop active dance")

-- 5. Callback error is contained and logged; other callbacks still execute.
local ran_after_error = false
host:on("error_test", function() error("expected-test-error") end)
host:on("error_test", function() ran_after_error = true end)
host:emit("error_test", {})
check(ran_after_error, "later callback should run after a callback error")
check(#logs == 1 and string.find(logs[1], "expected%-test%-error") ~= nil, "callback error should be logged")

print("PASS: 5 isolated Lua POC checks")
print("NOTE: mock host only; no Isaac API, game integration, or Switch validation")
