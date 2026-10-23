-- A nested local Workshop smoke-test module.
workshop_module_probe = (workshop_module_probe or 0) + 1
local item_config = Isaac.GetItemConfig()
return function(id)
    local item = item_config:GetCollectible(id)
    return item ~= nil and item.Quality == 4
end
