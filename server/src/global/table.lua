-- table.lua
-- 2015-09-14
-- 增加部分常用table函数

local select = select

local function dump_insert(tbl, v, ...)
    -- table.insert本身就不支持insert一个nil值，所以这里只要是nil值就表示中止
    if v then
        if v ~= "" then tbl[#tbl + 1] = v end
        return dump_insert(tbl, ...)
    end
end

local function dump_key(k)
    if type(k) == "string" then return k end
    return "[" .. tostring(k) .. "]"
end

local function dump_value(val)
    if type(val) == "string" then return "\"" .. val .. "\"" end

    return tostring(val)
end

--- @param tbl 要导入的数据，一般为table
local function dump_table(ctx, tbl)
    -- 注意：这个函数格式化出来的日志，一定要是合法的lua代码
    -- 即打印一个变量时，如有需要，可以直接从日志中复制去重现问题

    local buffer = ctx.buffer

    local recursion = ctx.recursion
    if recursion[tbl] then
        return -- 重复递归
    end

    recursion[tbl] = true

    local indent = ctx.indent
    if indent then
        indent = indent .. ctx.indenter
        ctx.indent = indent
    else
        indent = ""
    end

    dump_insert(buffer, indent, ctx.table_beg)

    local val_end = ctx.val_end
    local key_end = ctx.key_end
    local max_depth = ctx.max_depth - 1
    ctx.max_depth = max_depth
    for k, v in pairs(tbl) do
        if type(v) ~= "table" or max_depth <= 1 then
            dump_insert(buffer, indent, dump_key(k), " = ", dump_value(v), val_end)
        else
            dump_insert(buffer, indent, dump_key(k), key_end)
            dump_table(ctx, v)
            dump_insert(buffer, val_end)
        end
    end
    dump_insert(buffer, indent, "}")
end


-- 导出table为字符串.
-- table中不能包括thread、function、userdata，table之间不能相互引用
-- @param pretty 是否格式化
function table.dump(tbl, pretty)
    if type(tbl) ~= "table" then
        return tostring(tbl)
    end

    local ctx = {
        buffer = {},
        recursion = {},
        max_depth = 128,
    }
    if pretty then
        ctx.indent = ""
        ctx.indenter = "    "
        ctx.val_end = ",\n"
        ctx.key_end = " =\n"
        ctx.table_beg = "{\n"
    else
        ctx.val_end = ","
        ctx.key_end = "="
        ctx.table_beg = "{"
    end

    dump_table(ctx, tbl)
    return table.concat(ctx.buffer)
end

-- 从字符串加载一个table.dump生成的字符串
function table.load(str)
    -- 5.3没有dostring之类的方法了
    -- load返回一个函数，"{a = 9}"这样的字符串是不合lua语法的
    local chunk_f, chunk_e = load("return " .. str)
    if not chunk_f then error(chunk_e) end

    return chunk_f()
end

-- 计算table的大小，不计算Key为nil的情况(如果使用了rawset,value可能为nil)
function table.size(t)
    local size = 0
    for _ in pairs(t) do size = size + 1 end

    return size
end

-- 清空一个table
function table.clear(t)
    for k in pairs(t) do t[k] = nil end
end

-- 判断一个table是否为空
-- @param t table，为nil时也返回true
function table.empty(t)
    return not t or next(t) == nil
end

-- 浅拷贝
function table.copy(t)
    local ct = {}

    for k, v in pairs(t) do ct[k] = v end

    return ct
end

-- 深拷贝
function table.deep_copy(t)
    if "table" ~= type(t) then return t end

    local ct = {}

    for k, v in pairs(t) do ct[table.deep_copy(k)] = table.deep_copy(v) end

    local mt = getmetatable(t)
    if mt then setmetatable(ct, t) end

    return ct
end

-- 把src中的内容合并到dest中
function table.merge(src, dest)
    local ct = dest or {}

    for k, v in pairs(src) do ct[k] = v end

    return ct
end

-- 将一个table指定为数组array或对象object
-- 这与该table的解析有关（如转换为json、mongodb存库、转换为bson）
-- @param tbl 需要设置的table
-- @param opt 设置数组、对象转换参数 0对象 1数组，其他值则按照稀松程度判断
-- @param sparse 稀松数组阀值
function table.set_array(tbl, opt, sparse)
    -- opt参数说明：
    -- 0对象 1数组，其他值则按照稀松程度判断（整数部分为最大key，小数部分为稀松程度）

    -- 如 opt = 8.6，当table中的key为整数且小于等于8并且 所有元素的数量/最大key>=0.6则为数组
    -- 即元素数量 >= 60% 算数组

    local mt = getmetatable(tbl)
    if not mt then
        mt = {}
        setmetatable(tbl, mt)
    end

    rawset(mt, "__opt", opt)

    return tbl
end

-- Knuth-Durstenfeld Shuffle 洗牌算法
function table.random_shuffle(list)
    local max_idx = #list
    for idx = max_idx, 1, -1 do
        local index = math.random(1, idx)
        local tmp = list[idx]
        list[idx] = list[index]
        list[index] = tmp
    end

    return list
end

-- 默认排序算法(降序，与table.sort一致)
-- 如果返回true，则a排在b后面
local function default_comp(a, b)
    return a > b
end

-- 稳定排序算法，应该没table.sort快
-- @param comp 对比函数，默认使用<
function table.sort_stable(list, comp)
    -- 冒泡排序
    local length = #list
    comp = comp or default_comp

    local ok
    for index = 1, length - 1 do
        ok = true

        for i = 1, length - index do
            local a, b = list[i], list[i + 1]
            if comp(b, a) then
                local tmp = a
                list[i] = b
                list[i + 1] = tmp

                ok = false
            end
        end

        if ok then break end
    end

    return list
end

-- 判断table中是否包含指定元素e
-- @param comp_func 用于判断元素是否相等的函数，默认使用 == 判断
function table.includes(tbl, e, comp_func)
    if comp_func then
        for _, v in pairs(tbl) do if comp_func(e, v) then return true end end
    else
        -- 没有提供对比函数，则认为e为基础类型
        for _, v in pairs(tbl) do if e == v then return true end end
    end

    return false
end

-- 反转数组
-- @param tbl 需要反转的数组
-- @param i 开始反转的索引，默认为1
-- @param j 结束反转的索引，默认为数组长度
function table.reverse(tbl, i, j)
    if not i then i = 1 end
    if not j then j = #tbl end

    while i < j do
        local tmp = tbl[i]
        if nil == tmp then return tbl end

        tbl[i] = tbl[j]
        tbl[j] = tmp

        i = i + 1
        j = j - 1
    end

    return tbl
end

-- 判断两个table的值是否一样（不判断meta table）
function table.same(t1, t2)
    if t1 == t2 then return true end

    local type1 = type(t1)
    if type1 ~= type(t2) or type1 ~= "table" then return false end

    for k1, v1 in pairs(t1) do
        local v2 = t2[k1]
        if nil == v2 or not table.same(v1, v2) then return false end
    end

    for k2 in pairs(t2) do
        if nil == t1[k2] then return false end
    end

    return true
end

-- 把一个table通过过元表设置为只读
function table.readonly(tbl)
    setmetatable(tbl, {
        __newindex = function() assert(false) end
    })
    return tbl
end

-- 把多个变量添加到table中，类似table.insert但支持多个变量
function table.append(t, ...)
    local args = {...}
    return table.move(args, 1, #args, #t + 1, t)
end

-- 从一个table中随机选择一个元素，table可以不为数组
function table.choice(tbl)
    local size = table.size(tbl)
    local index = math.random(size)
    local i = 1
    for _, v in pairs(tbl) do
        if i == index then return v end
        i = i + 1
    end
end

-- 传入多层key，获取table中对应的值
-- @param tbl 需要获取值的table
-- @param ... 多层key，如a,b,c表示获取tbl[a][b][c]的值
function table.get_value(tbl, ...)
    local len = select("#", ...)
    for i = 1, len do
        local key = select(i, ...)
        tbl = tbl[key]
    end
    return tbl
end

-- 传入多层key，设置table中对应的值
-- @param tbl 需要设置值的table
-- @param value 需要设置的值
-- @param ... 多层key，如a,b,c表示设置tbl[a][b][c] = value
function table.set_value(tbl, value, ...)
    local len = select("#", ...)
    for i = 1, len - 1 do
        local key = select(i, ...)
        if not tbl[key] then tbl[key] = {} end
        tbl = tbl[key]
    end
    local key = select(len, ...)
    tbl[key] = value
end

-- 获取所有key，作为一个数组返回
function table.keys(tbl)
    local list = {}
    for k in pairs(tbl) do table.insert(list, k) end

    return list
end

-- 和ipairs相反，逆序遍历一个table，该table必须是数组
function rpairs(tbl)
    --[[
    Lua的 for ... in 迭代协议
    1. 返回一个迭代器函数、一个状态变量和一个初始值
    2. 每次迭代调用迭代器函数，传入状态

    下面的例子中，第1次调用传入(tbl, #tbl + 1)，第2次调用传入(tbl, #tbl)
    ]]
    return function(t, i)
        i = i - 1
        local v = t[i]
        if v then return i, v end
    end, tbl, #tbl + 1
end

-- 按指定的key把一个数组构建成一个map
-- @param list 需要构建的数组
function table.to_map(list, key)
    local map = {}
    for _, v in ipairs(list) do
        local k = v[key]
        map[k] = v
    end
    return map
end

-- 把table转换成一个数组，忽略原有key
function table.to_array(tbl)
    local list = {}

    local idx = 1
    for _, v in pairs(tbl) do
        list[idx] = v
        idx = idx + 1
    end
    return list
end
