-- mail_off.lua
-- 离线邮件管理（game线程）
-- 玩家不在线时收到的邮件存于off_mail表，不走缓存
-- 用local内存保存一份，避免重复读库

MailOff = {}

-- 推入一封离线邮件（玩家不在线时）
-- @param pid number
-- @param mail_obj MailObj
function MailOff.push(pid, mail_obj)
    local s = PlayerOff.get_modify(pid)
    local list = s.mail_off
    if not list then
        list = {}
        s.mail_off = list
    end
    table.insert(list, mail_obj)

    MailInternal.log(pid, LOG.ADD_OFF_MAIL, mail_obj)
end

local function fetch_off_mail(pid)
    local s = PlayerOff.get_storage(pid)
    local list = s.mail_off
    if not list then return end

    s.mail_off = nil
    PlayerOff.set_modify(pid)

    return list
end

-- game线程登录时调用，获取玩家离线邮件和符合条件的全服邮件
local function on_login(player)
    local pid = player.pid
    local fetch_list = fetch_off_mail(pid)

    fetch_list = MailSys.fetch_player_mails(player, fetch_list)
    if not fetch_list then return end

    PlayerDurable[player.paddr].MailPlayer.receive(pid, fetch_list)
end

Event.register(EV.LOGIN, on_login)

return MailOff
