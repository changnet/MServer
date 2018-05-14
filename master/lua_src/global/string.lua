-- string.lua
-- 2015-12-03
-- xzc

-- string library extend function

--[[
string.split("2.0.0.2","%.")
string.split("2.0.0.2",".",true)
]]
string.split = function (str, delimiter,plain)
    local search_pos  = 1
    local sub_tab_len = 0
    local sub_str_tab = {}
    while true do
        local pos = string.find( str, delimiter,search_pos,plain )
        if not pos then
            local sub_str = string.sub( str, search_pos, -1 )
            table.insert( sub_str_tab,sub_tab_len + 1,sub_str )
            break
        end

        local sub_str = string.sub( str, search_pos, pos - 1 )
        table.insert( sub_str_tab,sub_tab_len + 1,sub_str )
        sub_tab_len = sub_tab_len + 1

        search_pos = pos + 1
    end

    return sub_str_tab
end

-- 判断字符串是否以某个字符串开头
string.start_with = function(str,ch)
    for idx = 1,string.len(ch) do
        if string.byte(ch,idx) ~= string.byte(str,idx) then return false end
    end

    return true
end

-- 判断字符串是否以某个字符串结尾
string.end_with = function(str,ch)
    for idx = 1,string.len(ch) do
        if string.byte(ch,-idx) ~= string.byte(str,-idx) then return false end
    end

    return true
end

-- 去除字符串左边空格
string.right_trim = function(str)
    local first_byte = string.byte(str,1)
    --ASCII码 space = 32 tab = 9
    if first_byte ~= 32 and first_byte ~= 9 then return str end

    -- ^表示字符串开始，只匹配开始的空格
    -- %s+表示一个或多个空格(包括tab)
    return string.gsub(str,"^%s*","")
end

-- 去除字符串右边空格
string.left_trim = function(str)
    local first_byte = string.byte(str,-1)
    --ASCII码 space = 32 tab = 9
    if first_byte ~= 32 and first_byte ~= 9 then return str end

    -- ^表示字符串开始，只匹配开始的空格
    -- %s+表示一个或多个空格(包括tab)
    return string.gsub(str,"%s*$","")
end
