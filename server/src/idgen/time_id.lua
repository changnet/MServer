-- time_id.lua
-- 2018-05-23
-- xzc
-- 以时间戳为因子的自增id
-- 高16位为计数，低32位为时间戳,共48位，可用double（最高52位整数）表示，每秒支持65535个id
local TimeId = oo.class("TimeId")

local LIMIT = require "global.limits"

local max_seed = LIMIT.UINT16_MAX

function TimeId:__init()
    -- 用game_time，保证产生的时间戳在int32以内
    self.sec = time.game_time()
    self.seed = 0
end

-- 获取下一个自增Id
function TimeId:next_id()
    self.seed = self.seed + 1
    if self.seed >= max_seed then
        self.seed = 0
        self.sec = self.sec + 1
    end

    -- 0-31为时间戳，32-47为计数
    return ((self.seed << 32) + self.sec)
end

return TimeId
