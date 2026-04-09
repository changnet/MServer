-- mail.lua
-- 2018-05-20
-- xzc

-- 邮件模块
Mail = {
    T_NONE  = 0, -- 默认类型，一般是个人邮件
    T_SYS   = 1, -- 全服邮件
    T_GUILD = 2, -- 帮派邮件
}

local LOCAL_TYPE = LOCAL_TYPE

---@class MailObj 单个邮件结构
---@field id number 唯一id
---@field cid number 配置id（如果存在配置，title和text可能为nil，需要前端读配置）
---@field text string 邮件内容
---@field title string 标题
---@field atts table 邮件附件(attachments)，格式为通用奖励格式
---@field sender string 发件人
---@field time number 发送时间戳
---@field read number 是否已读
---@field att_stat number 是否已读
---@field type number 邮件类型，0个人邮件，1全服邮件，2帮派邮件
---@field tparam table 邮件类型相关参数，比如帮派邮件的帮派id
---@field op number 日志操作，用于跟踪附件资源产出。参考log_header
---@field log_str string 日志字符串，记录一些额外信息，方便日志分析

-- 发送个人邮件（支持离线）
-- 在game线程或player线程均可调用
-- @param pid number 玩家pid
-- @param mail_obj MailObj 邮件对象
function Mail.send_pid(pid, mail_obj)
    mail_obj = MailInternal.create(mail_obj)
    if LOCAL_TYPE == W.PLAYER then
        local player = PlayerMgr and PlayerMgr.get_player(pid)
        if player then
            MailPlayer.receive(player, mail_obj)
            return
        end
    end

    local addr = Router.find_worker_addr(W.GAME, "pid", pid)
    -- 路由到game线程处理（判断在线/离线）
    Durable[addr].MailInternal.forward_player_mail(pid, mail_obj)
end

--- 发送个人邮件（玩家已在当前player线程在线）
-- @param player Player 玩家对象
-- @param mail_obj MailObj 邮件对象
function Mail.send_player(player, mail_obj)
    -- 当前player对象不一定在player线程，还是以pid路由为主
    return Mail.send_pid(player.pid, mail_obj)
end

-- 发送全服邮件、帮派邮件（在game线程调用）
-- @param mail_obj MailObj 邮件对象
function Mail.send_sys(mail_obj)
    if not mail_obj.type then mail_obj.type = Mail.T_SYS end

    mail_obj = MailInternal.create(mail_obj)

    if LOCAL_TYPE == W.GAME then
        MailSys.send(mail_obj)
    else
        local addr
        local sid = mail_obj.sid
        if sid then
            -- 跨服，要先转到对应的服务器
            -- TODO 跨服发到所有服务器呢？
            addr = Router.find_worker_addr(W.GAME, "sid", sid)
        else
            addr = GAME_ADDR
        end
        Durable[addr].MailSys.send(mail_obj)
    end
end

return Mail
