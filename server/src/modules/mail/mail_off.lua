-- mail_off.lua
-- 离线邮件管理（game线程）
-- 玩家不在线时收到的邮件存于off_mail表，不走缓存
-- 用local内存保存一份，避免重复读库

MailOff = {}

local this = memory("MailOff") -- {[pid] = list}

-- 从数据库加载离线邮件，加载后删除数据库记录
-- @param pid number
-- @return list table 离线邮件列表
local function load_from_db(pid)
    local e, row = Call.DataMgr.load(DATA_ADDR, "off_mail", {"pid", pid})
    if e == 0 and row and row.list then
        -- 删除数据库记录
        Send.DataMgr.delete(DATA_ADDR, "off_mail", {"pid", pid})
        return row.list
    end
    return {}
end

-- 获取并清除玩家离线邮件（登录时调用）
-- 如果内存中已有记录，直接返回并清除；否则查数据库
-- @param pid number
-- @return list table 离线邮件列表
function MailOff.pop(pid)
    local list = this[pid]
    if list then
        this[pid] = nil
        return list
    end
    return load_from_db(pid)
end

-- 推入一封离线邮件（玩家不在线时）
-- @param pid number
-- @param mail_obj MailObj
function MailOff.push(pid, mail_obj)
    local list = this[pid]
    if not list then
        -- 先尝试从数据库加载，以防之前没加载过
        list = load_from_db(pid)
        this[pid] = list
    end
    table.insert(list, mail_obj)
    Send.DataMgr.save(DATA_ADDR, "off_mail", {"pid", pid},
        {pid = pid, list = list})
end

return MailOff
