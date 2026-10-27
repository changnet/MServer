-- postgresql数据读写worker入口文件

local addr = ...

local source = g_sharedata:get("source")
dofile(source .. "src/engine/startup.lua")

Startup.worker_init(tonumber(addr), "data.data_loader")

local PostgreSQL = require "postgresql.postgresql"

g_postgresql = PostgreSQL()
local conf = g_setting.postgresql

-- PG不需要thread_init
g_postgresql:connect_later(conf.ip, conf.port, conf.user, conf.pwd, conf.db)
