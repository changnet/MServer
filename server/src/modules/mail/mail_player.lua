-- 玩家邮件相关逻辑（player线程）
MailPlayer = {}

local new_pkt = {}
local MAX_MAIL_COUNT = 100

-- 获取玩家邮件存储（player线程本地，存缓存）
local function get_storage(player)
    local stg = Player.get_storage(player, "mail")
    if not stg then
        stg = {list = {}}
        Player.set_storage(player, "mail", stg)
    end
    return stg
end

-- 保存玩家邮件数据到缓存
local function save_player_mail(player)
    local stg = get_storage(player)
    Send[DATA_ADDR].DataCache.update("player_mail",
        {"pid", player.pid},
        {pid = player.pid, list = stg.list})
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
    local stg = get_storage(player)
    local list = stg.list
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
    local e, rows = Call[DATA_ADDR].DataCache.get(
        "player_mail", {"pid", player.pid})
    local stg = get_storage(player)
    if e == 0 and rows and #rows > 0 then
        stg.list = rows[1].list or {}
    end
end

-- 登录事件：加载邮件并同步离线/全服邮件
local function on_login(player)
    on_load_mail(player)

    local stg = get_storage(player)

    -- 下发邮件列表
    NetMsg.send(player, M.MailInfo, {mails = stg.list})
end

local function send_new(player, mail_obj)
    new_pkt.mail = mail_obj
    return NetMsg.send(player, M.MailNew, new_pkt)
end

-- 收到个人邮件（玩家在线时，由game线程转发来）
function MailPlayer.receive(player, mail_obj)
    local stg = get_storage(player)
    table.insert(stg.list, mail_obj)
    MailInternal.log(player.pid, LOG.ADD_MAIL, mail_obj)

    check_mail_limit(player)
    save_player_mail(player)

    send_new(player, mail_obj)
end

function MailPlayer.receive_list(player, list)
    local stg = get_storage(player)
    for _, mail_obj in ipairs(list) do
        table.insert(stg.list, mail_obj)
        MailInternal.log(player.pid, LOG.ADD_MAIL, mail_obj)

        send_new(player, mail_obj)
    end

    check_mail_limit(player)
    save_player_mail(player)
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
                MailPlayer.receive(player, m)
            end
        end
    end
end

-- 客户端读取邮件
local function c_mail_read(player, pkt)
    local stg = get_storage(player)
    local target_id = pkt.id
    for _, m in ipairs(stg.list) do
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
    local stg = get_storage(player)
    local target_ids = pkt.id
    local ids_map = {}
    for _, tid in ipairs(target_ids) do ids_map[tid] = true end

    local left = {}
    local changed = false
    for _, m in ipairs(stg.list) do
        if ids_map[m.id] then
            MailInternal.log(player.pid, LOG.DEL_MAIL, m)
            changed = true
        else
            table.insert(left, m)
        end
    end

    if changed then
        stg.list = left
        save_player_mail(player)
        NetMsg.send(player, M.MailDel, {id = target_ids})
    end
end

-- 客户端领取附件
local function c_mail_claim(player, pkt)
    local stg = get_storage(player)
    local target_id = pkt.id
    for _, m in ipairs(stg.list) do
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
    local stg = get_storage(player)
    local changed = false
    local claimed_ids = {}

    for _, m in ipairs(stg.list) do
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
    local stg = get_storage(player)
    local left = {}
    local del_ids = {}

    for _, m in ipairs(stg.list) do
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
        stg.list = left
        save_player_mail(player)
        NetMsg.send(player, M.MailDelRead, {ids = del_ids})
    end
end

Event.reg(EV.LOGIN, on_login)
NetMsg.reg(M.MailRead, c_mail_read)
NetMsg.reg(M.MailDel, c_mail_del)
NetMsg.reg(M.MailClaim, c_mail_claim)
NetMsg.reg(M.MailClaimAll, c_mail_claim_all)
NetMsg.reg(M.MailDelRead, c_mail_del_read)
