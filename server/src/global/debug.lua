-- 扩展debug库

local type = type
local tostring = tostring

local function trace_var(v)
    -- 有时候string变量是很大的，直接打印出来会刷屏或者导致log文件超级大
    if "string" == type(v) then
        local len = string.len(v)
        if len > 256 then
            return string.sub(v, 1, 256) .. string.format("(%d)...", len)
        end
    end

    return tostring(v)
end

-- 获取stack上各个变量
-- @param 变量全部存放到var_list中
-- @param level 从哪一级stack开始获取
function debug.tracestack(var_list, level)
    if not level then level = 2 end

    -- 各层调用堆栈上的local变量
    for lv = level, 16 do
        -- 检测该层堆栈是否存在，否则getlocal会报错
        -- n=name，函数名 S=source源文件 l=line行号
        local dbg_info = debug.getinfo(lv, "nSl")
        if not dbg_info then break end

        local sub_msg = {}
        for index = 1, 32 do
            local k, v = debug.getlocal(lv, index)
            if not k then break end

            if k ~= "(C temporary)" then
                table.insert(sub_msg,
                    string.format("%s:%s", tostring(k), trace_var(v)))
            end
        end
        if #sub_msg > 0 then
            local func_name = dbg_info.name
            if not func_name then
                func_name = string.format("%s:%d",
                    dbg_info.short_src, dbg_info.currentline)
            end
            table.insert(var_list, string.format(
                "#%d [%s] %s", lv - level, func_name, table.concat(sub_msg, ";")))
        end
    end

    -- 只打印最后一个函数调用的upvalue，不然就太多了
    local info = debug.getinfo(level, "f")
    if info and info.func then
        local sub_msg = {}
        for index = 1, 32 do
            local k, v = debug.getupvalue(info.func, index)
            if not k then break end

            table.insert(sub_msg,
                string.format("%s:%s", tostring(k), trace_var(v)))
        end

        if #sub_msg > 0 then
            table.insert(var_list, string.format(
                "upvalue: %s", table.concat(sub_msg, ",")))
        end
    end
end

-- 打印stack同时打印堆栈上的变量
function debug.dumpstack(msg)
    local var_list = {msg}
    debug.tracestack(var_list, 3)

    local co = coroutine.running()
    local stack_trace = debug.traceback(co)

    table.insert(var_list, stack_trace)
    warn(table.concat(var_list, "\n"))
end
