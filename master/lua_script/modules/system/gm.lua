-- gm.lua
-- 2018-04-11
-- xzc

-- gm处理

local gm_map = {}
local GM = oo.singleton( nil,... )

-- 检测聊天中是否带gm
function GM:chat_gm( player,context )
    if not g_setting.gm or not string.start_with( context,"@" ) then
        return false
    end

    -- 分解gm指令(@level 999分解为{@level,999})
    local args = string.split(context," ")
    args[1] = string.sub( args[1],2 ) -- 去掉@

    self:exec( player,table.unpack( args ) )

    return true
end

-- gm指令运行入口（注意Player对象可能为nil，因为有可能从http接口调用gm）
function GM:exec( player,cmd,... )
    -- 优先查找注册过来的gm指令
    local gm_func = gm_map[cmd]
    if gm_func then
        return gm_func( player,... )
    end

    -- 写在gm模块本身的指令
    gm_func = self[cmd]
    if gm_func then
        return gm_func( self,player,... )
    end

    return PLOG( "try to call gm:%s,no such gm",cmd )
end

-- 注册gm指令
function GM:reg_cmd( cmd,gm_func )
    gm_map[cmd] = gm_func
end

function GM:hf()
    hot_fix()
end

function GM:hfs()
    hot_fix_script()
end

function GM:ghf()
    global_hot_fix()
end

local gm = GM()

return gm
