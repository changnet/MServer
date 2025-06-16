-- 自动生成TypeScript协议

local AutoTs = {}

local function write_fields(f, val, prefix, first)
    if not val then return first end

    if not first then f:write(", ") end
    if prefix then f:write(prefix) end

    f:write(val)

    return false
end

function AutoTs.generate(ctx, path)
--[[
export interface CS {
    i: number;
    c?: string;
    s?: string;
}

export class PLAYER {

    /**
     * 登录
     */
    public static LOGIN: CS = {
        s: "player.SLogin", i: 1
    }
}

export const Cmd: Map<number, CS> = new Map<number, CS>([
    [1, PLAYER.LOGIN]
])
]]
    local f = io.open(path, "w")

    assert(f)

    f:write("// AUTO GENERATE, DO NOT MODIFY\n\n")
    f:write("export interface CS {\n")
    f:write("    i: number;\n")
    f:write("    c?: string;\n")
    f:write("    s?: string;\n")
    f:write("}\n\n")

    local cur_m
    for _, v in ipairs(ctx.sym_list) do
        local m = v.m
        if cur_m ~= m then
            -- 上一个模块结束
            if cur_m then f:write("}\n\n") end
            -- 当前模块开始
            cur_m = m
            f:write("export class " .. m .. " {\n")
        end

        local mm = v.mm
        f:write("    public static " .. mm .. ": CS = {\n")
        f:write("        ") -- 缩进

        v = ctx.symbols[m][mm]

        local first = true
        first = write_fields(f, v.s, "s: ", first)
        first = write_fields(f, v.c, "c: ", first)
        first = write_fields(f, v.i, "i: ", first)
        f:write("\n    }\n")
    end

    -- 上一个模块结束
    if cur_m then f:write("}\n\n") end

    -- 写入一个根据i获取协议信息的map
    f:write("export const Cmd: Map<number, CS> = new Map<number, CS>([")
    for _, v in ipairs(ctx.sym_list) do
        local m = v.m
        local mm = v.mm
        v = ctx.symbols[m][mm]
        f:write(string.format("\n    [%d, %s.%s],", v.i, m, mm))
    end
    f:write("\n]);\n")

    f:flush()
    io.close(f)
    print("TODO: 导出注释到ts")
end

return AutoTs
