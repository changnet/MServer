-- mail_mgr.lua
-- 2018-05-20
-- xzc

-- 系统邮件 （包括帮派邮件，game线程处理）
MailSys = {}

local this = memory("MailSys")

-- 玩家领取全服邮件的记录，存库（{[pid] = last_claimed_id}）
local player_storage = storage("MailPlayer")

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
-- @param pid number 玩家pid
-- @param mail_obj MailObj 邮件对象
-- @return boolean
local function is_eligible(pid, mail_obj)
    local tp = mail_obj.type
    if tp == Mail.T_SYS then
        -- 全服邮件，所有玩家都符合条件
        return true
    elseif tp == Mail.T_GUILD then
        -- 帮派邮件，需要判断帮派id
        local tparam = mail_obj.tparam
        if not tparam then return false end
        -- tparam中包含guild_id，需要判断玩家是否在该帮派
        -- 这里假设可以通过全局函数查询，具体实现依赖帮派模块
        -- TODO: 替换为实际的帮派归属查询
        return false
    end
    return false
end

-- 获取符合条件的全服邮件（player线程登录/新邮件时调用）
-- 根据邮件类型+tparam过滤，排除已领取过的邮件
-- @param pid number 玩家pid
-- @return list table 未领取的符合条件的全服邮件列表
function MailSys.get_mails_for(pid)
    local list = this.list
    if not list then return {} end

    local last_id = player_storage[pid] or 0
    local result = {}

    for _, m in ipairs(list) do
        -- 根据id大小快速跳过已领取的邮件
        if m.id > last_id and is_eligible(pid, m) then
            local rm = table.clone(m)
            rm.read = 0
            rm.att_stat = 0
            table.insert(result, rm)
        end
    end
    return result
end

-- 记录玩家已领取某封全服邮件
-- 更新player_storage中玩家的最大已领取邮件id
-- @param pid number
-- @param mail_id number
function MailSys.mark_claimed(pid, mail_id)
    local last_id = player_storage[pid] or 0
    if mail_id > last_id then
        player_storage[pid] = mail_id
    end
end

-- 发送全服邮件
-- @param mail_obj MailObj 邮件对象（已create过）
function MailSys.send(mail_obj)
    local list = this.list
    table.insert(list, mail_obj)

    this.modify = true

    -- 广播到所有player线程，由player线程判断在线玩家是否符合条件
    Worker.send_other_type(W.PLAYER,
        MailInternal.on_sys_mail_notify, mail_obj)
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
        JsonFile.save("mail_sys", this.list)
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
