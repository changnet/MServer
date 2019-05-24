-- async_worker.lua
-- xzc
-- 2018-12-18

-- 执行异步逻辑

-- 1. 所有事件类型在header中定义
-- 2. 因涉及热更问题，不要直接直接对象的函数
--    考虑回调固定函数根据事件处理，或者wrap在一个函数中
-- 3. 根据对象删除其所有异步事件，比如玩家退出，怪物死亡
--    以对象为key做weak table，用一个int64按位来标识事件
-- 4. 延迟N毫秒执行
--    用timer可以做，但不划算，考虑在底层循环加一个回调，现在暂时不做
-- 5. 异步执行时再添加、修改异步数据循环修改数据


local ASYNC = require "modules.async_worker.async_worker_header"

local Async_worker = oo.singleton( ... )

function Async_worker:__init()
end

local worker = Async_worker()

return worker
