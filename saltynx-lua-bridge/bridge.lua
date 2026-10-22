local callbacks = 0

function on_update(frame)
    callbacks = callbacks + 1
    if callbacks == 1 then
        log("bridge.lua: reached from IsaacUpdate at frame " .. tostring(frame))
    end
end

