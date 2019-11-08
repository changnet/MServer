-- time_id.lua
-- 2018-05-23
-- xzc

-- 以时间戳为因子的自增id
-- 高16位为计数，低32位为时间戳,共48位，可用double（最高52位整数）表示，每秒支持65535个id

local LIMIT = require "global.limits"

local TimeId = oo.class( ... )

function TimeId:__init()
    -- TODO:2038年可以修改偏移的位数或者对now作一个偏移，不再从1970年计算
    -- 从初始化开始计时，自增满才增加时间因子，只要重启进程时没用使用到当前时间，就不会
    -- 产生重复id
    self.sec = ev:time()
    self.seed = 0
end

-- 获取上一次产生的id
function TimeId:last_id()
    return ( (self.seed << 32) + self.sec )
end

-- 获取下一个自增Id
function TimeId:next_id()
    self.seed = self.seed + 1
    if self.seed >= LIMIT.UINT16_MAX then
        self.seed = 0
        self.sec  = self.sec + 1
    end

    -- 0-31为时间戳，32-47为计数
    return ( (self.seed << 32) + self.sec )
end

return TimeId
