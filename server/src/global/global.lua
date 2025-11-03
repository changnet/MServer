-- global.lua
-- 2015-09-07
-- xzc
-- 常用的全局函数

-- 用于底层C错误信息处理
function __G_C_TRACKBACK(msg, co)
    local msg_list = {}
    debug.tracestack(msg_list, 3)

    local stack_trace = debug.traceback(co)

    table.insert(msg_list, msg)
    table.insert(msg_list, stack_trace)
    return table.concat(msg_list, "\n")
end

-- 用于lua错误信息处理
function __G__TRACKBACK(msg, co)
    local msg_list = {}
    debug.tracestack(msg_list, 3)

    local stack_trace = debug.traceback(co)

    table.insert(msg_list, msg)
    table.insert(msg_list, stack_trace)
    local str = table.concat(msg_list, "\n")
    eprint(str)
    return str
end

-- 断言，支持多个message参数
function assert(expr, ...)
    if expr then return expr end

    local msg = table.concat({"assertion failed!", ...}, "    ")
    local msg_list = {msg}
    debug.tracestack(msg_list, 3)
    msg = table.concat(msg_list, "\n")
    return error(msg)
end
