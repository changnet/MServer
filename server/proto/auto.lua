-- 解析协议文件并自动生成协议号

-- ＠param path 需要解析的文件路径
-- ＠param s_path 生成的服务器协议文件路径(包含文件名)
-- ＠param c_type 成的客户端协议文件类型，支持 lua、typescript
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
    require(path)
    setmetatable(_G)

    return define
end

local function main()
    print(string.format("auto generating from %s", path))

    print(string.format("loading old define from %s", s_path))
    local old_define = load_define(s_path)

    print(string.format("writing new define to %s", s_path))
end

main()
