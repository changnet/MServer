-- gm.lua
-- 2018-04-11
-- xzc

-- gm处理

local gm_map = {}
local GM = oo.singleton( nil,... )

-- gm指令运行入口
function GM:exec( cmd,... )
    local gm_func = gm_map[cmd]
    if not gm_func then
        return PLOG( "try to call gm:%s,no such gm",cmd )
    end

    gm_func( ... )
end

-- 注册gm指令
function GM:reg_cmd( cmd,gm_func )
    gm_map[cmd] = gm_func
end

function GM:hot_fix()
    re_require()
end

local gm = GM()

gm:reg_cmd( "hf",GM.hot_fix )

return gm
