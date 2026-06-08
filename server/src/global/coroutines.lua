-- coroutines库增强

local debug = g_debug

if debug then
    local coroutine = coroutine

    -- 设置当前运行中的协程，用于死循环时底层调试
    local set_running_co = _G.__set_running_co
    if not coroutine.__resume then coroutine.__resume = coroutine.resume end

    local resume = coroutine.__resume
    local function _resume_ret(...)
        set_running_co()
        return ...
    end

    function coroutine.resume(co, ...)
        set_running_co(co)
        return _resume_ret(resume(co, ...))
    end
end
