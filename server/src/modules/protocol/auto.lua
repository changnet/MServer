-- 解析协议文件并自动生成协议号

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
local function load_define(def_path)
    local f = io.open(def_path, "r")

    -- 文件不存在
    if not f then return {} end

    io.close(f)

    local env = {}

    -- require(def_path) -- require不能处理带../..这种相对路径，或者固定路径
    loadfile(def_path, "bt", env)()

    local id_hash = {}
    for _, m in pairs(env) do
        if type(m) == "table" then
            for _, mm in pairs(m) do
                if type(mm) == "table" and mm.i then
                    if id_hash[mm.i] then
                        mm.i = nil
                        print("id already exist, ignore", mm.i)
                    else
                        id_hash[mm.i] = true
                    end
                end
            end
        end
    end

    return env
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
    local w = string.match(line, "w%s*=%s*([%w_%.'\"]+)")

    -- 行尾注释
    local t = string.match(line, "%-%-%[%[(.*)%]%]")
    if not t then
        t = string.match(line, "%-%-%s*(.*)")
    end

    -- 查找上一行的注释
    local prev_t
    if ctx.index > 1 then
        local prev_line = ctx.lines[ctx.index - 1]
        prev_t = string.match(prev_line, "%-%-%s*(.*)")
        -- 如果上一行注释刚好就是被解析成了上一行的t，就不要用它
        -- 不过考虑到协议格式一般注释是在定义上面的，例如:
        -- -- 登录
        -- PlayerLogin = {
        if prev_t then
            -- 检查是否纯粹的 -- 注释行，而不是行尾注释
            if not string.match(prev_line, "^%s*%-%-") then
                prev_t = nil
            end
        end
    end

    -- 处理 s = "Chat.SChat" 放到独立行的情况
    if "s" ~= name and "c" ~= name and "i" ~= name and "w" ~= name and "t" ~= name then
        ctx.mm = name
        ctx.symbols[ctx.m][name] = {}
        if prev_t then
            ctx.symbols[ctx.m][name].prev_t = prev_t
            ctx.symbols[ctx.m][name].prev_t_line = ctx.index - 1
        end
        table.insert(ctx.sym_list, {
            m = ctx.m,
            mm = ctx.mm,
        })
    else
        local sym = ctx.symbols[ctx.m][ctx.mm]
        if s then sym.s = s end
        if c then sym.c = c end
        if t then sym.t = t end
        if w then sym.w = w end

        ctx.changes[ctx.index] = {
            m = ctx.m,
            mm = ctx.mm,
        }
    end

    return true
end

-- 解析协议定义
local function parse(file_path)
    local ctx = {
        index = 0,
        lines = {},
        symbols = {},
        changes = {},
        sym_list = {}
    }
    for line in io.lines(file_path) do table.insert(ctx.lines, line) end

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

local function main(in_file, out_path)
    local max_id = 0
    local id_hash = {}
    local old_define = load_define(in_file)
    for _, m in pairs(old_define) do
        if "table" == type(m) then
            for _, mm in pairs(m) do
                local i = mm.i
                if i then
                    if id_hash[i] then error("id already exist: " .. i) end
                    id_hash[i] = true
                    if i > max_id then max_id = i end
                end
            end
        end
    end
    print(string.format("reading from %s, max = %d", in_file, max_id))

    local ctx = parse(in_file)

    local count = 0
    for _, v in ipairs(ctx.sym_list) do
        local m = v.m
        local mm = v.mm
        count = count + 1

        local old_m = old_define[m] or {}
        local old_mm = old_m[mm] or {}
        if old_mm.i then
            ctx.symbols[m][mm].i = old_mm.i
        else
            max_id = max_id + 1
            ctx.symbols[m][mm].i = max_id
        end
    end

    local L = require "auto_lua"
    L.generate(ctx, out_path .. "/protocol.lua")

    -- 客户端协议
    -- local T = require "auto_lua"
    -- T.generate(ctx, out_path .. "/protocol.ts")

    print(string.format("writting to %s, %d symbols, max = %d",
        out_path, count, max_id))
end

-- ＠param in_file 需要解析的协议文件
-- ＠param out_path 输出的路径
local in_file, out_path = ...

print(_VERSION, ...)
main(in_file, out_path)
