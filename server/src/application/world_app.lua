-- 世界world服务启动入口

-- 这个文件只能执行一次，不能热更

local App = require "application.app"

-- app模块启动文件，用于热更
g_app.module_boot_file = "modules.module_header"

App.exec()
