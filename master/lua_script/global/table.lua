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

table.clear = function(t)
    local k, v
    while true do
        k, v = next(t, k)
        if k == nil then return end
        t[k] = nil
    end
end

table.empty = function(t)
    return _G.next(t) == nil
end

--[[
将一个table指定为数组
此函数与底层bson解析有关，非通用函数
]]
table.array = function(t)
    local _t = t or {}
    local mt = getmetatable( _t ) or {}
    rawset( mt,"__array",true )
    setmetatable( _t,mt )

    return _t
end
