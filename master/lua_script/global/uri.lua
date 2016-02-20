-- uri.lua
-- 2015-12=21
-- xzc

-- uri处理

--[[
https://www.ietf.org/rfc/rfc3986.txt

Reserved Characters:
reserved    = gen-delims / sub-delims
gen-delims  = ":" / "/" / "?" / "#" / "[" / "]" / "@"
sub-delims  = "!" / "$" / "&" / "'" / "(" / ")"
            / "*" / "+" / "," / ";" / "="

Unreserved Characters:
Characters that are allowed in a URI but do not have a reserved
purpose are called unreserved.  These include uppercase and lowercase
letters, decimal digits, hyphen, period, underscore, and tilde.

unreserved  = ALPHA / DIGIT / "-" / "." / "_" / "~"
]]

local Uri = {}

function Uri.encode( str )
    -- 空格为%20
    return string.gsub(s, "([^%w%.%-_~])", function(c) return string.format("%%%02X", string.byte(c)) end)

    -- 部分版本空格是+，请使用以下版本
    -- s = string.gsub(s, "([^%w%.%-_~ ])", function(c) return string.format("%%%02X", string.byte(c)) end)
    -- return string.gsub(s, " ", "+")
end

function Uri.decode( str )
    return string.gsub(s, '%%(%x%x)', function(h) return string.char(tonumber(h, 16)) end)
end
