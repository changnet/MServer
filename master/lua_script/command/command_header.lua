-- 这里require各种子模块的cmd注册文件
-- 不要在其他地方require非oo写法的文件，可能会造成热更时漏掉

-- 公用
require "command.system_cmd"
require "modules.player.player_cmd"

-- 仅在gateway使用
if "gateway" == Main.srvname then
end

-- 仅在world使用
if "world" == Main.srvname then
end