-- string.lua
-- 2015-12-03
-- xzc

-- string library extend function

--[[
拆分字符串
@param str 需要拆分的字符串
@param delimeter 分隔符
@param plain 分隔符是否为启用正则，true为不启用

string.split("2.0.0.2","%.")
string.split("2.0.0.2",".",true)
]]
function string.split(str, delimiter, plain)
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

-- 检测字符串是否以某个字符串开头
-- @param str 需要检测的字符串
-- @param ch 开头字符串
function string.start_with(str, ch)
    for idx = 1,string.len(ch) do
        if string.byte(ch,idx) ~= string.byte(str,idx) then return false end
    end

    return true
end

-- 检测字符串是否以某个字符串结尾
-- @param str 需要检测的字符串
-- @param ch 结尾字符串
function string.end_with(str, ch)
    for idx = 1,string.len(ch) do
        if string.byte(ch,-idx) ~= string.byte(str,-idx) then return false end
    end

    return true
end

-- 去除字符串结尾空格
function string.right_trim(str)
    local first_byte = string.byte(str,1)
    --ASCII码 space = 32 tab = 9
    if first_byte ~= 32 and first_byte ~= 9 then return str end

    -- ^表示字符串开始，只匹配开始的空格
    -- %s+表示一个或多个空格(包括tab)
    return string.gsub(str,"^%s*","")
end

-- 去除字符串开头空格
function string.left_trim(str)
    local first_byte = string.byte(str,-1)
    --ASCII码 space = 32 tab = 9
    if first_byte ~= 32 and first_byte ~= 9 then return str end

    -- ^表示字符串开始，只匹配开始的空格
    -- %s+表示一个或多个空格(包括tab)
    return string.gsub(str,"%s*$","")
end

-- 去除字符串开头和结尾的空格
function string.trim(str)
    str = string.left_trim(str)
    return string.right_trim(str)
end

-- 替换第一个查找到的字符串
-- @param str 被替换的字符串
-- @param old_str 替换的字符
-- @param new_str 新的字符
function string.replace(str, old_str, new_str)
    -- 不要用gsub，因为sub_str可能包含正则字符
    local b, e = str:find(old_str, 1, true)
    if not b then
        return str
    end

    return str:sub(1, b - 1) .. new_str .. str:sub(e + 1)
end

-- 替换所有查找到的字符串
-- @param str 被替换的字符串
-- @param old_str 替换的字符
-- @param new_str 新的字符
function string.replace_all(str, old_str, new_str)
    -- 不要用gsub，因为sub_str可能包含正则字符

    -- TODO 当需要替换的字符太多时，这里字符串拆分、拼接效率会很差
    -- 如果有频繁调用的需求，需要做一个C++版本
    local offset = 1
    while true do
        local b, e = str:find(old_str, offset, true)
        if not b then
            return str
        end

        str = str:sub(1, b - 1) .. new_str .. str:sub(e + 1)
        offset = b + string.len(new_str)
    end
end

-- 根据字符串下标替换字符(i == j时可用于插入字符串)
-- @param str 被替换的字符串
-- @param sub_str 替换的字符串
-- @param i 替换开始的下标
-- @param 替换结束的下标，未指定则为sub_str的长度
function string.replace_at(str, sub_str, i, j)
    j = j or (i + string.len(sub_str))

    return str:sub(1, i - 1) .. sub_str .. str:sub(j, -1)
end

-- 把字符串中的正则字符进行转义
function string.gescape(str)
    return str:gsub('[%^%$%(%)%%%.%[%]%*%+%-%?]', '%%%1')
end
