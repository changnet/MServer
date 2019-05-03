-- mail_mgr.lua
-- 2018-05-20
-- xzc

-- 全服邮件 管理

local Time_id = require "modules.system.time_id"

local Mail_mgr = oo.singleton( nil,... )

function Mail_mgr:__init()
    self.list = {}

    -- 用这个time_id来做id，好处是不用存库，坏处是往后调时间的时候发邮件就会出错
    self.time_id = Time_id()
end

-- 发送个人邮件（离线在线均可）
-- @pid:玩家pid
-- @title:邮件标题
-- @ctx:邮件内容
-- @attachment:通用奖励格式，参考res.lua
-- @op:日志操作，用于跟踪附件资源产出。参考log_header
function Mail_mgr:send_mail( pid,title,ctx,attachment,op )
    -- 邮件数据统一在world处理，不是该进程则转
    if "world" ~= g_app.srvname then
        return g_rpc:invoke("rpc_send_mail",pid,title,ctx,attachment,op )
    end

    return self:raw_send_mail( pid,title,ctx,attachment,op )
end

function Mail_mgr:raw_send_mail( pid,title,ctx,attachment,op )
    local mail = {}
    mail.id    = self.time_id:next_id()
    mail.op    = op
    mail.ctx   = ctx
    mail.title = title
    mail.attachment = attachment

    -- 个人邮件不能同时发送超过65535
    -- 如果实在是在线人数太多,又不能发全服邮件，则考虑改一下time_id的位数，或者延迟几秒多次发
    if mail.id < 0 then
        ERROR("raw_send_mail:can NOT allocate mail id,%s",table.dump(mail))
        return
    end

    -- 检测pid是否有效
    if not g_player_mgr:is_pid_exist( pid ) then
        ERROR("raw_send_mail:pid not exist,%s",table.dump(mail))
        return
    end

    g_log_mgr:add_mail_log( pid,mail )

    local player = g_player_mgr:get_player( pid )
    if player then
        player:get_module("mail"):add_mail(mail)
        return
    end

    self:add_offline_mail( pid,mail )

    -- 正在登录中的玩家，标识一下需要重新加载邮件数据(他不一定能成功登录，可能不存库)
    -- 另一种是完全不在线,则由他登录时再加载邮件
    local raw_player = g_player_mgr:get_raw_player( pid )
    if raw_player then raw_player:get_module("mail"):set_reload() end
end

-- 添加个人离线邮件
function Mail_mgr:add_offline_mail( pid,mail )
    -- list.N，mongodb 2.2+版本后语法，表示list数组中第N个元素不存在时才插入(从0开始)
    -- 防止玩家太久不上线邮箱爆了
    local query = string.format(
        '{"_id":%d,"list.%d":{"$exists":false}}',pid,MAX_MAIL - 1 )
    local update = { ["$push"] = {["list"] = mail} }

    local cb = function( ecode,res )
        self:on_offline_mail( pid,mail,ecode,res )
    end

    -- 不能upsert,否则当邮箱满了的时候就会尝试插入新记录
    -- 新玩家时有初始化邮箱的
    return g_mongodb:find_and_modify(
        "mail",query,nil,update,{list = 0},false,false,false,cb )

    -- 使用update也可以，但是就没法知道返回值了
    -- g_mongodb:update( "mail",
    --     query,{ ["$push"] = {["list"] = mail} },true,false,cb )
end

-- 添加个个离线邮件结果
--[[
返回的值，不带list.如果因为邮箱满了，n的值为0
table: 0x2b8cff0
{
    "lastErrorObject" = table: 0x2bdde50
    {
        "n" = 1
        "updatedExisting" = true
    }
    "value" = table: 0x2c6d110
    {
        "_id" = 65537
    }
    "ok" = 1.0
}

]]
function Mail_mgr:on_offline_mail( pid,mail,ecode,res )
    if 0 == ecode and res.lastErrorObject.n == 1 then return end
    -- 如果插入失败，记录一下日志
    g_log_mgr:add_mail_log( string.format("offline_mail_error_%d",pid),mail )
end

-- 发送系统邮件
-- @title:邮件标题
-- @ctx:邮件内容

-- 以下均为可选参数
-- @attachment:通用奖励格式，参考res.lua
-- @op:操作日志，用于跟踪附件
-- @expire:超过这个时间戳此邮件失效(已发给玩家的不影响，只是这时候登录的玩家就不会收到了)
-- @level: >= 此等级的玩家才能收到
-- @vip:达到此vip等级才能收到
function Mail_mgr:send_sys_mail( title,ctx,attachment,op,expire,level,vip )
    if not title or not ctx then
        ERROR("send sys mail,no title(%s) or ctx(%s)",title,ctx)
        return
    end

    if "world" ~= g_app.srvname then
        return g_rpc:invoke(
            "rpc_send_sys_mail",title,ctx,attachment,op,expire,level,vip )
    end

    return self:raw_send_sys_mail( title,ctx,attachment,op,expire,level,vip )
end

function Mail_mgr:raw_send_sys_mail( title,ctx,attachment,op,expire,level,vip )
    local mail  = {}
    mail.id     = self.time_id:next_id()
    mail.op     = op
    mail.ctx    = ctx
    mail.vip    = vip
    mail.title  = title
    mail.level  = level
    mail.expire = expire
    mail.attachment = attachment

    if mail.id < 0 then
        ERROR("raw_send_sys_mail:can NOT allocate mail id,%s",table.dump(mail))
        return
    end

    table.insert( self.list,mail )
    g_log_mgr:add_mail_log("sys",mail)

    self:truncate()
    self:db_save() -- 系统邮件不多，直接存库
    self:dispatch_sys_mail( mail )
end

-- 把新增的全服邮件派发给在线的玩家
function Mail_mgr:dispatch_sys_mail( mail )
    local players = g_player_mgr:get_all_player()
    for _,player in pairs( players ) do
        player:get_module("mail"):add_sys_mail( mail )
    end
end

-- 存库
function Mail_mgr:db_save()
    local query = string.format('{"_id":%d}',g_app.srvindex)
    g_mongodb:update( "sys_mail",query,{ list = self.list },true )
end

-- 读库
function Mail_mgr:db_load()
    local callback = function( ... )
        self:on_db_loaded( ... )
    end

    local query = string.format('{"_id":%d}',g_app.srvindex)
    g_mongodb:find( "sys_mail",query,nil,callback )
end

-- db数据加载回调
function Mail_mgr:on_db_loaded( ecode,res )
    if 0 ~= ecode then
        ERROR( "sys mail db load error" )
        return
    end

    -- 新服没有全服邮件数据
    if res and res[1] then self.list = res[1].list end

    g_app:one_initialized( "sys_mail",1 )
end

-- 删除多出的邮件
function Mail_mgr:truncate()
    -- TODO:是不是要先找过期了的
    while #self.list > MAX_SYS_MAIL do
        local old_mail = self.list[1]
        table.remove( self.list,1 )

        g_log_mgr:del_mail_log( "sys",old_mail )
    end
end

-- 获取上一次使用的最大id
function Mail_mgr:get_now_id()
    return self.time_id:last_id()
end

-- 检查是否有新的全服邮件
function Mail_mgr:check_new_sys_mail( player,mail_box,sys_id )
    local new_cnt = 0

    -- 邮件是按时间倒序排列的
    for idx = #self.list,1,-1 do
        local mail = self.list[idx]
        if mail.id <= sys_id then break end

        if self:check_mail_limit( mail,palyer ) then
            new_cnt = new_cnt + 1
            mail_box:raw_add_sys_mail( mail )
        end
    end

    return new_cnt
end

-- 检查邮件条件
function Mail_mgr:check_mail_limit( mail,player )
    --  过期
    if mail.expire and ev:time() >= mail.expire then return false end
    -- 等级限制
    if mail.level and player:get_level() < mail.level then return false end

    return true
end

local mail_mgr = Mail_mgr()

return mail_mgr
