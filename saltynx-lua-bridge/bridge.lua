-- Bridge runtime checks; no simulated Isaac entities or Specialist behavior.
local callbacks = 0

function on_update(frame)
    assert(IsaacBridge.GetFrameCount() == frame, "callback frame mismatch")
    if callbacks < 4 or callbacks % 600 == 0 then
        local runtime = IsaacBridge.GetRuntimeState()
        assert(runtime.target_verified, "unverified callback target")
        assert(runtime.completed_callbacks == callbacks, "callback count mismatch")
        assert(runtime.failed_callbacks == 0, "previous callback failure")
        log("IsaacLuaBridge 0.4.0: continuous callback " .. (callbacks + 1))
    end
    callbacks = callbacks + 1
end
