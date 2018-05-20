-- mail.lua
-- 2018-05-20
-- xzc

-- 玩家邮件

--[[
个人邮件结构
{
    id:唯一id
    ctx:邮件内容
    title:标题
    attachment = -- 邮件附件，格式为通用奖励格式，也可以是背包道具，可包含强化、星级等...
    {
        { res = 1,id = 10001,count = 1},
        { res = 1,id = 10001,count = 1},
    }
}
]]

local Module = require "modules.player.module"

local Mail = oo.class( Module,... )

function Mail:__init( pid,player )
    Module.__init( self,pid,player )
end


-- 数据存库接口，自动调用
-- 必须返回操作结果
function Mail:db_save()
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Mail:db_load( sync_db )
    return true
end

-- 数据加载完成自动调用，用于初始化从数据库加载的数据
-- 必须返回操作结果
function Mail:db_init()
    return true
end

-- 玩家数据已加载完成，进入场景
-- 必须返回操作结果
function Mail:on_login()
    return true
end

-- 玩家退出游戏
-- 必须返回操作结果，但这个结果不影响玩家数据存库
function Mail:on_logout()
    return true
end

return Mail
