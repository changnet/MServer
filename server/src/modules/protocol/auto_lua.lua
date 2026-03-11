-- 自动生成lua协议

local AutoLua = {}

local function write_fields(f, val, prefix, first)
    if not val then return first end

    if not first then f:write(", ") end
    if prefix then f:write(prefix) end

    f:write(val)

    return false
end

function AutoLua.generate(ctx, path)
    local f = io.open(path, "w")

    assert(f)

    local done = {}
    local processed_comments = {}
    for index, line in ipairs(ctx.lines) do
        local change = ctx.changes[index]

        -- 检查当前行是否是某个协议的上一行注释
        local replace_line_str = nil
        for _, sym in pairs(ctx.sym_list) do
            local v = ctx.symbols[sym.m][sym.mm]
            if v and v.prev_t_line == index then
                -- 提取原有注释，去掉可能已有的ID前缀
                local old_comment = (v.prev_t or ""):gsub("^%s*%d+%s*", "")
                if old_comment == "" then
                    replace_line_str = string.format("    -- %d\n", v.i)
                else
                    replace_line_str = string.format("    -- %d %s\n", v.i, old_comment)
                end
                processed_comments[sym.mm] = true
                break
            end
        end

        if replace_line_str then
            f:write(replace_line_str)
        elseif not change then
            -- 如果这行不是被修改的行，且可能是我们要补注释的行
            local name = string.match(line, "^%s*([%w_]+)%s*=")
            if name then
                local found_sym = nil
                for _, sym in pairs(ctx.sym_list) do
                    if sym.mm == name then
                        found_sym = ctx.symbols[sym.m][sym.mm]
                        break
                    end
                end
                if found_sym and not processed_comments[name] then
                    local id = found_sym.i
                    f:write(string.format("    -- %d\n", id))
                end
            end
            f:write(line)
            f:write("\n")
        else
            local v = ctx.symbols[change.m][change.mm]

            if not done[v.i] then
                done[v.i] = true
                local first = true
                f:write("        ")
                first = write_fields(f, v.s, "s = ", first)
                first = write_fields(f, v.c, "c = ", first)
                first = write_fields(f, v.i, "i = ", first)
                first = write_fields(f, v.w, "w = ", first)
                first = write_fields(f, v.t, "-- ", first)
                f:write("\n")
            end
        end
    end

    f:flush()
    io.close(f)
end

return AutoLua
