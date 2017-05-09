-- hot_swap.lua 代理热更新http接口

local json = require "lua_parson"

local Hot_swap = {} -- 这个文件不要依赖oo.class，因为热更的时候可能会造成死循环

function Hot_swap:exec( fields,body )
    if not body or "" == body then
        return oo.hot_swap()
    end
    -- local tbl = json.decode( body )
    print( "hot swap exec ====>>>>>>>>>>>>>>>>",body )
    vd( tbl )
end

return Hot_swap