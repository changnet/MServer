-- 机器人管理
BotMgr = {}


-- 机器人不考虑热更，随便写
local bots = {}
local all_cmd = {}

-- 注册指令处理
function BotMgr.reg(cmd, func)
    cmd.func = func
    all_cmd[cmd.i] = cmd
end

require("protocol.protocol")
local Bot = require "bot.bot"

function BotMgr.on_message(socket, msg_id, buffer, size)
    local c = all_cmd[msg_id]
    if not c then
        eprint("bot message not found", msg_id)
        return
    end

    local pkt = Pbc.decode(c.s, buffer, size)

    local entity = socket.entity
    if entity.msg_dbg then
        printf("%s(%s) message %d: %s",
            entity.pid, entity.name, msg_id, table.dump(pkt))
    end

    -- 未注册该消息回调
    local func = c.func
    if not func then return end

    func(socket.entity, pkt)
end

-- 逻辑循环
function BotMgr.routine()
    local ms_now = Engine.time_ms()
    for _, bot in pairs(bots) do
        bot:routine(ms_now)
    end
end

-- 开始机器人测试
function BotMgr.start()
    require("message.pbc")

    Pbc.load()
    Timer.interval(1000, 1000, -1, BotMgr.routine)

    -- 根据命令行参数，用不同策略启动机器人
    -- start.sh bot1 --run 1 机器人单元测试，通常用于替换客户端调试功能
    -- start.sh bot1 --run 2 批量机器人压测
    local r = g_sharedata:get("--run")
    if "1" == r then
        -- 单元调试使用ai策略1
        local bot = Bot(1 << 32 | LOCAL_ADDR, 1)
        table.insert(bots, bot)
        bot.msg_dbg = 1
    else
        warn("unknow bot mode:", r)
    end
end

local function init()
    for _, c in pairs(M) do
        all_cmd[c.i] = c
    end
end

init()
