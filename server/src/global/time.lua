-- time.lua
-- 2018-04-15
-- xzc

-- 游戏逻辑中常用时间相关函数
time = {}

local ONE_MINUTE = 60
local ONE_HOUR = ONE_MINUTE * ONE_MINUTE
local ONE_DAY = ONE_HOUR * 24
-- local ONE_WEEK = ONE_DAY*7

-- utc减去一个固定值，保证时间戳在32位以内（1735660800 = 2025-01-01）
local OFFSET = 1735660800

local engine_time = Engine.time

-- 获取游戏时间戳(utc时间戳-1577808000，保证在int32范围内)，单位是秒
function time.game_time()
    return engine_time() - OFFSET
end

-- Compute the difference in seconds between local time and UTC.
function time.get_timezone()
    local now = os.time()
    return os.difftime(now, os.time(os.date("!*t", now)))
end

local TIMEZONE = time.get_timezone()

-- 传入年月日时分秒，返回utc时间戳
function time.mktime(y, m, d, h, n, s)
    return os.time({year = y, month = m, day = d, hour = h, min = n, sec = s})
end

-- 传入utc时间，得到本地时间
--[[
返回格式：
table: 0x1eea750
{
    "wday" = 1           -- 星期几(1表示周日)
    "hour" = 15          -- 时
    "isdst" = false      -- 是否夏令时
    "sec" = 58           -- 秒
    "min" = 20           -- 分
    "day" = 15           -- 日
    "month" = 4          -- 月
    "yday" = 105         -- 当年已过天数
    "year" = 2018        -- 年
}
]]
function time.ctime(when)
    return os.date("*t", when or engine_time())
end

-- 输入UTC时间戳，返回本地时间字符串
-- time.date( 1558012266 ) 2019-06-16 21:11:06
function time.date(when, fmt)
    return os.date(fmt or "%Y-%m-%d %H:%M:%S", when or engine_time())
end

-- 获取当天的0时0分0秒的时间
function time.get_begin_of_day()
    local local_t = engine_time() + TIMEZONE
    return local_t - local_t % ONE_DAY
end

-- 获取下分钟的时间
function time.get_next_minite(when)
    if not when then when = engine_time() end

    -- 时区不影响分钟
    return when - when % ONE_MINUTE + ONE_MINUTE
end

-- 获取下小时的时间
function time.get_next_hour(when)
    if not when then when = engine_time() end

    -- 时区不影响小时
    return when - when % ONE_HOUR + ONE_HOUR
end

-- 获取当天已经过去的秒数，范围是(0 <= sec < 24 * 3600)
function time.get_second_of_day()
    return (engine_time() + TIMEZONE) % ONE_DAY
end

return time
