-- time.lua
-- 2018-04-15
-- xzc

-- 游戏逻辑中常用时间相关函数
-- 请使用ev:time()来获取时间戳，而不是os.time()，防止帧时间和实时时间对不上

local time = {}

local math = math
local ONE_MINUTE = 60
local ONE_HOUR = ONE_MINUTE * ONE_MINUTE
local ONE_DAY = ONE_HOUR * 24
local ONE_WEEK = ONE_DAY*7

-- 手动建立一些时间基准，本地时间2000-01-01 00:00对应UTC
local sec_2000 = os.time{year=2000,month=1,day=1,hour=0,min=0,sec=0}
local refresh_2000 = sec_2000 + ONE_HOUR*4
local monday_2000 = os.time{year=2000,month=1,day=3,hour=0,min=0,sec=0}

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


--这是获取2000-01-01 00:00:00以来所过的天数，根据系统时区变化
function time.get_day_from_2000()
    return math.floor( (ev:time() - sec_2000)/ONE_DAY )
end

--这是获取2000-01-01 00:00:00以来所过的秒数，根据系统时区变化
function time.get_second_from_2000()
    return ev:time() - sec_2000
end

--这是获取2000-01-01 04:00:00以来所过的天数，根据系统时区变化
function time.get_refresh_day_from_2000()
    return math.ceil( (ev:time() - refresh_2000)/ONE_DAY )
end

--这是获取2000-01-01 00:00:00以来所过的周数，根据系统时区变化
function time.get_week_from_2000()
    return math.ceil( (ev:time() - monday_2000)/ONE_WEEK )
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

--获取下一次的4时0分0秒的时间
function time.get_next_refresh_time()
    local pass = (ev:time() - refresh_2000)%ONE_DAY  --相对04:00的已过秒数

    return ev:time() - pass + ONE_DAY
end

--获得星期几 1星期一
--TODO 这里星期天是7
function time.get_weekday()
    local sec = (ev:time() - monday_2000)%ONE_WEEK
    if sec > 0 then
        return math.ceil( sec/ONE_DAY)
    end
    
    return 1  --周日00:00才会这样，算周一
end

--获取当天的秒数，范围是(0 <= sec < 24 * 3600)
function time.get_second_of_day()
    return (ev:time() + TIMEZONE)%ONE_DAY
end

--比较a-b时间是否>=num天
function time.compare_days(a,b,num)
    local ds = math.floor((a - sec_2000)/ONE_DAY) - math.floor((b - sec_2000)/ONE_DAY)
    return ds >= num
end

return time
