-- mail_mgr.lua
-- 2018-05-20
-- xzc

-- 系统邮件 （包括帮派邮件，game线程处理）
MailSys = {}

local this = memory("MailSys")

local function load_mail()
    local e, rows = Call[DATA_ADDR].DataMgr.load("sys_mail", {})
    if e ~= 0 then
        eprint("load sys_mail error", e)
        return
    end

    local list
    local srv_list = {}
    local sid = Engine.get_server_id()

    -- {sid = 1, list = {mail_obj1, mail_obj2, ...}}
    -- sid和当前服务器一致的，为当前服务器的邮件
    -- 存在非当前服务器邮件的，则说明进行了合服

    for _, row in ipairs(rows) do
        local row_sid = row.sid
        if row_sid == sid then
            list = row.list
        else
            srv_list[row_sid] = row.list
        end
    end

    this.list = list or {}
    this.srv_list = srv_list

    return true
end

-- 判断玩家是否符合接收该系统邮件的条件
-- @param mail_obj MailObj 邮件对象
-- @return boolean
local function is_eligible(player, mail_obj)
    local tp = mail_obj.type
    if tp == Mail.T_SYS then
        -- 全服邮件，在发邮件前创号的玩家都符合条件
        return mail_obj.time <= player.create_time
    elseif tp == Mail.T_GUILD then
        -- 帮派邮件，需要判断帮派id
        return Player.get_property(player, PP.guild_id) == mail_obj.tparam
    end
    return false
end

-- 获取符合条件的全服邮件（player线程登录/新邮件时调用）
-- 根据邮件类型+tparam过滤，排除已领取过的邮件
function MailSys.fetch_player_mails(player, fetch_list)
    local list = this.list
    if not list then return end

    local stg = Player.get_misc_stg(player)
    local last_id = stg.mail_sys or 0

    local max_obj = list[#list]
    if max_obj then stg.mail_sys = max_obj.id end

    -- id是按顺序递增，逆序的话大部分情况下只会遍历少量邮件
    for _, m in rpairs(list) do
        if m.id <= last_id then return end

        if is_eligible(player, m) then
            if not fetch_list then fetch_list = {} end
            table.insert(fetch_list, m)
        end
    end
    return fetch_list
end

-- 发送全服邮件
-- @param mail_obj MailObj 邮件对象（已create过）
function MailSys.send(mail_obj)
    MailInternal.init(mail_obj)

    local list = this.list
    table.insert(list, mail_obj)

    this.modify = true
    MailInternal.log(0, LOG.ADD_SYS_MAIL, mail_obj)

    -- 通知所有在线玩家领取
    -- TODO 理论上可以只检测当前这封邮件是否能被领取，但fetch_player_mails检测所有邮件更稳
    local players = PlayerMgr.get_all_player()
    for _, player in pairs(players) do
        local fetch_list = MailSys.fetch_player_mails(player)
        if fetch_list then
            PlayerDurable[player.paddr].MailPlayer.receive_list(player.pid, fetch_list)
        end
    end
end

-- 起服初始化
local function on_startup(retry)
    if not retry then
        print("loading mail sys ...")
        return load_mail()
    else
        if this.list then return true end

        print("mail sys loading...")
        return false
    end
end

local function save_mail_sys()
    if not this.modify then return end

    local sid = Engine.get_server_id()
    local query_keys = {"sid", sid}
    local e = Call[DATA_ADDR].DataMgr.save("mail_sys", query_keys, {
        sid = sid,
        list = this.list,
    })
    if 0 ~= e then
        eprint("save mail sys error", e)
        Json.save("mail_sys", this.list)
    end

    -- Call调用里嵌套了协程，可能会不准，但仍能大概反馈到问题
    -- 或者直接用Send就准了
    local size = Rpc.last_codec_size()
    if size > DB_WARN_SIZE then
        eprint("mail sys too large", size)
    end

    this.modify = nil
    print("saving mail sys ...", size)
end

local function timer_save_mail_sys(now)
    local last = this.last
    if not last then
        this.last = now
        return
    elseif now - last < 30 * 60 then
        return
    end

    save_mail_sys()
    this.last = now
end

Startup.reg(on_startup)

Event.reg(EV.MIN_TIMER, timer_save_mail_sys)
Shutdown.reg({name = "mail_sys", func = save_mail_sys}, 65534)

return MailSys
