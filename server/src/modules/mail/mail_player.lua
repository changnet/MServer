-- 玩家邮件相关逻辑（player线程）
MailPlayer = {}

local MAX_MAIL_COUNT = 100

-- 获取玩家邮件存储（player线程本地，存缓存）
local function get_mail_sg(player)
    local sg = Player.get_storage(player, "mail")
    if not sg then
        sg = {list = {}}
        Player.set_storage(player, "mail", sg)
    end
    return sg
end

-- 保存玩家邮件数据到缓存
local function save_player_mail(player)
    local sg = get_mail_sg(player)
    Send[DATA_ADDR].DataCache.update("player_mail",
        {"pid", player.pid},
        {pid = player.pid, list = sg.list})
end

-- 邮件权重计算（越小越容易被删除）
local function get_weight(m)
    local w = 0
    if m.read == 0 then w = w + 100 end
    if m.atts and #m.atts > 0 and m.att_stat == 0 then
        w = w + 100
    end
    return w
end

-- 检查并清理超过最大值的邮件
local function check_mail_limit(player)
    local sg = get_mail_sg(player)
    local list = sg.list
    if #list <= MAX_MAIL_COUNT then return end

    table.sort(list, function(a, b)
        local wa = get_weight(a)
        local wb = get_weight(b)
        if wa ~= wb then return wa < wb end
        return a.time < b.time
    end)

    while #list > MAX_MAIL_COUNT do
        local m = table.remove(list, 1)
        MailInternal.log(player.pid, LOG.DEL_MAIL, m)
    end
    save_player_mail(player)
end

-- 从缓存加载玩家邮件
local function on_load_mail(player)
    local e, rows = Call[DATA_ADDR].DataCache.get("player_mail", {"pid", player.pid})
    local sg = get_mail_sg(player)
    if e == 0 and rows and #rows > 0 then
        sg.list = rows[1].list or {}
    end
end

-- 登录事件：加载邮件并同步离线/全服邮件
local function on_login(player)
    on_load_mail(player)

    local sg = get_mail_sg(player)
    local pid = player.pid

    -- RPC调用game线程，获取离线邮件+符合条件的全服邮件
    local e, new_list = Call[GAME_ADDR].MailInternal.get_login_mails(pid)

    if e == 0 and new_list and #new_list > 0 then
        for _, m in ipairs(new_list) do
            table.insert(sg.list, m)
            MailInternal.log(pid, LOG.ADD_MAIL, m)
        end
        check_mail_limit(player)
        save_player_mail(player)
    end

    -- 下发邮件列表
    NetMsg.send(player, M.MailInfo, {mails = sg.list})
end

-- 收到个人邮件（玩家在线时，由game线程转发来）
function MailPlayer.receive_mail(player, mail_obj)
    local sg = get_mail_sg(player)
    table.insert(sg.list, mail_obj)
    MailInternal.log(player.pid, LOG.ADD_MAIL, mail_obj)

    check_mail_limit(player)
    save_player_mail(player)

    NetMsg.send(player, M.MailNew, {mail = mail_obj})
end

-- game线程路由个人邮件时，如果玩家在线直接投递，否则存离线
-- @param pid number
-- @param mail_obj MailObj
function MailPlayer.receive_or_offline(pid, mail_obj)
    local player = PlayerMgr.get_player(pid)
    if player then
        MailPlayer.receive_mail(player, mail_obj)
    else
        -- 玩家不在本线程，通知game线程走离线
        Send[GAME_ADDR].MailInternal.push_offline(pid, mail_obj)
    end
end

-- 发送邮件（对外接口，player线程调用）
-- @param pid number
-- @param mail_obj MailObj
function MailPlayer.send(pid, mail_obj)
    -- 路由到game线程处理（game线程判断玩家在哪个player线程）
    Send[GAME_ADDR].MailInternal.send_pid(pid, mail_obj)
end

-- game线程广播全服邮件通知，player线程接收
-- 遍历本线程在线玩家，RPC拉取符合条件的全服邮件
function MailPlayer.on_sys_mail_notify(mail_obj)
    local players = PlayerMgr.get_all_player()
    for _, player in pairs(players) do
        local pid = player.pid
        local e, list = Call[GAME_ADDR].MailInternal.get_sys_mails_for(pid)
        if e == 0 and list and #list > 0 then
            for _, m in ipairs(list) do
                MailPlayer.receive_mail(player, m)
            end
        end
    end
end

-- 客户端读取邮件
local function c_mail_read(player, pkt)
    local sg = get_mail_sg(player)
    local target_id = pkt.id
    for _, m in ipairs(sg.list) do
        if m.id == target_id then
            if m.read == 0 then
                m.read = 1
                save_player_mail(player)
            end
            NetMsg.send(player, M.MailRead, {id = target_id})
            return
        end
    end
end

-- 客户端删除邮件
local function c_mail_del(player, pkt)
    local sg = get_mail_sg(player)
    local target_ids = pkt.id
    local ids_map = {}
    for _, tid in ipairs(target_ids) do ids_map[tid] = true end

    local left = {}
    local changed = false
    for _, m in ipairs(sg.list) do
        if ids_map[m.id] then
            MailInternal.log(player.pid, LOG.DEL_MAIL, m)
            changed = true
        else
            table.insert(left, m)
        end
    end

    if changed then
        sg.list = left
        save_player_mail(player)
        NetMsg.send(player, M.MailDel, {id = target_ids})
    end
end

-- 客户端领取附件
local function c_mail_claim(player, pkt)
    local sg = get_mail_sg(player)
    local target_id = pkt.id
    for _, m in ipairs(sg.list) do
        if m.id == target_id then
            if m.atts and #m.atts > 0 and m.att_stat == 0 then
                m.att_stat = 1
                m.read = 1
                save_player_mail(player)

                Res.add(player, m.atts, LOG.MAILATTACH)
                MailInternal.log(player.pid, LOG.MAILATTACH, m)

                -- 如果是全服邮件，通知game线程记录已领取
                if m.type == Mail.T_SYS or m.type == Mail.T_GUILD then
                    Send[GAME_ADDR].MailInternal.mark_sys_claimed(player.pid, m.id)
                end

                NetMsg.send(player, M.MailClaim,
                    {id = target_id, atts = m.atts})
            end
            return
        end
    end
end

-- 一键领取所有邮件附件
local function c_mail_claim_all(player, pkt)
    local sg = get_mail_sg(player)
    local changed = false
    local claimed_ids = {}

    for _, m in ipairs(sg.list) do
        if m.atts and #m.atts > 0 and m.att_stat == 0 then
            m.att_stat = 1
            m.read = 1
            Res.add(player, m.atts, LOG.MAILATTACH)
            MailInternal.log(player.pid, LOG.MAILATTACH, m)
            table.insert(claimed_ids, m.id)
            changed = true

            if m.type == Mail.T_SYS or m.type == Mail.T_GUILD then
                Send[GAME_ADDR].MailInternal.mark_sys_claimed(player.pid, m.id)
            end
        end
    end

    if changed then
        save_player_mail(player)
        NetMsg.send(player, M.MailClaimAll, {ids = claimed_ids})
    end
end

-- 一键删除所有已读邮件
local function c_mail_del_read(player, pkt)
    local sg = get_mail_sg(player)
    local left = {}
    local del_ids = {}

    for _, m in ipairs(sg.list) do
        -- 已读且无未领附件的才可删（有未领附件不删）
        local has_unclaimed = m.atts and #m.atts > 0
            and m.att_stat == 0
        if m.read == 1 and not has_unclaimed then
            MailInternal.log(player.pid, LOG.DEL_MAIL, m)
            table.insert(del_ids, m.id)
        else
            table.insert(left, m)
        end
    end

    if #del_ids > 0 then
        sg.list = left
        save_player_mail(player)
        NetMsg.send(player, M.MailDelRead, {ids = del_ids})
    end
end

PE.reg(PE_LOGIN, on_login)
NetMsg.reg(M.MailRead, c_mail_read)
NetMsg.reg(M.MailDel, c_mail_del)
NetMsg.reg(M.MailClaim, c_mail_claim)
NetMsg.reg(M.MailClaimAll, c_mail_claim_all)
NetMsg.reg(M.MailDelRead, c_mail_del_read)
