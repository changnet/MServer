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
]] local Module = require "modules.player.module"

local Mail = oo.class(..., Module)

function Mail:__init(pid, player)
    self.list = {}
    self.sys_id = 0

    self.modify = false
    self.db_query = string.format('{"_id":%d}', pid)
    Module.__init(self, pid, player)
end

-- 数据存库接口，自动调用
-- 必须返回操作结果
function Mail:db_save()
    if not self.modify then return true end

    -- 说明db里的数据比当前新，不能覆盖
    if self.reload then
        ERROR("db_save find reload flag,data can NOT be overwrote")
        return false
    end

    local info = {sys_id = self.sys_id, list = self.list}
    g_mongodb:update("mail", self.db_query, info, true)
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Mail:db_load(sync_db)
    local ecode, res = sync_db:find("mail", self.db_query)
    if 0 ~= ecode then return false end -- 出错

    if not res or not res[1] then return true end

    self.list = res[1].list
    -- 当玩家未创建mail时给他发了离线邮件就没有sys_id
    self.sys_id = res[1].sys_id or 0

    return true
end

-- 数据加载完成自动调用，用于初始化从数据库加载的数据
-- 必须返回操作结果
function Mail:on_init(is_new)
    local sys_id = g_mail_mgr:get_now_id()

    -- 新创建的号，直接跳过旧的全服邮件
    if is_new then
        self.sys_id = sys_id

        self.modify = true
        return self:db_save()
    end

    -- 检查是否有新全服邮件
    local new_cnt =
        g_mail_mgr:check_new_sys_mail(self.player, self, self.sys_id)
    if self.sys_id < sys_id then
        self.modify = true
        self.sys_id = sys_id
    end

    if new_cnt > 0 then self.modify = true end

    return true
end

-- 检查是否需要重新加载邮件
-- 防止正在登录的时候有人发了邮件
function Mail:check_reload()
    if not self.reload then return true end

    -- 不能直接改数据库又改了内存中的数据
    if self.modify then
        ERROR("mail reload data modify found")
        return false
    end

    -- 重新读库
    if not self:db_load(self.player:get_sync_db()) then return false end

    -- 重新初始化数据
    return self:on_init(self.payer:is_new())
end

-- 玩家数据已加载完成，进入场景
-- 必须返回操作结果
function Mail:on_login()
    if not self:check_reload() then return false end

    self:send_info() -- 发送邮件数据
    return true
end

-- 玩家退出游戏
-- 必须返回操作结果，但这个结果不影响玩家数据存库
function Mail:on_logout()
    return true
end

-- 由mail_mgr添加一封邮件
function Mail:add_mail(mail)
    self.modify = true
    table.insert(self.list, mail)
    self:send_new_mail(mail)

    self:truncate()
end

-- 增加一封系统邮件
function Mail:add_sys_mail(mail)
    -- id是根据时间来算的，调时间有可能出现问题
    if self.sys_id >= mail.id then
        ERROR("add_sys_mail sys id error:pid = %d,old = %d,now = %d", self.pid,
              self.sys_id, mail.id)
        return
    end
end

function Mail:raw_add_sys_mail(mail)
    if mail.id > self.sys_id then self.sys_id = mail.id end
    self:add_mail(mail)
end

-- 标识需要重新加载邮件(当你正在登录中，有收到新邮件)
function Mail:set_reload()
    self.reload = true
end

-- 删除多出的邮件
function Mail:truncate()
    -- TODO:策划应该会有删除要求，暂时不做
end

-- 发送邮件数据
function Mail:send_info()
    local pkt = {}
    pkt.mails = self.list

    self.player:send_pkt(MAIL.INFO, pkt)
end

-- 发送新邮件
function Mail:send_new_mail(new_mail)
    local pkt = {}
    pkt.mail = new_mail

    self.player:send_pkt(MAIL.NEW, pkt)
end

-- 能否删除邮件
function Mail:can_del(mail)
    return true
end

-- 处理邮件删除
function Mail:handle_mail_del(pkt)
    if not pkt.id or #pkt.id == 0 then
        return ERROR("handle_mail_del no mail id specify:%d", self.pid)
    end

    local mail_map = {}
    for _, id in pairs(pkt.id) do mail_map[id] = true end

    -- 先判断所有邮件是否都可以删除
    for _, mail in pairs(self.list) do
        if mail_map[mail.id] then
            if not self:can_del(mail) then
                return g_lang:send_tips(self.player, TIPS.CANOT_DEL_MAIL)
            end
        end
    end

    -- 统一进行删除操作
    for id in pairs(mail_map) do
        for idx, mail in pairs(self.list) do
            if mail.id == id then
                table.remove(self.list, idx)
                g_log_mgr:del_mail_log(self.pid, mail)
                break
            end
        end
    end

    self.player:send_pkt(MAIL.DEL, pkt)
end

return Mail
