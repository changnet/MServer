-- 这里require各种子模块的cmd注册文件
-- 不要在其他地方require非oo写法的文件，可能会造成热更时漏掉

-- 这里require非oo写法的文件，要用require_ex，不然无法自动热更，要手动指定文件路径热更

-- 公用
require_ex "command.system_cmd" -- 系统模块
require_ex "modules.player.player_cmd" -- 玩家基础模块

-- 仅在gateway使用
if "gateway" == Main.srvname then
end

-- 仅在world使用
if "world" == Main.srvname then
    require_ex "modules.chat.chat_cmd" -- 聊天
end
