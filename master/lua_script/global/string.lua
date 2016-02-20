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
