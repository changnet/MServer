-- 解析协议文件并自动生成协议号

-- ＠param path 需要解析的文件路径
-- ＠param s_path 生成的服务器协议文件路径(包含文件名)
-- ＠param c_type 成的客户端协议文件类型，支持 lua、ts(typescript)
-- ＠param c_path 生成的客户端协议文件路径(包含文件名)
local path, s_path, c_type, c_path = ...

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
local function to_readable( val )
    if type(val) == "string" then
        return "\"" .. val .. "\""
    end

    return val
end

--- @param data 要打印的字符串
--- @param [max_level] table要展开打印的计数，默认nil表示全部展开
--- @param [prefix] 用于在递归时传递缩进，该参数不供用户使用于
local recursion = {}
local function var_dump(data, max_level, prefix)
    if type(prefix) ~= "string" then
        prefix = ""
    end

    if type(data) ~= "table" then
        print(prefix .. tostring(data))
    elseif recursion[data] then
        print(data,"dumplicate")  --重复递归
    else
        recursion[data] = true
        print(data)

        local prefix_next = prefix .. "    "
        print(prefix .. "{")
        for k,v in pairs(data) do
            io.stdout:write(prefix_next .. tostring( to_readable(k) ) .. " = ")
            if type(v) ~= "table" or
                (type(max_level) == "number" and max_level <= 1) then
                print( to_readable(v) )
            else
                var_dump(v, max_level - 1, prefix_next)
            end
        end
        print(prefix .. "}")
    end
end

--[[
    eg: local b = {aaa="aaa",bbb="bbb",ccc="ccc"}
]]
function vd(data, max_level)
    var_dump(data, max_level or 20)
    recursion = {}  --释放内存
end
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

-- 加载lua文件，并读取其中的全局变量
local function load_define(path)
    local f = io.open(path, "r")

    -- 文件不存在
    if not f then return {} end

    io.close(f)

    local define = {}
    setmetatable(_G, {
        __newindex = define
    })
    -- require(path) -- require不能处理带../..这种相对路径，或者固定路径
    dofile(path)
    setmetatable(_G, nil)

    return define
end

local function nextLine(ctx)
    if ctx.index >= #ctx.lines then
        ctx.line = nil
        return false
    end

    ctx.index = ctx.index + 1
    ctx.line = ctx.lines[ctx.index]

    return true
end

-- 解析注释
local function parse_multi_comment(ctx)
    -- 多行注释 --[[]]
    if not string.match(ctx.line, "^%s*%-%-%[%[") then return false end

    repeat
        if string.find(ctx.line, "]]") then break end
    until not nextLine(ctx)

    return true
end

-- 解析单行注释
local function parse_comment(ctx)
    if not string.match(ctx.line, "^%s*%-%-") then return end

    return true
end

-- 解析table是否结束
local function parse_brace(ctx)
    if not ctx.brace then return false end

    local lbyte = string.byte("{")
    local rbyte = string.byte("}")

    -- 不计算在注释里的括号
    -- TODO: 不计算在字符串里的括号
    local t = string.find(ctx.line, "%-%-")
    for i = 1, t and t - 1 or string.len(ctx.line) do
        local byte = string.byte(ctx.line, i)
        if byte == lbyte then
            ctx.brace = 1 + (ctx.brace or 0)
        elseif byte == rbyte then
            ctx.brace = ctx.brace - 1
        end
    end

    assert(ctx.brace >= 0)
    if 0 == ctx.brace then
        ctx.m = nil
        ctx.brace = nil
    end

    -- 这个函数总是返回false，不影响解析其他内容
    return false
end

-- 解析table
local function parse_table(ctx)
--[[
定义客户端与服务器之间的通信协议，其格式必须是固定的，用于自动解析并生成协议号

-- 定义模块号，大写。之所以没定义成全局的CHAT_CHAT而是定义成一个table是方便模块里本地化
CHAT = {
    -- 定义功能，大写，表示常量不可修改
    -- s表示服务器发往客户端使用的protobuf或者flatbuffer结构名（没有可以忽略此字段）
    -- c表示客户端发往服务器使用的protobuf或者flatbuffer结构名（没有可以忽略此字段）
    -- i是工具自动生成的协议号，定义时无需定义该字段
    CHAT = { s = "chat.SChat", c = "chat.CChat", i = 1}

-- 格式要求
-- 1. 名字和等号比如在同一行，如 CHAT =
-- 2. c、s字段和结构名比如在同一行，如s = "chat.SChat"，但c、s可以分开在多行写
}
]]
    local line = ctx.line
    local name = string.match(line, "^%s*([%w_]+)%s*=")
    if not name then return end

    -- 模块名
    if not ctx.m then
        ctx.m = name
        ctx.brace = 0
        ctx.symbols[name] = {}

        parse_brace(ctx)
        return
    end


    local s = string.match(line, "s%s*=%s*([%w_%.'\"]+)")
    local c = string.match(line, "c%s*=%s*([%w_%.'\"]+)")

    -- 行尾注释
    local t = string.match(line, "%-%-%[%[(.*)%]%]")
    if not t then
        t = string.match(line, "%-%-%s*(.*)")
    end

    -- 处理 s = "Chat.SChat" 放到独立行的情况
    local mm = nil
    if "s" ~= name and "c" ~= name then
        mm = name
        ctx.mm = name
        ctx.symbols[ctx.m][name] = {}
        table.insert(ctx.sym_list, {
            m = ctx.m,
            mm = ctx.mm,
        })
    else
        if s then ctx.symbols[ctx.m][ctx.mm].s = s end
        if c then ctx.symbols[ctx.m][ctx.mm].c = c end
        if t then ctx.symbols[ctx.m][ctx.mm].t = t end
    
        ctx.changes[ctx.index] = {
            m = ctx.m,
            mm = ctx.mm,
        }
    end

    return true
end

-- 解析协议定义
local function parse(path)
    local ctx = {
        index = 0,
        lines = {},
        symbols = {},
        changes = {},
        sym_list = {}
    }
    for line in io.lines(path) do table.insert(ctx.lines, line) end

    while nextLine(ctx) do
        repeat
            -- 这些解析顺序有严格要求，比如单行注释必须在多行注释之后
            -- 因为必须先检测--[[之后才检测--
            if parse_multi_comment(ctx) then break end
            if parse_comment(ctx) then break end
            if parse_brace(ctx) then break end
            if parse_table(ctx) then break end
        until true
    end
    -- vd(ctx.symbols)
    assert(0 == (ctx.brace or 0))

    return ctx
end

local function write_fields(f, val, prefix, first)
    if not val then return first end

    if not first then f:write(", ") end
    if prefix then f:write(prefix) end

    f:write(val)

    return false
end

-- 生成lua配置文件
local function write_lua(path, ctx)
    local f = io.open(path, "w")

    assert(f)

    f:write("-- AUTO GENERATE, DO NOT MODIFY\n\n")

    for index, line in ipairs(ctx.lines) do
        local change = ctx.changes[index]
        if not change then
            f:write(line)
        else
            local v = ctx.symbols[change.m][change.mm]

            local first = true
            f:write("        ")
            first = write_fields(f, v.s, "s = ", first)
            first = write_fields(f, v.c, "c = ", first)
            first = write_fields(f, v.i, "i = ", first)
            first = write_fields(f, v.t, "-- ", first)
        end
        f:write("\n")
    end

    f:flush()
    io.close(f)
end

-- 生成typescript配置
local function write_ts(path, ctx)
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

local function main()
    local max_id = 0
    local old_define = load_define(s_path)
    for _, m in pairs(old_define) do
        if "table" == type(m) then
            for _, mm in pairs(m) do
                if mm.i and mm.i > max_id then max_id = mm.i end
            end
        end
    end
    print(string.format("loading old define from %s, old = %d", s_path, max_id))

    local ctx = parse(path)
    for _, v in ipairs(ctx.sym_list) do
        local m = v.m
        local mm = v.mm

        local old_m = old_define[m] or {}
        local old_mm = old_m[mm] or {}
        if old_mm.i then
            ctx.symbols[m][mm].i = old_mm.i
        else
            max_id = max_id + 1
            ctx.symbols[m][mm].i = max_id
        end
    end
    print(string.format("auto generating from %s, new = %d", path, max_id))

    write_lua(s_path, ctx)
    print(string.format("writing new define to %s", s_path))

    if c_type == "ts" or c_type == "typescript" then
        write_ts(c_path, ctx)
        print(string.format("writing new define to %s", c_path))
    elseif c_type == "lua" then
        write_lua(c_path, ctx)
        print(string.format("writing new define to %s", c_path))
    end
end

main()
