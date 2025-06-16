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
    for index, line in ipairs(ctx.lines) do
        local change = ctx.changes[index]
        if not change then
            f:write(line)
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
            end
        end
        f:write("\n")
    end

    f:flush()
    io.close(f)
end

return AutoLua
