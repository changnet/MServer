-- 扩展debug库

-- 获取stack上各个变量
-- @param 变量全部存放到var_list中
-- @param level 从哪一级stack开始获取
function debug.tracestack(var_list, level)
    if not level then level = 2 end

    -- 各层调用堆栈上的local变量
    for lv = level, 16 do
        -- 检测该层堆栈是否存在，否则getlocal会报错
        if not debug.getinfo(lv, "f") then break end

        local sub_msg = {}
        for index = 1, 32 do
            local k, v = debug.getlocal(lv, index)
            if not k then break end

            if k ~= "(C temporary)" then
                table.insert(sub_msg,
                    string.format("%s:%s", tostring(k), tostring(v)))
            end
        end
        if #sub_msg > 0 then
            table.insert(var_list, string.format(
                "local%02d: %s", lv - level, table.concat(sub_msg, ",")))
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
                string.format("%s:%s", tostring(k), tostring(v)))
        end

        if #sub_msg > 0 then
            table.insert(var_list, string.format(
                "upvalue: %s", table.concat(sub_msg, ",")))
        end
    end
end
