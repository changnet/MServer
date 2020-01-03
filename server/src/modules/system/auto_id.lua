-- auto_id.lua
-- 2018-02-26
-- xzc

-- auto increament id 一个不存库的自增id。如果达到最大值，则从1重复自增

local LIMIT = require "global.limits"

local AutoId = oo.class( ... )

function AutoId:__init()
    self.seed = 0
end

--[[
类型为int32，需要防止越界
@unique_map : 以id为key的table，用于获取唯一key.如果不需要可以不传值
]]
function AutoId:next_id( unique_map )
    repeat
        if self.seed >= LIMIT.INT32_MAX then self.seed = 0 end

        self.seed = self.seed + 1
    until (not unique_map or nil == unique_map[self.seed])

    return self.seed
end

return AutoId
