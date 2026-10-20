-- Lua 5.1 mod registration and callback runtime.
-- Isaac-facing globals must be supplied by an explicit host adapter. This module
-- does not emulate Isaac, discover files, or decide callback return semantics.
local Runtime = {}
Runtime.__index = Runtime

local SAFE_GLOBALS = {
    "_VERSION", "assert", "error", "getmetatable", "ipairs", "next", "pairs",
    "pcall", "print", "rawequal", "rawget", "rawset", "select", "setmetatable",
    "tonumber", "tostring", "type", "unpack", "xpcall",
    "coroutine", "math", "string", "table"
}

local function pack(...)
    return { n = select("#", ...), ... }
end

local function copy_table(source, destination)
    for key, value in pairs(source) do
        destination[key] = value
    end
end

function Runtime.new(native_api)
    assert(type(native_api) == "table", "native API adapter is required")
    assert(type(native_api.globals) == "table", "native_api.globals must be a table")
    assert(type(native_api.globals.ModCallbacks) == "table",
        "host must provide the target build's ModCallbacks values")

    local self = setmetatable({
        native = native_api,
        callbacks = {},
        mods = {}
    }, Runtime)
    return self
end

function Runtime:_log(message)
    local logger = self.native.log
    if type(logger) == "function" then
        pcall(logger, tostring(message))
    else
        print(tostring(message))
    end
end

function Runtime:_remove_callbacks(mod)
    for callback_id, list in pairs(self.callbacks) do
        local kept = {}
        for i = 1, #list do
            if list[i].mod ~= mod then
                kept[#kept + 1] = list[i]
            end
        end
        if #kept == 0 then
            self.callbacks[callback_id] = nil
        else
            self.callbacks[callback_id] = kept
        end
    end
end

function Runtime:_register_mod(name, api_version)
    assert(type(name) == "string" and name ~= "", "mod name must be a non-empty string")
    assert(type(api_version) == "number", "mod API version must be a number")
    assert(self.mods[name] == nil, "a mod with this name is already loaded")

    local runtime = self
    local mod = {
        Name = name,
        ApiVersion = api_version
    }

    function mod:AddCallback(callback_id, callback, filter)
        assert(type(callback_id) == "number" or type(callback_id) == "string",
            "callback ID must be a host-provided number or string")
        assert(type(callback) == "function", "callback must be a function")
        if filter ~= nil then
            error("callback filters are not implemented by this runtime", 2)
        end

        local list = runtime.callbacks[callback_id]
        if list == nil then
            list = {}
            runtime.callbacks[callback_id] = list
        end
        list[#list + 1] = { mod = mod, callback = callback }
    end

    function mod:SaveData(data)
        local save = runtime.native.save_data
        if type(save) ~= "function" then
            error("SaveData requires a host persistence adapter", 2)
        end
        return save(self.Name, data)
    end

    function mod:LoadData()
        local load = runtime.native.load_data
        if type(load) ~= "function" then
            error("LoadData requires a host persistence adapter", 2)
        end
        return load(self.Name)
    end

    function mod:HasData()
        local has_data = runtime.native.has_data
        if type(has_data) ~= "function" then
            error("HasData requires a host persistence adapter", 2)
        end
        return has_data(self.Name)
    end

    self.mods[name] = mod
    return mod
end

function Runtime:load_mod(name, source)
    assert(type(name) == "string" and name ~= "", "mod name must be a non-empty string")
    assert(type(source) == "string", "mod source must be provided as text")
    assert(self.mods[name] == nil, "a mod with this name is already loaded")

    local chunk, compile_error = loadstring(source, "@" .. name .. "/main.lua")
    if chunk == nil then
        return nil, compile_error
    end

    local environment = {}
    for i = 1, #SAFE_GLOBALS do
        local key = SAFE_GLOBALS[i]
        environment[key] = _G[key]
    end
    copy_table(self.native.globals, environment)

    local registered_mod
    environment.RegisterMod = function(mod_name, api_version)
        if registered_mod ~= nil then
            error("a mod script may call RegisterMod only once", 2)
        end
        if mod_name ~= name then
            error("RegisterMod name does not match the loaded mod name", 2)
        end
        registered_mod = self:_register_mod(mod_name, api_version)
        return registered_mod
    end
    environment._G = environment

    setfenv(chunk, environment)
    local ok, load_error = pcall(chunk)
    if not ok then
        if registered_mod ~= nil then
            self:_remove_callbacks(registered_mod)
            self.mods[registered_mod.Name] = nil
        end
        return nil, load_error
    end
    if registered_mod == nil then
        return nil, "mod script did not call RegisterMod"
    end
    return registered_mod
end

-- The host supplies the callback ID and native arguments. Results are returned
-- individually; aggregation into engine-specific return values remains a host
-- policy and is deliberately not guessed here.
function Runtime:dispatch(callback_id, ...)
    local list = self.callbacks[callback_id]
    local results = {}
    local errors = {}
    if list == nil then
        return results, errors
    end

    local arguments = pack(...)
    for i = 1, #list do
        local entry = list[i]
        local values
        local ok, callback_error = pcall(function()
            values = pack(entry.callback(entry.mod, unpack(arguments, 1, arguments.n)))
        end)
        if ok then
            results[#results + 1] = {
                mod_name = entry.mod.Name,
                values = values,
                value_count = values.n
            }
        else
            local message = tostring(callback_error)
            errors[#errors + 1] = { mod_name = entry.mod.Name, message = message }
            self:_log("callback error [" .. tostring(callback_id) .. "] " ..
                entry.mod.Name .. ": " .. message)
        end
    end
    return results, errors
end

function Runtime:unload_mod(name)
    local mod = self.mods[name]
    if mod == nil then
        return false
    end
    self:_remove_callbacks(mod)
    self.mods[name] = nil
    return true
end

return Runtime
