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
        {pid = player.pid, list = table.to_array(stg.list)})
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
    local list_map = stg.list
    local count = 0
    for _, _ in pairs(list_map) do count = count + 1 end
    if count <= MAX_MAIL_COUNT then return end

    local arr = {}
    for _, m in pairs(list_map) do table.insert(arr, m) end

    table.sort(arr, function(a, b)
        local wa = get_weight(a)
        local wb = get_weight(b)
        if wa ~= wb then return wa < wb end
        return a.time < b.time
    end)

    while #arr > MAX_MAIL_COUNT do
        local m = table.remove(arr, 1)
        list_map[m.id] = nil
        MailInternal.log(player.pid, LOG.DEL_MAIL, m)
    end
    save_player_mail(player)
end

-- 从缓存加载玩家邮件
local function on_load_mail(player)
    local e, row = Call[DATA_ADDR].DataCache.get(
        "player_mail", {"pid", player.pid})

    if 0 ~= e then
        perror(player, "load mail failed", e)
        return false
    end

    local list
    local row_list = row.list
    if row_list then
        list = table.to_map(row_list)
    else
        list = {} -- 新号
    end

    local stg = get_storage(player)
    stg.list = list

    return true
end

-- 登录事件：加载邮件并同步离线/全服邮件
local function on_login(player)
    local stg = get_storage(player)

    NetMsg.send(player, M.MailInfo, {mails = stg.list})
end

local function send_new(player, mail_obj)
    new_pkt.mail = mail_obj
    return NetMsg.send(player, M.MailNew, new_pkt)
end

-- 收到个人邮件（玩家在线时，由game线程转发来）
function MailPlayer.receive(player, mail_obj)
    MailInternal.init(mail_obj)

    local stg = get_storage(player)
    stg.list[mail_obj.id] = mail_obj
    MailInternal.log(player.pid, LOG.ADD_MAIL, mail_obj)

    check_mail_limit(player)
    save_player_mail(player)

    send_new(player, mail_obj)
end

function MailPlayer.receive_list(player, list)
    local stg = get_storage(player)
    for _, mail_obj in ipairs(list) do
        MailInternal.init(mail_obj)
        stg.list[mail_obj.id] = mail_obj
        MailInternal.log(player.pid, LOG.ADD_MAIL, mail_obj)

        send_new(player, mail_obj)
    end

    check_mail_limit(player)
    save_player_mail(player)
end

-- 客户端读取邮件
local function c_mail_read(player, pkt)
    local stg = get_storage(player)
    local target_id = pkt.id
    local m = stg.list[target_id]
    if m then
        if m.read == 0 then
            m.read = 1
            save_player_mail(player)
        end
        NetMsg.send(player, M.MailRead, {id = target_id})
        return
    end
end

-- 客户端删除邮件
local function c_mail_del(player, pkt)
    local stg = get_storage(player)
    local target_ids = pkt.id
    local changed = false
    for _, tid in ipairs(target_ids) do
        local m = stg.list[tid]
        if m then
            MailInternal.log(player.pid, LOG.DEL_MAIL, m)
            stg.list[tid] = nil
            changed = true
        end
    end

    if changed then
        save_player_mail(player)
        NetMsg.send(player, M.MailDel, {id = target_ids})
    end
end

-- 客户端领取附件
local function c_mail_claim(player, pkt)
    local stg = get_storage(player)
    local target_id = pkt.id
    local m = stg.list[target_id]
    if m then
        if m.atts and #m.atts > 0 and m.att_stat == 0 then
            m.att_stat = 1
            m.read = 1
            save_player_mail(player)

            Res.add(player, m.atts, LOG.MAILATTACH)
            MailInternal.log(player.pid, LOG.MAILATTACH, m)

            NetMsg.send(player, M.MailClaim, {id = target_id, atts = m.atts})
        end
        return
    end
end

-- 一键领取所有邮件附件
local function c_mail_claim_all(player, pkt)
    local stg = get_storage(player)
    local changed = false
    local claimed_ids = {}

    for id, m in pairs(stg.list) do
        if m.atts and #m.atts > 0 and m.att_stat == 0 then
            m.att_stat = 1
            m.read = 1
            Res.add(player, m.atts, LOG.MAILATTACH)
            MailInternal.log(player.pid, LOG.MAILATTACH, m)
            table.insert(claimed_ids, id)
            changed = true
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
    local del_ids = {}
    for id, m in pairs(stg.list) do
        local has_unclaimed = m.atts and #m.atts > 0 and m.att_stat == 0
        if m.read == 1 and not has_unclaimed then
            MailInternal.log(player.pid, LOG.DEL_MAIL, m)
            table.insert(del_ids, id)
        end
    end

    if #del_ids > 0 then
        for _, id in ipairs(del_ids) do stg.list[id] = nil end
        save_player_mail(player)
        NetMsg.send(player, M.MailDelRead, {ids = del_ids})
    end
end

Event.reg(EV.LOGIN, on_login)
Event.reg(EV.LOADING, on_load_mail)

NetMsg.reg(M.MailRead, c_mail_read)
NetMsg.reg(M.MailDel, c_mail_del)
NetMsg.reg(M.MailClaim, c_mail_claim)
NetMsg.reg(M.MailClaimAll, c_mail_claim_all)
NetMsg.reg(M.MailDelRead, c_mail_del_read)
