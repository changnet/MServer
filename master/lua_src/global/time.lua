--游戏时间相关函数

local time = {}

local math = math
local ONE_MINUTE = 60
local ONE_HOUR = ONE_MINUTE * ONE_MINUTE
local ONE_DAY = ONE_HOUR * 24
local ONE_WEEK = ONE_DAY*7

--本地时间2000-01-01 00:00对应UTC
local sec_2000 = os.time{year=2000,month=1,day=1,hour=0,min=0,sec=0}
local refresh_2000 = sec_2000 + ONE_HOUR*4
local monday_2000 = os.time{year=2000,month=1,day=3,hour=0,min=0,sec=0} --第一个周一

-- Compute the difference in seconds between local time and UTC.
function time.get_timezone()
  local now = os.time()
  return os.difftime(now, os.time(os.date("!*t", now)))
end

local TIMEZONE = time.get_timezone()

--这是获取2000-01-01 00:00:00以来所过的天数，根据系统时区变化
function time.get_day_from_2000()
    return math.floor( (ev.time - sec_2000)/ONE_DAY )
end

--这是获取2000-01-01 00:00:00以来所过的秒数，根据系统时区变化
function time.get_second_from_2000()
    return ev.time - sec_2000
end

--这是获取2000-01-01 04:00:00以来所过的天数，根据系统时区变化
function time.get_refresh_day_from_2000()
    return math.ceil( (ev.time - refresh_2000)/ONE_DAY )
end

--这是获取2000-01-01 00:00:00以来所过的周数，根据系统时区变化
function time.get_week_from_2000()
    return math.ceil( (ev.time - monday_2000)/ONE_WEEK )
end

--获取当天的0时0分0秒的时间
function time.get_begin_of_day()
    local local_t = ev.time + TIMEZONE
    return local_t - local_t%ONE_DAY
end

--获取下分钟的时间
function time.get_next_minite()
    return ev.time - ev.time%ONE_MINUTE + ONE_MINUTE
end

--获取下小时的时间
function time.get_next_hour()
    return ev.time - ev.time%ONE_HOUR + ONE_HOUR
end

--获取下一次的4时0分0秒的时间
function time.get_next_refresh_time()
    local pass = (ev.time - refresh_2000)%ONE_DAY  --相对04:00的已过秒数

    return ev.time - pass + ONE_DAY
end

--获得星期几 1星期一
--TODO 这里星期天是7
function time.get_weekday()
    local sec = (ev.time - monday_2000)%ONE_WEEK
    if sec > 0 then
        return math.ceil( sec/ONE_DAY)
    end
    
    return 1  --周日00:00才会这样，算周一
end

--获取当天的秒数，范围是(0 <= sec < 24 * 3600)
function time.get_second_of_day()
    return (ev.time + TIMEZONE)%ONE_DAY
end

--比较a-b时间是否>=num天
function time.compare_days(a,b,num)
    local ds = math.floor((a - sec_2000)/ONE_DAY) - math.floor((b - sec_2000)/ONE_DAY)
    return ds >= num or false
end

return time