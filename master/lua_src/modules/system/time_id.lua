-- time_id.lua
-- 2018-05-23
-- xzc

-- 以时间戳为因子的自增id
-- 高16位为计数，低32位为时间戳,共48位，可用double（最高52位整数）表示，每秒支持65535个id

local LIMIT = require "global.limits"

local Time_id = oo.class( nil,... )

function Time_id:__init()
    self.sec = 0
    self.seed = 0
end

function Time_id:next_id()
    local now = ev:time()
    if self.sec ~= now then
        self.seed = 0
        self.sec  = now
        return now
    end

    self.seed = self.seed + 1
    if self.seed >= LIMIT.UINT16_MAX then
        ELOG( "Time_id:next_id over max" )
        return -1
    end

    -- TODO:2038年可以修改偏移的位数或者对now作一个偏移，不再从1970年计算
    return ( self.seed << 16 + now )
end

return Time_id
