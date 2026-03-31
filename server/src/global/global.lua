-- global.lua
-- 2015-09-07
-- xzc
-- 常用的全局函数

__script_loaded_funcs = {}

local xpcall = xpcall

-- 在加载完脚本后调用（优先级比SE_SCRIPT_LOADED高，且无文件依赖，用于基础功能初始化）
function script_loaded(func)
    table.insert(__script_loaded_funcs, func)
end

-- 用于底层C错误信息处理
function __G_C_TRACKBACK(msg, co)
    local msg_list = {}
    debug.tracestack(co, msg_list, 2)

    local stack_trace = debug.traceback(co)

    table.insert(msg_list, msg)
    table.insert(msg_list, stack_trace)
    return table.concat(msg_list, "\n")
end

-- 用于lua错误信息处理(直接打印错误信息)
function __G_DUMP_STACK(msg, co)
    local msg_list = {}
    -- 在co_pool中co_return要level=2才会打印逻辑所在的函数调用堆栈
    debug.tracestack(co, msg_list, 2)

    local stack_trace = debug.traceback(co)

    table.insert(msg_list, msg)
    table.insert(msg_list, stack_trace)
    local str = table.concat(msg_list, "\n")

    -- 这个参考eprint()写法，但不要自动附加堆栈
    g_async_log:print("error", 129, str)
    return str
end

-- 断言，支持多个message参数
function assert(expr, ...)
    if expr then return expr end

    -- 这里不需要添加堆栈信息，触发error时会自动添加
    return error(table.concat({"assertion failed!", ...}, "    "))
end

local function scall_ret(success, ...)
    -- __G_DUMP_STACK会自动打印错误日志，所以这里不需要再打印一次了
    if not success then return false end

    return true, ...
end

-- safe_call，类似xpcall，但自动捕获错误堆栈并打印错误日志
--@return boolean 成功返回true，原函数返回的值。失败返回false
function scall(func, ...)
    return scall_ret(xpcall(func, __G_DUMP_STACK, ...))
end
