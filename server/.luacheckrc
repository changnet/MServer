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

    "require",
    "no_update_require",
    "__g_async_log",
    "sig_handler",
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
    "g_android_mgr",
    "unrequire",
    "hot_fix",
    "hot_fix_script",
    "application_ev",

    "conn_accept",
    "conn_new",
    "conn_del",
    "command_new",
    "css_command_new",
    "handshake_new",
    "ctrl_new",
    "clt_multicast_new",

    "__method",
    "method_thunk",
    "__reg_func",
    "__func",
    "AST",
    "E",
    "HTTPE",
    "SC",
    "CS",
    "SS",
    "vd",
    "oo",
    "g_app",
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
    "g_httpd",
    "g_account_mgr",
    "g_mongodb_mgr",
    "g_mongodb",
    "g_player_mgr",
    "g_map_mgr",
    "g_entity_mgr",
    "g_dungeon_mgr",
    "__G__TRACKBACK",
    "SYNC_PRINT",
    "SYNC_PRINTF",
    "PRINT",
    "PRINTF",
    "ERROR",
    "ASSERT",
    "f_tm_start",
    "f_tm_stop",
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
