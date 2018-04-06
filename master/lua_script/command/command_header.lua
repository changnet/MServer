-- 这里require各种子模块的cmd注册文件

-- 公用
require "command.system_cmd" -- 系统模块
require "modules.player.player_cmd" -- 玩家基础模块

-- 仅在gateway使用
if "gateway" == g_app.srvname then
end

-- 仅在world使用
if "world" == g_app.srvname then
    require "modules.chat.chat_cmd" -- 聊天
end
