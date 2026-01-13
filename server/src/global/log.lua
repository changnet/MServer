-- 基础日志接口
Log = {}

local type = type
local tostring = tostring
local g_async_log = g_async_log

local logger_print = g_async_log.print

local vd_recursion = nil
local enable_dbg = _G.__enable_dbg

local function vd_insert(tbl, v, ...)
    -- table.insert本身就不支持insert一个nil值，所以这里只要是nil值就表示中止
    if v then
        tbl[#tbl + 1] = v -- 比table.insert(tbl, v)快点
        return vd_insert(tbl, ...)
    end
end

local function vd_key(k)
    if type(k) == "string" then return k end
    return "[" .. tostring(k) .. "]"
end

local function vd_value(val)
    if type(val) == "string" then return "\"" .. val .. "\"" end

    return tostring(val)
end

--- @param var 要打印的数据
--- @param max_level table要展开打印的计数，默认nil表示全部展开
--- @param prefix 用于在递归时传递缩进，该参数不供用户使用于
local function var_dump(vd_buffer, var, max_level, prefix)
    -- 注意：这个函数格式化出来的日志，一定要是合法的lua代码
    -- 即打印一个变量时，如有需要，可以直接从日志中复制去重现问题
    if type(var) ~= "table" then
        return vd_insert(vd_buffer, prefix, tostring(var))
    elseif vd_recursion[var] then
        return vd_insert(vd_buffer, tostring(var), "dumplicate")-- 重复递归
    else
        vd_recursion[var] = true

        local prefix_next = prefix .. "    "
        vd_insert(vd_buffer, prefix, "{\n")
        for k, v in pairs(var) do
            if type(v) ~= "table" or max_level <= 1 then
                vd_insert(vd_buffer,
                    prefix_next, vd_key(k), " = ", vd_value(v), ",\n")
            else
                vd_insert(vd_buffer, prefix_next, vd_key(k), " =\n")
                var_dump(vd_buffer, v, max_level - 1, prefix_next)
                vd_insert(vd_buffer, ",\n")
            end
        end
        vd_insert(vd_buffer, prefix, "}")
    end
end

-- var dump 打印任意变量的值，包括table
-- @param var 需要打印的变量
-- @param message 附加的消息，可为nil
function vd(var, message)
    local vd_buffer = {}
    vd_recursion = {}

    if not message then
        local info = debug.getinfo(2)
        message = string.format("%s:%d", info.short_src, info.currentline)
    end

    -- TODO 这里可以直接用table.dump，但考虑到log是个基础模块，就不依赖其他代码了

    vd_insert(vd_buffer, message, "\n")
    var_dump(vd_buffer, var, 96, "")

    vd_recursion = nil -- 释放内存
    print(table.concat(vd_buffer))
end

-- 异步print log,只打印，不格式化
function print(...)
    -- 128 = MASK_S_L，参考C++中的定义
    return logger_print(g_async_log, 128, ...)
end

-- 异步print format log,以第一个为format参数，格式化后面的参数
function printf(fmt, ...)
    return logger_print(g_async_log, 128, string.format(fmt, ...))
end

-- 异步调试日志打印，不格式化
function dprint(...)
    if not enable_dbg then return end
    -- 128 = MASK_S_L，参考C++中的定义
    return logger_print(g_async_log, 130, ...)
end

-- 异步调试日志打印,以第一个为format参数，格式化后面的参数
function dprintf(fmt, ...)
    if not enable_dbg then return end
    return logger_print(g_async_log, 130, string.format(fmt, ...))
end

-- 错误日志，写入根目录下的error文件 (参数不能带有nil参数)
function eprint(...)
    -- 129 = MASK_C_R|MASK_S_L，参考C++中的定义

    local list = {...} -- 在5.3+版本中，数组允许包含nil，可以用#list获取长度。但pair遍历不会遍历nil
    for i = 1, #list do
        list[i] = tostring(list[i]) -- 转为string，不然包含nil或者bool之类的变量会报错
    end

    -- 错误时，打印堆栈方便查问题
    local str = table.concat(list, "    ") .. "\n" .. debug.traceback()

    -- 单独输出到error文件，用于触发运维邮件、电话
    return g_async_log:error(129, str)
end

--  格式化日志并写入根目录下的lua_error.txt文件
function eprintf(fmt, ...)
    local str = string.format(fmt, ...) .. "\n" .. debug.traceback()

    return g_async_log:error(129, str)
end

-- 异步print log,只打印，不格式化。仅在日志线程开启后有效
function warn(...)
    -- 136 = MASK_C_Y | MASK_S_L，参考C++中的定义
    return logger_print(g_async_log, 136, ...)
end

-- 异步print format log,以第一个为format参数，格式化后面的参数.仅在日志线程开启后有效
function warnf(fmt, ...)
    return logger_print(g_async_log, 136, string.format(fmt, ...))
end

-- 以红色打印日志
function Log.red(...)
    return logger_print(g_async_log, 129, ...)
end

-- 以红色格式化江打印日志
function Log.redf(fmt, ...)
    return logger_print(g_async_log, 129, string.format(fmt, ...))
end

-- 以红色打印日志
function Log.green(...)
    return logger_print(g_async_log, 130, ...)
end

-- 以红色格式化江打印日志
function Log.greenf(fmt, ...)
    return logger_print(g_async_log, 130, string.format(fmt, ...))
end

-- 以蓝色打印日志
function Log.blue(...)
    return logger_print(g_async_log, 132, ...)
end

-- 以蓝色格式化江打印日志
function Log.bluef(fmt, ...)
    return logger_print(g_async_log, 132, string.format(fmt, ...))
end

-- 以黄色打印日志
function Log.yellow(...)
    return logger_print(g_async_log, 136, ...)
end

-- 以黄色格式化江打印日志
function Log.yellowf(fmt, ...)
    return logger_print(g_async_log, 136, string.format(fmt, ...))
end

-- 启用或禁用调试日志打印
function Log.enable_debug(enable)
    enable_dbg = enable
    _G.__enable_dbg = enable
end
