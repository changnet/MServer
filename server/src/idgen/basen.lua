-- 自定义base62、base36算法
local BaseN = {}

--[[
这里的base算法不是为了压缩或者编码数据，而是为了生成一个具备字典序的字符串和标准的
base64算法不一样。

普通数字转成字符串是没有字符序的，比如2<10，而"2">"10"

数字编码成base62后，也不具备字典序，但前面加上长度后就具备了字典序
例如：
2 -> "12"，其中1表示长度，2是内容
10 -> "1A"，其中1表示长度，A是内容
"1A" > "12"和10>2是一样的

而int64转换为base62，长度最多是11，所以长度位占1个字符就够了，不会出现2字符长度破坏字典序

当数值>int64时，可以把高位和低位分开编码，两段字符串都是具备字符序，拼在一起也是有序的
]]

-- base62编码表 0-9 A-Z a-z(必须按ASCII升序)
local BASE62_CHARS = {
    "0","1","2","3","4","5","6","7","8","9",
    "A","B","C","D","E","F","G","H","I","J",
    "K","L","M","N","O","P","Q","R","S","T",
    "U","V","W","X","Y","Z",
    "a","b","c","d","e","f","g","h","i","j",
    "k","l","m","n","o","p","q","r","s","t",
    "u","v","w","x","y","z",
}

-- base36编码表 0-9 a-z
local BASE36_CHARS = {
    "0","1","2","3","4","5","6","7","8","9",
    "a","b","c","d","e","f","g","h","i","j",
    "k","l","m","n","o","p","q","r","s","t",
    "u","v","w","x","y","z",
}

-- shared buffer to avoid per-call allocations
local chars = {}
local BUF_END = 32

-- encode into shared `chars` from BUF_END backwards, return substring and length
local function encode_msb_buf(n, base_chars)
    local base = #base_chars
    local endpos = BUF_END
    local pos = endpos

    if n == 0 then
        chars[pos] = base_chars[1]
        return pos, 1
    end

    while n > 0 do
        local r = n % base
        chars[pos] = base_chars[r + 1]
        pos = pos - 1
        n = n // base
    end

    local start = pos + 1
    local len = endpos - pos
    return start, len
end

-- encode and include length head in buffer, return concatenated string and digit length
local function encode_msb_len(n, base_chars)
    local start, len = encode_msb_buf(n, base_chars)
    local head = base_chars[len + 1]
    chars[start - 1] = head
    return table.concat(chars, "", start - 1, BUF_END), len
end

-- 把两个整数编码成一个base62字符串，该字符串具备字典序
-- @param hi number 高位
-- @param lo number 低位，可以为nil
function BaseN.sequence62(hi, lo)
    local s_hi = encode_msb_len(hi, BASE62_CHARS)

    if not lo then
        return s_hi
    end

    local s_lo = encode_msb_len(lo, BASE62_CHARS)
    return s_hi .. s_lo
end

-- 把两个整数编码成一个base36字符串，该字符串具备字典序
-- @param hi number 高位
-- @param lo number 低位，可以为nil
function BaseN.sequence36(hi, lo)
    local s_hi = encode_msb_len(hi, BASE36_CHARS)

    if not lo then
        return s_hi
    end

    local s_lo = encode_msb_len(lo, BASE36_CHARS)
    return s_hi .. s_lo
end

return BaseN
