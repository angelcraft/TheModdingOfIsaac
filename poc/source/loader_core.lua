-- Small isolated callback core; compatible with Lua 5.1 syntax.
local Core = {}
Core.__index = Core

function Core.new(log_fn)
    return setmetatable({ callbacks = {}, log = log_fn or function(_) end }, Core)
end

function Core:on(event_name, callback)
    assert(type(event_name) == "string", "event name must be a string")
    assert(type(callback) == "function", "callback must be a function")
    local list = self.callbacks[event_name]
    if not list then
        list = {}
        self.callbacks[event_name] = list
    end
    list[#list + 1] = callback
end

function Core:emit(event_name, context)
    local list = self.callbacks[event_name]
    if not list then return true end
    for i = 1, #list do
        local ok, err = pcall(list[i], context)
        if not ok then
            self.log("callback error [" .. event_name .. "]: " .. tostring(err))
        end
    end
    return true
end

return Core
