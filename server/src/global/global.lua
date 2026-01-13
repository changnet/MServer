-- global.lua
-- 2015-09-07
-- xzc
-- 常用的全局函数

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
    g_async_log:error(129, str)
    return str
end

-- 断言，支持多个message参数
function assert(expr, ...)
    if expr then return expr end

    -- 这里不需要添加堆栈信息，触发error时会自动添加
    return error(table.concat({"assertion failed!", ...}, "    "))
end
