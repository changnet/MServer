-- 机器人进程入口文件(此文件不热更)

Startup.process_init(function()
    require "bot.bot_mgr"

    Rtti.collect()
    BotMgr.start()
end)
