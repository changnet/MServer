-- 邮件内部通用接口
MailInternal = {}

local TimeId = require "idgen.time_id"

local assert = assert
local LOCAL_TYPE = LOCAL_TYPE

local this = memory("MailInternal")
if not this.time_id then
    this.time_id = TimeId()
end

-- 获取下一个邮件id
function MailInternal.next_id()
    return this.time_id:next_id()
end

-- 初始化一个邮件对象
-- @param mail_obj MailObj 邮件对象，包含以下字段
-- @return MailObj
function MailInternal.init(mail_obj)
    --[[
    注意，一定要在到达目的线程才可初始化id，以保证id的自增性
    这个id不是线程安全的，不同线程可能生成同样id。如果在id里加入addr，就不具备自增性了

    对于个人邮件，id唯一就行，是否自增就无所谓
    但系统邮件是要对比的，因此要保证id的自增性

    或者换种思路
        邮件id用basen.lua的算法生成，就是全局唯一
        而自增只用于系统邮件，在系统邮件里多维护一个seq_id即可

        问题就是邮件id变成字符串了，没有数字高效。但json安全的数字只有53位，没法构建线程安全的id
    ]]

    mail_obj.id = MailInternal.next_id()
    -- mail_obj.read = 0
    -- mail_obj.att_stat = 0

    return mail_obj
end

-- 记录邮件日志
-- @param pid number 玩家pid，如果是系统邮件则为0
-- @param mail_obj MailObj 邮件对象
-- @param op number 操作类型
function MailInternal.log(pid, op, mail_obj)
    -- 使用Log.misc把标题、内容、附件、配置id等字段记录到日志里，方便分析邮件相关问题
    local log_str = string.format("mail_id:%f, cid:%s, title:%s, text:%s",
        math.floor(mail_obj.id), tostring(mail_obj.cid),
        tostring(mail_obj.title), tostring(mail_obj.text))

    local att_str = ""
    if mail_obj.atts then
        for _, att in ipairs(mail_obj.atts) do
            att_str = att_str .. string.format("%d:%d|", att.type, att.val)
        end
    end

    Log.misc(pid, op, log_str, att_str)
end

-- 转发个人邮件到玩家
-- 如果玩家在线，转发到player线程；否则存入离线邮件
-- @param pid number
-- @param mail_obj MailObj
function MailInternal.forward_player_mail(pid, mail_obj)
    assert(LOCAL_TYPE == W.GAME)

    local player = PlayerMgr.get_player(pid)
    if not player then
        -- 不在线了，存离线
        MailOff.push(pid, mail_obj)
        return
    end

    -- 玩家在线，正常来讲发到对应的player线程玩家对象肯定是存在的
    -- 但考虑到集群部署，网络可能会断，必须得用Durable保证邮件不丢
    -- 但在宕机情况下，Durable虽然可以保证数据不丢，但player对象可能就不存在了
    -- 用PlayerDurable可以同时处理这两种情况
    PlayerDurable[GAME_ADDR].MailPlayer.receive(pid, mail_obj)
end
