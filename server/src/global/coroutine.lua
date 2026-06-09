-- coroutines库增强

local debug = g_debug

--[[
有多个模块需要重写coroutine的resume、yield等函数
1. 模块A重写原始函数，模块B重写A的函数，模块C重写B的函数
    理论上这没有什么问题，但效率比较低
    单个模块无法取消

2. 提供一个hook函数，所有模块通过hook管理
    方便统一处理，易于调试
    很难自定义处理，重写可以在前后添加自己的逻辑，但hook就要两个函数才能区分前后

无论哪种方式，热更时必须要恢复原始函数，否则就会一层层叠加上去
]]

if not coroutine.__resume then
    coroutine.__yield = coroutine.yield
    coroutine.__resume = coroutine.resume
else
    coroutine.yield = coroutine.__yield
    coroutine.resume = coroutine.__resume
end

if debug then
    -- 设置当前运行中的协程，用于死循环时底层调试
    local set_running_co = _G.__set_running_co

    local resume = coroutine.resume
    local function _resume_ret(...)
        set_running_co()
        return ...
    end

    function coroutine.resume(co, ...)
        set_running_co(co)
        return _resume_ret(resume(co, ...))
    end
end
