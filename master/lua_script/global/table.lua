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
        k, v = next(t, k)
        if k == nil then
            break
        end
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
浅拷贝
]]
table.shallow_copy = function(t,mt)
    if "table" ~= type(t) then return t end

    local ct = {}

    for k,v in pairs(t) do
        ct[k] = v
    end

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
table.array = function(t,sparse)
    local _t = t or {}
    local mt = getmetatable( _t ) or {}
    rawset( mt,"__array",true )
    if sparse then rawset( mt,"__sparse",true ) end

    setmetatable( _t,mt )
    return _t
end
