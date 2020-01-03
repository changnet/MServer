-- time.lua
-- 2018-04-15
-- xzc

-- 游戏逻辑中常用时间相关函数
-- 请使用ev:time()来获取时间戳，而不是os.time()，防止帧时间和实时时间对不上

time = {}

local ONE_MINUTE = 60
local ONE_HOUR = ONE_MINUTE * ONE_MINUTE
local ONE_DAY = ONE_HOUR * 24
-- local ONE_WEEK = ONE_DAY*7


-- 手动建立一些时间基准，本地时间2000-01-01 00:00对应UTC
local sec_2000 = os.time{year=2000,month=1,day=1,hour=0,min=0,sec=0}

-- 获取短时间戳
-- 之前有项目组这样用，把标准UTC时间戳减去一个时间基准，以解决时间戳超出32bit的问题
-- 千万不要这样用，千万不要这样用。因为sec_2000这个值是用本地时间算的，不同时区是不固定的。
-- 管理后台尝试还原这个时间戳，服务器在韩国，但是后台可以在中国、韩国使用，导致后台的同事需要分
-- 开处理，而且，做全球同服的时候，或者客户端和服务器在不同时区的时候，就SB了
-- 因此，这个时间基准也应该是UTC的，固定一个值就好，不要用本地时间算
function time.get_mini_time()
    assert(false)
    return ev:time() - sec_2000
end

-- Compute the difference in seconds between local time and UTC.
function time.get_timezone()
  local now = os.time()
  return os.difftime(now, os.time(os.date("!*t", now)))
end

local TIMEZONE = time.get_timezone()

-- 传入年月日时分秒，返回utc时间戳
function time.mktime( y,m,d,h,n,s )
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
function time.ctime( when )
    return os.date( "*t", when or ev:time() )
end

-- 输入UTC时间戳，返回本地时间字符串
-- time.date( 1558012266 ) 2019-06-16 21:11:06
function time.date( when,fmt )
    return os.date( fmt or "%Y-%m-%d %H:%M:%S", when or ev:time() )
end


--获取当天的0时0分0秒的时间
function time.get_begin_of_day()
    local local_t = ev:time() + TIMEZONE
    return local_t - local_t%ONE_DAY
end

--获取下分钟的时间
function time.get_next_minite()
    return ev:time() - ev:time()%ONE_MINUTE + ONE_MINUTE
end

--获取下小时的时间
function time.get_next_hour()
    return ev:time() - ev:time()%ONE_HOUR + ONE_HOUR
end

--获取当天的秒数，范围是(0 <= sec < 24 * 3600)
function time.get_second_of_day()
    return (ev:time() + TIMEZONE)%ONE_DAY
end

return time
