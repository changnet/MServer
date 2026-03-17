-- 通用的业务进程加载入口文件

local require = require_worker

require "engine.preloader"

require("message.pbc")

Pbc.load()

if g_setting.cli then
require("gm.cmd_line_interface", W.GAME)
end
