-- timer.lua
-- 2015-10-04
-- xzc

-- 定时器

local Timer = oo.class( nil,... )

-- 构造函数
function Timer:__init( cb,... )
    self.cb   = cb
    self.args = { ... }
end

-- 回调触发函数
function Timer:invoke()
    self.cb( table.unpack(self.args) )
end

return Timer
