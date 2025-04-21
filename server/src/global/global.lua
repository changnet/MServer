-- global.lua
-- 2015-09-07
-- xzc
-- 常用的全局函数

-- 用于存储全局临时数据
__global_storage = __global_storage or {}

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
    debug.tracestack(msg_list, 4)

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
    debug.tracestack(msg_list, 4)
    msg = table.concat(msg_list, "\n")
    return error(msg)
end

-- 创建一块数据存储空间，仅仅是为了方便第一管理，热更，本地化
-- @param def_val 存储空间初始化时默认值
-- @param init_func 存储空间初始时执行的初始化函数
-- 复杂的初始化使用函数而不是def_val，避免热更不断创建def_val变量
function global_storage(key, def_val, init_func)
    local storage = __global_storage[key]
    if not storage then
        storage = def_val or {}
        __global_storage[key] = storage

        if init_func then init_func(storage) end
    end

    return storage
end
