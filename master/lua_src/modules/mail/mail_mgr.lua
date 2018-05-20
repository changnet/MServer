-- mail_mgr.lua
-- 2018-05-20
-- xzc

-- 全服邮件 管理

local Mail_mgr = oo.singleton( nil,... )

function Mail_mgr:__init()
end

-- 发送个人邮件（离线在线均可）
-- @pid:玩家pid
-- @title:邮件标题
-- @ctx:邮件内容
-- @attachemnt:通用奖励格式，参考res.lua
function Mail_mgr:send_mail( pid,title,ctx,attachment )
end

-- 发送系统邮件
-- @title:邮件标题
-- @ctx:邮件内容
-- @attachemnt:通用奖励格式，参考res.lua

-- 以下均为可选参数
-- @time:超过这个时间戳此邮件失效(已发给玩家的不影响，只是这时候登录的玩家就不会收到了)
-- @level: >= 此等级的玩家才能收到 
-- @vip:达到此vip等级才能收到
function Mail_mgr:send_sys_mail( title,ctx,attachment,time,level,vip )
end

local mail_mgr = Mail_mgr()

return mail_mgr
