-- time_id.lua
-- 2018-05-23
-- xzc

-- 以时间戳为因子的自增id
-- 高16位为计数，低32位为时间戳,共48位，可用double（最高52位整数）表示，每秒支持65535个id

local LIMIT = require "global.limits"

local Time_id = oo.class( nil,... )

function Time_id:__init()
    self.sec = ev:time()
    self.seed = 0
end

-- 获取当前的id
function Time_id:now_id()
    return ( (self.seed << 32) + self.sec )
end

-- 获取下一个自增Id
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

    -- 0-31为时间戳，32-47为计数
    -- TODO:2038年可以修改偏移的位数或者对now作一个偏移，不再从1970年计算
    return ( (self.seed << 32) + now )
end

-- 获取下一个自增Id
-- 如果当前已达上限，将占用未来时间，以处理瞬间创建大量id的场景
function Time_id:next_id_ex()
    local now = ev:time()
    if self.sec ~= now then
        self.seed = 0
        self.sec  = now
        return now
    end

    self.seed = self.seed + 1
    if self.seed >= LIMIT.UINT16_MAX then
        self.seed = 1
        self.sec = self.sec + 1
        PLOG( "Time_id:next_id over max,using next second" )
    end

    -- TODO:2038年可以修改偏移的位数或者对now作一个偏移，不再从1970年计算
    return ( (self.seed << 32) + now )
end

return Time_id
