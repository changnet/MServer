-- 机器人管理
BotMgr = {}

require("protocol.protocol")
local Bot = require "bot.bot"


-- 机器人不考虑热更，随便写
local bots = {}

local sc_cmd = {}

-- 注册指令处理
function BotMgr.reg(cmd, func)
    cmd.func = func
    sc_cmd[cmd.i] = cmd
end

function BotMgr.on_message(socket, msg_id, buffer, size)
    local c = sc_cmd[msg_id]

    if not c then
        -- android_cmd:dump_pkt( ... )
        -- eprint("android on cmd no handler found:%d", cmd)
        return
    end

    local pkt = Pbc.decode(c.s, buffer, size)

    c.func(socket.entity, pkt)
end

-- 逻辑循环
function BotMgr.routine()
    for _, bot in pairs(bots) do
        bot:routine()
    end
end

-- 开始机器人测试
function BotMgr.start()
    require("message.pbc")

    Pbc.load()
    Timer.interval(1000, 1000, -1, BotMgr.routine)

    -- 根据命令行参数，用不同策略启动机器人
    -- start.sh bot1 --bot=1 机器人单元测试，通常用于替换客户端调试功能
    -- start.sh bot1 --bot=2 批量机器人压测
    local b = g_env:get("--bot")
    if "1" == b then
        -- 单元调试使用ai策略1
        local bot = Bot(1 << 32 | LOCAL_ADDR, 1)
        table.insert(bots, bot)
    else
        warn("unknow bot mode:", b)
    end
end
