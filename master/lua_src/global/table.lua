-- table.lua
-- 2015-09-14


-- 增加部分常用table函数

--[[
不计算Key为nil的情况
如果使用了rawset,value可能为nil
]]
table.size = function(t)
    local ret = 0
    local k,v
    while true do
        k, v = _G.next(t, k)
        if k == nil then break end
        ret = ret + 1
    end
    return ret
end

--[[
清空一个table
]]
table.clear = function(t)
    local k, v
    while true do
        k, v = _G.next(t, k)
        if k == nil then return end
        t[k] = nil
    end
end

--[[
判断一个table是否为空
]]
table.empty = function(t)
    return _G.next(t) == nil
end

--[[
浅拷贝,shallow_copy
]]
table.copy = function(t,mt)
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
table.deep_copy = function(t,mt)
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
table.shallow_merge = function(src,dest,mt)
    local ct = dest or {}

    for k,v in pairs(t) do
        ct[k] = v
    end

    if mt then
        setmetatable( ct,getmetatable(t) )
    end

    return ct
end

--[[
将一个table指定为数组
此函数与底层bson解析有关，非通用函数
]]
table.set_array = function(t,sparse)
    local _t = t or {}
    local mt = getmetatable( _t ) or {}
    rawset( mt,"__array",true )
    if sparse then rawset( mt,"__sparse",true ) end

    setmetatable( _t,mt )
    return _t
end

-- Knuth-Durstenfeld Shuffle 洗牌算法
table.random_shuffle = function( list )
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
table.concat_any = function( sep,... )
    local any = table.pack( ... )

    return table.concat_tbl( any,sep )
end

-- 这个tbl必须指定数组大小n，通常来自table.pack
table.concat_tbl = function( tbl,sep )
    local max_idx = tbl.n
    for k = 1,max_idx do
        local v = tbl[k]
        if nil == v then
            tbl[k] = "nil"
        elseif true == v then
            tbl[k] = "true"
        elseif false == v then
            tbl[k] = "false"
        elseif "userdata" == type(v) then
            tbl[k] = tostring(v)
        end
    end

    return table.concat( tbl,sep )
end
