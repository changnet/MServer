-- table.lua
-- 2015-09-14


-- 增加部分常用table函数

local next = next
local raw_table_dump -- 函数前置声明

-- 导出缩进，pretty = true时才生效
local function dump_table_indent( indent,dump_tbl )
    if indent <= 0 then return end

    -- TODO:这个感觉不用做缓存的吧，一来这个功能不常用，二是同一个字符串只分配一次内存
    for _ = 1,indent do table.insert( dump_tbl,"    " ) end
end

-- 导出换行，pretty = true时才生效
local function dump_table_break( indent,dump_tbl )
    if indent < 0 then return end

    return table.insert( dump_tbl,"\n" )
end

-- kv之间的分隔，比如"a = 9"有分隔，"a=9"无分隔
local function dump_table_seperator( indent,dump_tbl )
    if indent < 0 then return end

    return table.insert( dump_tbl," " )
end

-- 导出key
local function dump_table_key( key,dump_tbl )
    local key_type = type( key )

    if "string" == key_type then
        table.insert( dump_tbl,"['" )
        table.insert( dump_tbl,key  )
        table.insert( dump_tbl,"']" )
        return
    end

    if "number" == key_type then
        table.insert( dump_tbl,"[" )
        table.insert( dump_tbl,key  )
        table.insert( dump_tbl,"]" )
        return
    end

    if "boolean" == key_type then
        table.insert( dump_tbl,"[" )
        table.insert( dump_tbl,tostring(key)  )
        table.insert( dump_tbl,"]" )
        return
    end

    --table nil thread userdata 不能作key.否则写入文件后没法还原
    error( string.format("can NOT converte %s to string key",key_type) )
end

-- 导出value
local function dump_table_val( key,val,indent,dump_tbl,dump_recursion )
    local val_type = type( val )

    if "number" == val_type then
        dump_table_seperator( indent,dump_tbl )
        return table.insert( dump_tbl,val  )
    end

    if "boolean" == val_type then
        dump_table_seperator( indent,dump_tbl )
        -- table.concat不支持boolean类型的
        return table.insert( dump_tbl,tostring( val )  )
    end

    if "string" == val_type then
        dump_table_seperator( indent,dump_tbl )
        table.insert( dump_tbl,"'" )
        table.insert( dump_tbl,val  )
        table.insert( dump_tbl,"'" )
        return
    end

    if "table" == val_type then
        -- 不能循环引用，一是防止死循环，二是写入到文件后没法还原
        if dump_recursion[val] then
            error( string.format(
                "recursion reference table at key:%s",tostring(key)) )
        end

        dump_recursion[val] = true
        return raw_table_dump( val,indent,dump_tbl,dump_recursion )
    end

    -- function thread userdata 不能作value.否则写入文件后没法还原
    error( string.format("can NOT converte %s to string val",val_type) )
end

-- 导出逻辑
function raw_table_dump( tbl,indent,dump_tbl,dump_recursion )
    local next_indent = indent >= 0 and indent + 1 or -1 -- indent < 0 表示不缩进

    -- 子table和key之间是需要换行的
    if indent > 0 then dump_table_break( indent,dump_tbl ) end

    dump_table_indent( indent,dump_tbl )
    table.insert( dump_tbl,"{" )
    dump_table_break( indent,dump_tbl )

    local first = true
    for k,v in pairs( tbl ) do
        if not first then
            table.insert( dump_tbl,"," )
            dump_table_break( next_indent,dump_tbl )
        end

        first = false;
        dump_table_indent( next_indent,dump_tbl )
        dump_table_key( k,dump_tbl )
        dump_table_seperator( indent,dump_tbl )
        table.insert( dump_tbl,"=" )
        dump_table_val( k,v,next_indent,dump_tbl,dump_recursion )
    end

    dump_table_break( indent,dump_tbl )
    dump_table_indent( indent,dump_tbl )
    table.insert( dump_tbl,"}" )

    return dump_tbl
end

-- 导出table为字符串.
-- table中不能包括thread、function、userdata，table之间不能相互引用
-- @pretty:是否美化
function table.dump( tbl,pretty )
    local dump_tbl = {}
    local dump_recursive = {}

    local indent = pretty and 0 or -1
    raw_table_dump( tbl,indent,dump_tbl,dump_recursive )

    return table.concat( dump_tbl )
end

-- 从字符串加载一个table
function table.load( str )
    -- 5.3没有dostring之类的方法了
    -- load返回一个函数，"{a = 9}"这样的字符串是不合lua语法的
    local chunk_f,chunk_e = load( "return " .. str )
    if not chunk_f then error ( chunk_e ) end

    return chunk_f()
end

--[[
不计算Key为nil的情况
如果使用了rawset,value可能为nil
]]
function table.size(t)
    local ret = 0
    local k
    while true do
        k = next(t, k)
        if k == nil then break end
        ret = ret + 1
    end
    return ret
end

--[[
清空一个table
]]
function table.clear(t)
    local k
    while true do
        k = next(t, k)
        if k == nil then return end
        t[k] = nil
    end
end

--[[
判断一个table是否为空
]]
function table.empty(t)
    return next(t) == nil
end

--[[
浅拷贝,shallow_copy
]]
function table.copy(t,mt)
    local ct = {}

    for k,v in pairs(t) do ct[k] = v end

    if mt then
        setmetatable( ct,getmetatable(t) )
    end

    return ct
end

--[[
深拷贝
]]
function table.deep_copy(t,mt)
    if "table" ~= type(t) then return t end

    local ct = {}

    for k,v in pairs(t) do
        ct[ table.deep_copy(k) ] = table.deep_copy(v)
    end

    if mt then
        setmetatable( ct,table.deep_copy(getmetatable(t)) )
    end

    return ct
end

--[[
浅合并
]]
function table.merge(src,dest,mt)
    local ct = dest or {}

    for k,v in pairs(src) do
        ct[k] = v
    end

    if mt then
        setmetatable( ct,getmetatable(src) )
    end

    return ct
end

--[[
将一个table指定为数组
此函数与底层bson解析有关，非通用函数
]]
function table.set_array(t,sparse)
    local _t = t or {}
    local mt = getmetatable( _t ) or {}
    rawset( mt,"__array",true )
    if sparse then rawset( mt,"__sparse",true ) end

    setmetatable( _t,mt )
    return _t
end

-- Knuth-Durstenfeld Shuffle 洗牌算法
function table.random_shuffle( list )
    local max_idx = #list
    for idx = max_idx,1,-1 do
        local index = math.random( 1,idx )
        local tmp = list[idx]
        list[idx] = list[index]
        list[index] = tmp
    end

    return list
end

-- 合并任意变量
-- table.concat并不能合并带nil、false、true之类的变量
-- table.concat_any( "|","abc",nil,true,false,999 )
function table.concat_any( sep,... )
    local any = table.pack( ... )

    return table.concat_tbl( any,sep )
end

-- 这个tbl必须指定数组大小n，通常来自table.pack
function table.concat_tbl( tbl,sep )
    local max_idx = tbl.n
    for k = 1,max_idx do
        local v = tbl[k]
        if nil == v then
            tbl[k] = "nil"
        elseif true == v then
            tbl[k] = "true"
        elseif false == v then
            tbl[k] = "false"
        else
            local s_type = type(v)
            if "userdata" == s_type or "table" == s_type then
                tbl[k] = tostring(v)
            end
        end
    end

    return table.concat( tbl,sep )
end

-- 默认排序算法(升序，从小到大，与table.sort一致)
-- 如果返回true，则a排在b后面
local function default_comp(a, b)
    return a > b
end

-- 稳定排序算法，应该没table.sort快
-- @param comp 对比函数，默认使用<
function table.sort_ex(list, comp)
    -- 冒泡排序
    local length = #list
    comp = comp or default_comp

    local ok
    for index = 1, length - 1 do
        ok = true

        for i = 1, length - index do
            local a, b = list[i], list[i + 1]
            if comp(a, b) then
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
