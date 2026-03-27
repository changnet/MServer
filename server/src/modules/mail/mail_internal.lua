-- 邮件内部通用接口
MailInternal = {}

local TimeId = require "modules.system.time_id"

local this = memory("MailInternal")
if not this.time_id then
    this.time_id = TimeId()
end

-- 获取下一个邮件id
function MailInternal.next_id()
    return this.time_id:next_id()
end

-- 创建一个邮件对象
-- @param mail_obj MailObj 邮件对象，包含以下字段
-- @return MailObj
function MailInternal.create(mail_obj)
    mail_obj.id = MailInternal.next_id()
    mail_obj.read = 0
    mail_obj.att_stat = 0
    mail_obj.time = mail_obj.time or time.game_time()

    return mail_obj
end

-- 记录邮件日志
-- @param pid number 玩家pid，如果是系统邮件则为0
-- @param mail_obj MailObj 邮件对象
-- @param op number 操作类型
function MailInternal.log(pid, op, mail_obj)
    -- 使用Log.misc把标题、内容、附件、配置id等字段记录到日志里，方便分析邮件相关问题
    local log_str = string.format("mail_id:%f, cid:%s, title:%s, text:%s",
        math.floor(mail_obj.id), tostring(mail_obj.cid), tostring(mail_obj.title), tostring(mail_obj.text))

    local att_str = ""
    if mail_obj.atts then
        for _, att in ipairs(mail_obj.atts) do
            att_str = att_str .. string.format("%d:%d|", att.type, att.val)
        end
    end

    Log.misc(pid, op, log_str, att_str)
end


-- 玩家登录时，获取离线邮件 + 符合条件的全服邮件
-- @param pid number 玩家pid
-- @return list table 邮件列表（合并后）
function MailInternal.get_login_mails(pid)
    local list = {}

    -- 1. 获取离线邮件（pop后自动清除内存和数据库记录）
    local off_list = MailOff.pop(pid)
    for _, m in ipairs(off_list) do
        table.insert(list, m)
    end

    -- 2. 获取符合条件的全服邮件
    local sys_list = MailSys.get_mails_for(pid)
    for _, m in ipairs(sys_list) do
        table.insert(list, m)
    end

    return list
end

-- 处理发送个人邮件（player线程RPC调用）
-- 如果玩家在线，转发到player线程；否则存入离线邮件
-- @param pid number
-- @param mail_obj MailObj
function MailInternal.send_pid(pid, mail_obj)
    -- 通过路由找到对应的player worker地址
    local addr = Router.find_worker_addr(W.PLAYER, "player", pid)
    if not addr then
        -- 没有找到worker，直接存离线
        MailOff.push(pid, mail_obj)
        return
    end
    -- 发到player线程，player线程判断玩家是否在线
    Send[addr].MailPlayer.receive_or_offline(pid, mail_obj)
end

-- player线程則管玩家不在本线程，请求game线程小署离线邮件
-- @param pid number
-- @param mail_obj MailObj
function MailInternal.push_offline(pid, mail_obj)
    MailOff.push(pid, mail_obj)
end

-- player线程通知game线程发送全服邮件
-- @param mail_obj MailObj
function MailInternal.send_sys(mail_obj)
    MailSys.send_sys(mail_obj)
end

-- player线程通知：全服邮件通知广播
-- game线程广播到所有player线程时会调用此函数（作为RPC接收端）
-- @param mail_obj MailObj
function MailInternal.on_sys_mail_notify(mail_obj)
    -- 这个函数运行在 player 线程
    -- 遍历本线程所有在线玩家，拉取符合条件的全服邮件
    local players = PlayerMgr.get_all_player()
    for _, player in pairs(players) do
        local pid = player.pid
        -- RPC Call game线程获取符合条件的邮件（仅sys mail部分）
        local e, list = Call[GAME_ADDR].MailInternal.get_sys_mails_for(pid)
        if e == 0 and list and #list > 0 then
            for _, m in ipairs(list) do
                MailPlayer.receive_mail(player, m)
            end
        end
    end
end

-- 获取符合条件的全服邮件（供player线程查询，仅sys mail）
-- @param pid number
-- @return list table
function MailInternal.get_sys_mails_for(pid)
    return MailSys.get_mails_for(pid)
end

-- player线程通知game线程：玩家已领取某封全服邮件的附件
-- @param pid number
-- @param mail_id number
function MailInternal.mark_sys_claimed(pid, mail_id)
    MailSys.mark_claimed(pid, mail_id)
end
