-- 一些常用的数学库

-- 四舍五入
-- @decimal 保留的小数位数，可以是负数
function math.round(val, decimal)
    -- Note that math.ceil(num-.5) ~= math.floor(num+.5)
    -- e.g. for -.5 with math.ceil(num-.5) == -1 math.floor(num+.5) == 0
    if not decimal then
        return val >= 0 and math.floor(val + 0.5) or math.ceil(val - 0.5)
    end

    -- math.round(102.994, 2) = 102.994
    -- math.round(102.994, 0) = 102
    -- math.round(102.994, -1) = 100
    local mult = 10^decimal
    local base = val * mult + 0.5

    return (val >= 0 and math.floor(base + 0.5) or math.ceil(base - 0.5)) / mult
end