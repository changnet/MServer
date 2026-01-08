-- 数据缓存及读写worker入口文件

local addr = ...

local source = g_env:get("source")
dofile(source .. "src/engine/startup.lua")

Startup.worker_init(tonumber(addr), "data.data_loader")

local MySQL = require "mysql.mysql"

g_mysql = MySQL()
local conf = g_setting.mysql

g_mysql:connect_later(conf.ip, conf.port, conf.user, conf.pwd, conf.db)
