-- 飘字、弹窗提示
Alert = {
    -- 这些要按位定义，一个提示可能会同时显示到多个位置

    MSG = 0x01, -- 飘字提示
    WIN = 0x02, -- 弹窗提示
    ERR = 0x04, -- 错误提示，通常是测试环境后端把错误在前端弹出，方便测试
    SYS = 0x08, -- 系统聊天频道
    ANC = 0x10, -- 屏幕中间滚动公告（announcement）
}

local Pkt = {}
local DEF_MASK = Alert.MSG

-- 发送提示给单个玩家
--@param mask number 提示掩码，可按位组合，如：MSG|SYS
function Alert.send(player, msg, mask)
    if not mask then mask = DEF_MASK end

    Pkt.msg = msg
    Pkt.mask = mask
    NetMsg.send(player, MISC.ALERTMSG, Pkt)
end

-- 发送提示给单个玩家
--@param mask number 提示掩码，可按位组合，如：MSG|SYS
function Alert.isend(pid, msg, mask)
end

-- 广播提示给全服玩家
--@param mask number 提示掩码，可按位组合，如：MSG|SYS
function Alert.broadcast(msg, mask)
end
