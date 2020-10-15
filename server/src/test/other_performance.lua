-- other_performance.lua
-- xzc
-- 2018-11-17

-- 一些细小的代码测试及用例

local MaxHeap = require "util.max_heap"

local tbl = { 9,6,7,4,8,3,2,1,5,4,9 }

local mh = MaxHeap()

for idx,val in pairs( tbl ) do mh:push(val,idx) end


for idx = 1,#tbl do
    local object = mh:top()
    PRINT(object.key,object.value)
    mh:pop()
end
