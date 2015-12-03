-- table.lua
-- 2015-09-14


-- 增加部分常用table函数

--[[
不计算Key为nil的情况
如果使用了rawset,value可能为nil
]]
table.size = function(o)
    local ret = 0
    local k,v
    while true do
        k, v = next(o, k)
        if k == nil then
            break
        end
        ret = ret + 1
    end
    return ret
end

table.clear = function(o)
    local k, v
    while true do
        k, v = next(o, k)
        if k == nil then return end
        o[k] = nil
    end
end

table.empty = function(t)
    return _G.next(t) == nil
end
