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
function debug.tracestack(co, var_list, level)
    --[[
    当不传co或者在同一个协程内时：
    which means the function running at level f of the call stack of the given
    thread: level 0 is the current function (getinfo itself); level 1 is the
    function that called getinfo (except for tail calls, which do not count in
    the stack); and so on.

    所以level=2时表示不打印下面这几个堆栈的信息
     0 debug.getinfo
     1 debug.tracestack
     2 __G_DUMP_STACK

    当传co时，co通常是一个协程，协程报错后变为dead状态并退出，此时获取它的堆栈就没有上
    面的那几个堆栈，直接从0获取即可。
    ]]
    if not co then
        co = coroutine.running()
        if not level then level = 2 end
    else
        if not level then level = 0 end
    end

    -- 各层调用堆栈上的local变量
    for lv = level, 16 do
        -- 检测该层堆栈是否存在，否则getlocal会报错
        -- n=name，函数名 S=source源文件 l=line行号
        local dbg_info = debug.getinfo(co, lv, "nSl")
        if not dbg_info then break end

        local sub_msg = {}
        for index = 1, 32 do
            local k, v = debug.getlocal(co, lv, index)
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
    local co = coroutine.running()

    debug.tracestack(co, var_list, 3)
    local stack_trace = debug.traceback(co)

    table.insert(var_list, stack_trace)
    warn(table.concat(var_list, "\n"))
end
