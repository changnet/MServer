-- luacheck setting
-- https://luacheck.readthedocs.io/en/stable/config.html

std = "lua53"

max_line_length = 120 -- 行长，包括注释
max_code_line_length = 80 -- 代码行长，不包括注释
max_string_line_length = 120 -- 暂时没生效，后面再看看为啥

-- 只读全局变量,一般是C++导出到Lua的对象
read_globals =
{
    "ev",
    "network_mgr"
}

-- 可读可写全局变量
globals =
{
    -- 经过扩展的标准库，也需要添加到可写
    "table",
    "string",
    "time",
    "uri",

    -- 一些lua-tags不能导出的全局变量
    "g_app",
    "g_httpd",
    "g_account_mgr",
    "g_player_mgr",
    "g_map_mgr",
    "g_entity_mgr",
    "g_dungeon_mgr",

-- auto export by lua-tags 113 symbols


-- file:///e%3A/7yao/MServer/server/engine/acism.lua
"Acism",

-- file:///e%3A/7yao/MServer/server/engine/aoi.lua
"Aoi",

-- file:///e%3A/7yao/MServer/server/engine/lua_parson.lua
"lua_parson",

-- file:///e%3A/7yao/MServer/server/engine/lua_rapidxml.lua
"lua_rapidxml",

-- file:///e%3A/7yao/MServer/server/engine/statistic.lua
"statistic",

-- file:///e%3A/7yao/MServer/server/engine/util.lua
"util",

-- file:///e%3A/7yao/MServer/server/src/android/android_cmd.lua
"handshake_new",
"conn_new",
"command_new",
"conn_del",

-- file:///e%3A/7yao/MServer/server/src/android/app.lua
"g_timer_mgr",
"g_android_cmd",
"g_android_mgr",
"g_ai_mgr",
"sig_handler",
"application_ev",

-- file:///e%3A/7yao/MServer/server/src/application/application.lua
"sig_handler",

-- file:///e%3A/7yao/MServer/server/src/area/app.lua
"application_ev",

-- file:///e%3A/7yao/MServer/server/src/example/https_performance.lua
"http_listen",
"http_conn",

-- file:///e%3A/7yao/MServer/server/src/example/log_performance.lua
"g_log_mgr",

-- file:///e%3A/7yao/MServer/server/src/example/mongo_performance.lua
"g_mongodb_mgr",
"g_mongodb",

-- file:///e%3A/7yao/MServer/server/src/example/mysql_performance.lua
"g_mysql_mgr",

-- file:///e%3A/7yao/MServer/server/src/example/rank_performance.lua
"brank",

-- file:///e%3A/7yao/MServer/server/src/example/scene_performance.lua
"test_path",

-- file:///e%3A/7yao/MServer/server/src/example/websocket_performance.lua
"ws_listen",
"ws_local_conn",

-- file:///e%3A/7yao/MServer/server/src/global/global.lua
"__g_async_log",
"vd",
"__G__TRACKBACK",
"SYNC_PRINT",
"SYNC_PRINTF",
"PRINT",
"PRINTF",
"ERROR",
"ASSERT",
"f_tm_start",
"f_tm_stop",

-- file:///e%3A/7yao/MServer/server/src/global/name.lua
"__method",
"method_thunk",
"__reg_func",
"__func",

-- file:///e%3A/7yao/MServer/server/src/global/oo.lua
"oo",

-- file:///e%3A/7yao/MServer/server/src/global/require.lua
"require",
"no_update_require",
"unrequire",

-- file:///e%3A/7yao/MServer/server/src/global/require_conf.lua
"require_conf",
"require_kv_conf",

-- file:///e%3A/7yao/MServer/server/src/global/time.lua
"time",

-- file:///e%3A/7yao/MServer/server/src/http/http_header.lua
"HTTPE",

-- file:///e%3A/7yao/MServer/server/src/modules/ai/ai_header.lua
"AST",

-- file:///e%3A/7yao/MServer/server/src/modules/async_worker/async_worker_header.lua
"ASYNC",

-- file:///e%3A/7yao/MServer/server/src/modules/attribute/attribute_header.lua
"ABT",
"ABTSYS",

-- file:///e%3A/7yao/MServer/server/src/modules/command/command_mgr.lua
"SS",
"SC",
"CS",

-- file:///e%3A/7yao/MServer/server/src/modules/entity/entity_header.lua
"ET",

-- file:///e%3A/7yao/MServer/server/src/modules/event/event_header.lua
"PLAYER_EV",
"SYSTEM_EV",

-- file:///e%3A/7yao/MServer/server/src/modules/lang/lang_header.lua
"TIPS",

-- file:///e%3A/7yao/MServer/server/src/modules/log/log_header.lua
"LOG",

-- file:///e%3A/7yao/MServer/server/src/modules/module_header.lua
"E",
"g_setting",
"g_app_setting",
"g_log_mgr",
"g_unique_id",
"g_conn_mgr",
"g_timer_mgr",
"g_rpc",
"g_authorize",
"g_command_mgr",
"g_network_mgr",
"g_res",
"g_gm",
"g_mysql_mgr",
"g_player_ev",
"g_system_ev",
"g_lang",
"g_mail_mgr",
"g_ping",
"g_stat",

-- file:///e%3A/7yao/MServer/server/src/modules/move/move_header.lua
"MT",

-- file:///e%3A/7yao/MServer/server/src/modules/property/property.lua
"PROP",

-- file:///e%3A/7yao/MServer/server/src/modules/res/res_header.lua
"RES",

-- file:///e%3A/7yao/MServer/server/src/modules/system/define.lua
"SRV_NAME",
"SRV_KEY",
"LOGIN_KEY",
"MAX_MAIL",
"MAX_SYS_MAIL",
"SRV_ALIVE_INTERVAL",
"SRV_ALIVE_TIMES",
"PLATFORM",
"UNIQUEID",
"CLTCAST",

-- file:///e%3A/7yao/MServer/server/src/modules/system/hot_fix.lua
"hot_fix",
"hot_fix_script",

-- file:///e%3A/7yao/MServer/server/src/mongodb/mongodb_mgr.lua
"mongodb_read_event",

-- file:///e%3A/7yao/MServer/server/src/mysql/mysql_mgr.lua
"mysql_read_event",

-- file:///e%3A/7yao/MServer/server/src/network/conn_mgr.lua
"conn_accept",
"conn_new",
"conn_del",
"command_new",
"css_command_new",
"handshake_new",
"ctrl_new",

-- file:///e%3A/7yao/MServer/server/src/network/network_mgr.lua
"clt_multicast_new",

-- file:///e%3A/7yao/MServer/server/src/rpc/rpc.lua
"rpc_command_new",
"rpc_command_return",

-- file:///e%3A/7yao/MServer/server/src/test/app.lua
"json",
"sig_handler",

-- file:///e%3A/7yao/MServer/server/src/timer/timer_mgr.lua
"timer_event",

}

exclude_files =
{
    "engine/*", -- 用于代码实例的引擎接口文件，不用检查
    "protto/proto_default.lua", -- 示例文件，不用检查
}

ignore =
{
    "212", -- (W212)unused argument
    "311", -- (W311)value assigned to variable 'player' is unused: player = nil
}
