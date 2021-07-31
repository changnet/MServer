-- 一些常用的数学库

-- 36进制转换使用的字符
local CHARS = {
    "0","1","2","3","4","5","6","7","8","9",
    "a","b","c","d","e","f","g","h","i","j",
    "k","l","m","n","o","p","q","r","s","t",
    "u","v","w","x","y","z"
}

-- 36进制转换使用的数字
local DIGITS = {
    ["0"] = 0, ["1"] = 1, ["2"] = 2, ["3"] = 3, ["4"] = 4,
    ["5"] = 5, ["6"] = 6, ["7"] = 7, ["8"] = 8, ["9"] = 9,
    ["a"] = 10, ["b"] = 11, ["c"] = 12, ["d"] = 13, ["e"] = 14,
    ["f"] = 15, ["g"] = 16, ["h"] = 17, ["i"] = 18, ["j"] = 19,
    ["k"] = 20, ["l"] = 21, ["m"] = 22, ["n"] = 23, ["o"] = 24,
    ["p"] = 25, ["q"] = 26, ["r"] = 27, ["s"] = 28, ["t"] = 29,
    ["u"] = 30, ["v"] = 31, ["w"] = 32, ["x"] = 33, ["y"] = 34,
    ["z"] = 35
}

-- 四舍五入
-- @param decimal 保留的小数位数，可以是负数
function math.round(val, decimal)
    -- Note that math.ceil(num-.5) ~= math.floor(num+.5)
    -- e.g. for -.5 with math.ceil(num-.5) == -1 math.floor(num+.5) == 0
    if not decimal then
        return val >= 0 and math.floor(val + 0.5) or math.ceil(val - 0.5)
    end

    -- math.round(102.994, 2) = 102.994
    -- math.round(102.994, 0) = 102
    -- math.round(102.994, -1) = 100
    local mult = 10 ^ decimal
    local base = val * mult + 0.5

    return (val >= 0 and math.floor(base + 0.5) or math.ceil(base - 0.5)) / mult
end

-- 转换为任意进制
-- @param num 需要转化的number，只支持正整数
-- @param base 进制，如 2 16 36
-- @param chars 用于表示进制的字母数组，不传则使用默认
function math.to_base(num, base, chars)
    --[[
        1. 在游戏里，这个用途通常是用于生成邀请码、礼包码
        2. chars可以任意去掉一些容易混淆字符，比如0和o，它们的顺序也可以是任意顺序，这
           样看起来就更没什么规则
        3. 邀请码、礼包码要尽可能随机，这样才不容易被别人猜出来。
            这时候可以使用一个自增（比如32位），然后前面补8位随机数，后面补8位随机数
            变成了一个没有规则的48位数字，再转成36进制
    ]]

    assert(num >= 0) -- 暂时不支持负数

    local c = {}
    chars = chars or CHARS
    repeat
        table.insert(c, chars[num % base + 1])
        -- 这里不能用 math.floor(num / base)，因为lua中除法都会被转换为double来执行
        -- 当这个数非常大，超过double时，就会相差比较多，比如
        -- 7022597144829442788 / 36 = 195,072,142,911,928,966
        -- 但lua中得到的是 195,072,142,911,928,960
        num = num // base
    until num == 0

    -- TODO 如果标准的进制转换，这里还要把数组反转，但游戏里可能不需要这个
    return table.concat(c)
end

-- 从任意进制字符串转换为数字
function math.from_base(str, base, digits)
    -- TODO DIGITS用string.byte的值来做更高效，这里也不用分解字符串
    -- 只是就没那么直观了
    digits = digits or DIGITS

    -- 把字符串分解成单个字符
    -- TODO 如果to_base进行了反转，这里也是需要反转字符串的

    local num = 0
    for i = 1, string.len(str) do
        local c = string.sub(str, i, i)
        -- 当进行乘除法运算时，必须强制转成整形，否则lua默认是以double来运算的
        -- 如果最终值超过2^53次数，double就会表示不了
        num = num + digits[c] * math.tointeger(base ^ (i - 1))
    end

    return num
end
